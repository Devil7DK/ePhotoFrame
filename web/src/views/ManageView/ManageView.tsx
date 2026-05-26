import "./ManageView.css";

import type { FunctionalComponent, SubmitEventHandler } from "preact";
import { useCallback, useEffect, useRef, useState } from "preact/hooks";

import {
  calculateCropFill,
  loadImage,
  processFile,
  type CropRegion,
} from "../../utils";
import { ConfirmDialog, CropEditor, LvglBinViewer } from "../../components";

type ImageEntry = { name: string; size: number };
type Orientation = "landscape" | "portrait";
type QueueStatus = "pending" | "uploading" | "done" | "error";

type QueueItem = {
  id: string;
  file: File;
  sourceUrl: string;
  width: number;
  height: number;
  crop: CropRegion;
  status: QueueStatus;
  error?: string;
};

function orientationDims(o: Orientation): [number, number] {
  return o === "portrait" ? [240, 320] : [320, 240];
}

function formatDuration(seconds: number): string {
  if (!Number.isFinite(seconds) || seconds < 0) return "";
  if (seconds === 0) return "off";
  if (seconds < 60) return `${seconds} second${seconds === 1 ? "" : "s"}`;
  if (seconds < 3600) {
    const m = Math.floor(seconds / 60);
    const s = seconds % 60;
    if (s === 0) return `${m} minute${m === 1 ? "" : "s"}`;
    return `${m}m ${s}s`;
  }
  const h = Math.floor(seconds / 3600);
  const remMin = Math.floor((seconds % 3600) / 60);
  if (remMin === 0) return `${h} hour${h === 1 ? "" : "s"}`;
  return `${h}h ${remMin}m`;
}

async function fetchImages(): Promise<ImageEntry[]> {
  const r = await fetch("/api/images");
  if (!r.ok) throw new Error(`list failed: HTTP ${r.status}`);
  const data = await r.json();
  return data.images ?? [];
}

async function uploadImage(name: string, blob: Blob): Promise<void> {
  const r = await fetch(`/api/images?name=${encodeURIComponent(name)}`, {
    method: "POST",
    body: blob,
  });
  if (!r.ok) {
    const err = await r.json().catch(() => ({}));
    throw new Error(err.error ?? `upload failed: HTTP ${r.status}`);
  }
}

async function deleteImage(name: string): Promise<void> {
  const r = await fetch(`/api/images/${encodeURIComponent(name)}`, {
    method: "DELETE",
  });
  if (!r.ok) throw new Error(`delete failed: HTTP ${r.status}`);
}

async function postAction(path: string): Promise<void> {
  const r = await fetch(path, { method: "POST" });
  if (!r.ok) {
    const err = await r.json().catch(() => ({}));
    throw new Error(err.error ?? `HTTP ${r.status}`);
  }
}

async function fetchConfig(): Promise<{ autoplay_ms: number }> {
  const r = await fetch("/api/config");
  if (!r.ok) throw new Error(`config fetch failed: HTTP ${r.status}`);
  return r.json();
}

async function postAutoplay(ms: number): Promise<void> {
  const r = await fetch("/api/config/autoplay", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ autoplay_ms: ms }),
  });
  if (!r.ok) {
    const err = await r.json().catch(() => ({}));
    throw new Error(err.error ?? `HTTP ${r.status}`);
  }
}

const QueuePreview: FunctionalComponent<{
  item: QueueItem;
  orientation: Orientation;
}> = ({ item, orientation }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [dstW, dstH] = orientationDims(orientation);

  useEffect(() => {
    if (!canvasRef.current) return;
    const previewW = 128;
    const previewH = Math.round((dstH / dstW) * previewW);
    canvasRef.current.width = previewW;
    canvasRef.current.height = previewH;
    const ctx = canvasRef.current.getContext("2d");
    if (!ctx) return;
    const img = new Image();
    img.onload = () => {
      ctx.imageSmoothingEnabled = true;
      ctx.imageSmoothingQuality = "high";
      ctx.drawImage(
        img,
        item.crop.sx,
        item.crop.sy,
        item.crop.sw,
        item.crop.sh,
        0,
        0,
        previewW,
        previewH,
      );
    };
    img.src = item.sourceUrl;
    return () => {
      img.onload = null;
    };
  }, [
    item.sourceUrl,
    item.crop.sx,
    item.crop.sy,
    item.crop.sw,
    item.crop.sh,
    dstW,
    dstH,
  ]);

  return <canvas class="preview" ref={canvasRef} />;
};

const orientation: Orientation = "landscape";

export const ManageView = () => {
  const [images, setImages] = useState<ImageEntry[] | null>(null);
  const [listError, setListError] = useState<string | null>(null);
  const [actionMsg, setActionMsg] = useState<string | null>(null);
  const [busyAction, setBusyAction] = useState<
    null | "restart" | "wifi-reset" | "factory-reset"
  >(null);
  const [viewingImage, setViewingImage] = useState<string | null>(null);
  const [pendingConfirm, setPendingConfirm] = useState<{
    title: string;
    message: string;
    confirmLabel: string;
    confirmKind: "danger" | "warn";
    onConfirm: () => void;
  } | null>(null);

  const [queue, setQueue] = useState<QueueItem[]>([]);
  const [uploading, setUploading] = useState(false);
  const [dragOver, setDragOver] = useState(false);
  const [editingId, setEditingId] = useState<string | null>(null);
  const idRef = useRef(0);
  const queueRef = useRef<QueueItem[]>([]);

  const [autoplaySec, setAutoplaySec] = useState<string>("");
  const [autoplaySaving, setAutoplaySaving] = useState(false);
  const [autoplayMsg, setAutoplayMsg] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    setListError(null);
    try {
      setImages(await fetchImages());
    } catch (e) {
      setListError(String(e));
    }
  }, []);

  useEffect(() => {
    refresh();
  }, [refresh]);

  useEffect(() => {
    fetchConfig()
      .then((cfg) => setAutoplaySec(String((cfg.autoplay_ms ?? 3600000) / 1000)))
      .catch((e) => setAutoplayMsg(`Failed to load: ${e}`));
  }, []);

  useEffect(() => {
    queueRef.current = queue;
  }, [queue]);

  // Revoke every still-live blob URL on unmount.
  useEffect(() => {
    return () => {
      queueRef.current.forEach((it) => URL.revokeObjectURL(it.sourceUrl));
    };
  }, []);

  const onSaveAutoplay: SubmitEventHandler<HTMLFormElement> = useCallback(
    async (e) => {
      e.preventDefault();
      const seconds = Number(autoplaySec);
      if (!Number.isFinite(seconds) || seconds < 0) {
        setAutoplayMsg("Enter a non-negative number of seconds (0 = off).");
        return;
      }
      if (seconds > 0 && seconds < 10) {
        setAutoplayMsg("Interval must be 0 (off) or at least 10 seconds.");
        return;
      }
      if (seconds > 3600) {
        setAutoplayMsg("Interval must be at most 3600 seconds (1 hour).");
        return;
      }
      setAutoplaySaving(true);
      setAutoplayMsg(null);
      try {
        await postAutoplay(Math.round(seconds * 1000));
        setAutoplayMsg(
          seconds === 0 ? "Autoplay disabled." : `Autoplay set to ${seconds}s.`,
        );
      } catch (err) {
        setAutoplayMsg(String((err as Error).message ?? err));
      } finally {
        setAutoplaySaving(false);
      }
    },
    [autoplaySec],
  );

  const onDelete = useCallback(
    async (name: string) => {
      if (!confirm(`Delete ${name}?`)) return;
      try {
        await deleteImage(name);
        await refresh();
      } catch (e) {
        setListError(String(e));
      }
    },
    [refresh],
  );

  const addFiles = useCallback(
    async (files: FileList | File[]) => {
      const fileArr = Array.from(files).filter((f) =>
        f.type.startsWith("image/"),
      );
      if (fileArr.length === 0) return;
      const [dstW, dstH] = orientationDims(orientation);
      const newItems: QueueItem[] = [];
      for (const file of fileArr) {
        try {
          const img = await loadImage(file);
          newItems.push({
            id: String(++idRef.current),
            file,
            sourceUrl: URL.createObjectURL(file),
            width: img.naturalWidth,
            height: img.naturalHeight,
            crop: calculateCropFill(
              img.naturalWidth,
              img.naturalHeight,
              dstW,
              dstH,
            ),
            status: "pending",
          });
        } catch (e) {
          console.error("Failed to load", file.name, e);
        }
      }
      if (newItems.length > 0) {
        setQueue((prev) => [...prev, ...newItems]);
      }
    },
    [orientation],
  );

  const onFileInput = useCallback(
    (e: Event) => {
      const target = e.currentTarget as HTMLInputElement;
      if (target.files) addFiles(target.files);
      target.value = "";
    },
    [addFiles],
  );

  const onDrop = useCallback(
    (e: DragEvent) => {
      e.preventDefault();
      setDragOver(false);
      if (e.dataTransfer?.files) addFiles(e.dataTransfer.files);
    },
    [addFiles],
  );

  const onDragOver = useCallback((e: DragEvent) => {
    e.preventDefault();
    setDragOver(true);
  }, []);

  const onDragLeave = useCallback(() => {
    setDragOver(false);
  }, []);

  const onRemoveQueueItem = useCallback((id: string) => {
    setQueue((prev) => {
      const removed = prev.find((it) => it.id === id);
      if (removed) URL.revokeObjectURL(removed.sourceUrl);
      return prev.filter((it) => it.id !== id);
    });
  }, []);

  const onClearQueue = useCallback(() => {
    setQueue((prev) => {
      prev.forEach((it) => URL.revokeObjectURL(it.sourceUrl));
      return [];
    });
  }, []);

  const applyCropEdit = useCallback((id: string, crop: CropRegion) => {
    setQueue((prev) =>
      prev.map((it) =>
        it.id === id
          ? { ...it, crop, status: "pending", error: undefined }
          : it,
      ),
    );
    setEditingId(null);
  }, []);

  const onUploadAll = useCallback(async () => {
    if (uploading) return;
    const items = queue.filter(
      (it) => it.status === "pending" || it.status === "error",
    );
    if (items.length === 0) return;
    setUploading(true);
    const [dstW, dstH] = orientationDims(orientation);
    for (const item of items) {
      setQueue((prev) =>
        prev.map((it) =>
          it.id === item.id
            ? { ...it, status: "uploading", error: undefined }
            : it,
        ),
      );
      try {
        const stem = item.file.name.replace(/\.[^.]+$/, "");
        const target = `${stem}.bin`;
        const blob = await processFile(item.file, dstW, dstH, item.crop);
        await uploadImage(target, blob);
        setQueue((prev) =>
          prev.map((it) =>
            it.id === item.id ? { ...it, status: "done" } : it,
          ),
        );
      } catch (err) {
        const msg = (err as Error).message ?? String(err);
        setQueue((prev) =>
          prev.map((it) =>
            it.id === item.id ? { ...it, status: "error", error: msg } : it,
          ),
        );
      }
    }
    setUploading(false);
    await refresh();
  }, [queue, uploading, orientation, refresh]);

  const runAction = useCallback(
    async (
      kind: "restart" | "wifi-reset" | "factory-reset",
      path: string,
      successMsg: string,
    ) => {
      setBusyAction(kind);
      setActionMsg(null);
      try {
        await postAction(path);
        setActionMsg(successMsg);
      } catch (e) {
        setActionMsg(String(e));
        setBusyAction(null);
      }
    },
    [],
  );

  const doAction = useCallback(
    (
      kind: "restart" | "wifi-reset" | "factory-reset",
      path: string,
      confirmMsg: string,
      successMsg: string,
    ) => {
      if (kind === "restart") {
        runAction(kind, path, successMsg);
        return;
      }
      setPendingConfirm({
        title: kind === "wifi-reset" ? "Reset WiFi?" : "Factory reset?",
        message: confirmMsg,
        confirmLabel: "Reset",
        confirmKind: "danger",
        onConfirm: () => {
          setPendingConfirm(null);
          runAction(kind, path, successMsg);
        },
      });
    },
    [runAction],
  );

  if (busyAction) {
    return (
      <div class="status">
        <h2>Device restarting…</h2>
        <p>{actionMsg}</p>
        <p>You can close this page.</p>
      </div>
    );
  }

  const [dstW, dstH] = orientationDims(orientation);
  const pendingCount = queue.filter(
    (it) => it.status === "pending" || it.status === "error",
  ).length;

  return (
    <div class="manage-view">
      <h1>ePhotoFrame</h1>

      <section class="gallery">
        <header>
          <strong>Images on device ({images?.length ?? 0})</strong>
          <button type="button" onClick={refresh}>
            Refresh
          </button>
        </header>
        {listError && <p class="error">{listError}</p>}
        {!images && !listError && <p class="muted">Loading…</p>}
        {images && images.length === 0 && (
          <p class="muted">No images yet. Upload some below.</p>
        )}
        {images && images.length > 0 && (
          <ul>
            {images.map((img) => (
              <li key={img.name}>
                <span class="name">{img.name}</span>
                <span class="muted">{(img.size / 1024).toFixed(1)} KB</span>
                <button type="button" onClick={() => setViewingImage(img.name)}>
                  View
                </button>
                <button type="button" onClick={() => onDelete(img.name)}>
                  Delete
                </button>
              </li>
            ))}
          </ul>
        )}
      </section>

      <section class="upload">
        <h2>Upload images</h2>
        <p class="muted">
          Images are converted to LVGL RGB565 <code>.bin</code> in your browser
          before upload — the device only stores the bytes.
        </p>

        <label
          class={"drop-zone" + (dragOver ? " drag-over" : "")}
          onDragOver={onDragOver}
          onDragLeave={onDragLeave}
          onDrop={onDrop}
        >
          <input
            type="file"
            accept="image/*"
            multiple
            onChange={onFileInput}
            disabled={uploading}
            hidden
          />
          <strong>Drop images here or click to select</strong>
          <span class="muted">
            Cropped to {dstW}×{dstH} landscape. Use Edit on a card to adjust the
            crop area.
          </span>
        </label>

        {queue.length > 0 && (
          <>
            <ul class="queue">
              {queue.map((item) => (
                <li key={item.id} class={`queue-card status-${item.status}`}>
                  <QueuePreview item={item} orientation={orientation} />
                  <div class="meta">
                    <div class="name">{item.file.name}</div>
                    <div class="muted dims">
                      {item.width}×{item.height} → {dstW}×{dstH}
                    </div>
                    {item.status === "uploading" && (
                      <div class="muted">Uploading…</div>
                    )}
                    {item.status === "done" && <div class="ok">Uploaded ✓</div>}
                    {item.status === "error" && (
                      <div class="error" title={item.error}>
                        {item.error ?? "Failed"}
                      </div>
                    )}
                  </div>
                  <div class="card-actions">
                    <button
                      type="button"
                      onClick={() => setEditingId(item.id)}
                      disabled={uploading}
                    >
                      Edit
                    </button>
                    <button
                      type="button"
                      onClick={() => onRemoveQueueItem(item.id)}
                      disabled={uploading}
                    >
                      Remove
                    </button>
                  </div>
                </li>
              ))}
            </ul>

            <div class="queue-actions">
              <button type="button" onClick={onClearQueue} disabled={uploading}>
                Clear all
              </button>
              <button
                type="button"
                class="primary"
                onClick={onUploadAll}
                disabled={uploading || pendingCount === 0}
              >
                {uploading
                  ? "Uploading…"
                  : pendingCount === 0
                    ? "All uploaded"
                    : `Upload ${pendingCount}`}
              </button>
            </div>
          </>
        )}
      </section>

      <section class="autoplay">
        <h2>Autoplay</h2>
        <p class="muted">
          Auto-advance to the next photo after this many seconds. Minimum 10 s
          (lower values can advance before the previous image finishes loading
          from SD). Set to 0 to disable. Tapping prev/next on the device pauses
          autoplay for one full interval.
        </p>
        <form onSubmit={onSaveAutoplay}>
          <p>
            <label>
              Interval (seconds):{" "}
              <input
                type="number"
                min={0}
                max={3600}
                step={1}
                value={autoplaySec}
                onInput={(e) => setAutoplaySec(e.currentTarget.value)}
                disabled={autoplaySaving}
              />
            </label>
            {(() => {
              if (autoplaySec.trim() === "") return null;
              const pretty = formatDuration(Number(autoplaySec));
              return pretty ? (
                <span class="muted duration-hint">→ {pretty}</span>
              ) : null;
            })()}
          </p>
          <p>
            <button type="submit" disabled={autoplaySaving}>
              {autoplaySaving ? "Saving…" : "Save"}
            </button>
            {autoplayMsg && (
              <span
                class={`autoplay-status ${
                  autoplayMsg.startsWith("Autoplay") ? "muted" : "error"
                }`}
              >
                {autoplayMsg}
              </span>
            )}
          </p>
        </form>
      </section>

      <section class="actions">
        <h2>Device actions</h2>
        <p>
          <button
            type="button"
            onClick={() =>
              doAction(
                "restart",
                "/api/restart",
                "",
                "Restarting back to photo frame mode.",
              )
            }
          >
            Restart (back to photos)
          </button>
        </p>
        <p>
          <button
            type="button"
            class="warn"
            onClick={() =>
              doAction(
                "wifi-reset",
                "/api/wifi-reset",
                "Clear WiFi credentials and restart into setup mode?",
                "WiFi reset. Device is restarting into setup mode.",
              )
            }
          >
            Reset WiFi
          </button>
        </p>
        <p>
          <button
            type="button"
            class="danger"
            onClick={() =>
              doAction(
                "factory-reset",
                "/api/factory-reset",
                "Wipe all device settings and restart? Images on the SD card are NOT removed.",
                "Factory reset. Device is restarting into setup mode.",
              )
            }
          >
            Factory reset
          </button>
        </p>
      </section>

      <LvglBinViewer
        fileName={viewingImage ?? ""}
        onClose={() => setViewingImage(null)}
      />

      {editingId !== null &&
        (() => {
          const item = queue.find((it) => it.id === editingId);
          if (!item) return null;
          return (
            <CropEditor
              file={item.file}
              aspectRatio={dstW / dstH}
              initialCrop={item.crop}
              onConfirm={(crop) => applyCropEdit(item.id, crop)}
              onCancel={() => setEditingId(null)}
            />
          );
        })()}

      {pendingConfirm && (
        <ConfirmDialog
          title={pendingConfirm.title}
          message={pendingConfirm.message}
          confirmLabel={pendingConfirm.confirmLabel}
          confirmKind={pendingConfirm.confirmKind}
          onConfirm={pendingConfirm.onConfirm}
          onCancel={() => setPendingConfirm(null)}
        />
      )}
    </div>
  );
};
