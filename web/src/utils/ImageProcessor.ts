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
