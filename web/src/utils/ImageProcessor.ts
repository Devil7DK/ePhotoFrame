export async function processFile(
  file: File,
  dstW: number,
  dstH: number,
): Promise<Blob> {
  const img = await loadImage(file);
  const { sx, sy, sw, sh } = calculateCropFill(
    img.naturalWidth,
    img.naturalHeight,
    dstW,
    dstH,
  );

  const canvas = document.createElement("canvas");
  canvas.width = dstW;
  canvas.height = dstH;
  const ctx = canvas.getContext("2d");

  if (!ctx) {
    throw new Error("Failed to get canvas context");
  }

  ctx.imageSmoothingEnabled = true;
  ctx.imageSmoothingQuality = "high";
  ctx.drawImage(img, sx, sy, sw, sh, 0, 0, dstW, dstH);

  const imageData = ctx.getImageData(0, 0, dstW, dstH);
  return createLvglBin(imageData);
}

function loadImage(file: File): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const url = URL.createObjectURL(file);
    const img = new Image();
    img.onload = () => {
      URL.revokeObjectURL(url);
      resolve(img);
    };
    img.onerror = () => {
      URL.revokeObjectURL(url);
      reject(new Error("failed to decode"));
    };
    img.src = url;
  });
}

function calculateCropFill(
  srcW: number,
  srcH: number,
  dstW: number,
  dstH: number,
) {
  const srcRatio = srcW / srcH;
  const dstRatio = dstW / dstH;
  if (srcRatio > dstRatio) {
    const sh = srcH;
    const sw = sh * dstRatio;
    return { sx: (srcW - sw) / 2, sy: 0, sw, sh };
  } else {
    const sw = srcW;
    const sh = sw / dstRatio;
    return { sx: 0, sy: (srcH - sh) / 2, sw, sh };
  }
}

function createLvglBin(imageData: ImageData) {
  const { width, height, data } = imageData;
  const buf = new ArrayBuffer(12 + width * height * 2);
  const view = new DataView(buf);

  view.setUint8(0, 0x19); // magic
  view.setUint8(1, 0x12); // cf = LV_COLOR_FORMAT_RGB565
  view.setUint16(2, 0, true); // flags
  view.setUint16(4, width, true);
  view.setUint16(6, height, true);
  view.setUint16(8, width * 2, true); // stride
  view.setUint16(10, 0, true); // reserved

  let offset = 12;
  for (let i = 0; i < data.length; i += 4) {
    const r = data[i],
      g = data[i + 1],
      b = data[i + 2];
    const rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    view.setUint16(offset, rgb565, true);
    offset += 2;
  }

  return new Blob([buf], { type: "application/octet-stream" });
}

export async function renderBin(binBlob: Blob, canvas: HTMLCanvasElement) {
  const buf = await binBlob.arrayBuffer();
  if (buf.byteLength < 12) {
    throw new Error("file too small to be an LVGL bin");
  }
  const view = new DataView(buf);

  // LVGL header — written by createLvglBin above:
  //   byte 0: magic 0x19
  //   byte 1: color format 0x12 (LV_COLOR_FORMAT_RGB565)
  //   bytes 4-5: width, 6-7: height
  const magic = view.getUint8(0);
  const cf = view.getUint8(1);
  if (magic !== 0x19) {
    throw new Error(`bad magic byte 0x${magic.toString(16)} (expected 0x19)`);
  }
  if (cf !== 0x12) {
    throw new Error(`unsupported color format 0x${cf.toString(16)} (expected 0x12 RGB565)`);
  }

  const width = view.getUint16(4, true);
  const height = view.getUint16(6, true);
  const expected = 12 + width * height * 2;
  if (buf.byteLength < expected) {
    throw new Error(`truncated: ${buf.byteLength} bytes, expected at least ${expected} for ${width}x${height}`);
  }

  canvas.width = width;
  canvas.height = height;

  const ctx = canvas.getContext("2d");

  if (!ctx) {
    throw new Error("Failed to get canvas context");
  }

  const imageData = ctx.createImageData(width, height);

  let offset = 12;

  for (let i = 0; i < width * height; i++) {
    const rgb565 = view.getUint16(offset, true);
    offset += 2;

    // RGB565 -> RGB888
    const r = ((rgb565 >> 11) & 0x1f) << 3;
    const g = ((rgb565 >> 5) & 0x3f) << 2;
    const b = (rgb565 & 0x1f) << 3;

    const p = i * 4;

    imageData.data[p] = r;
    imageData.data[p + 1] = g;
    imageData.data[p + 2] = b;
    imageData.data[p + 3] = 255;
  }

  ctx.putImageData(imageData, 0, 0);
}
