import "./ManageView.css";

import type { SubmitEventHandler } from "preact";
import { useCallback, useEffect, useState } from "preact/hooks";

import { processFile } from "../../utils";
import { ConfirmDialog, LvglBinViewer } from "../../components";

type ImageEntry = { name: string; size: number };

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

export const ManageView = () => {
  const [images, setImages] = useState<ImageEntry[] | null>(null);
  const [listError, setListError] = useState<string | null>(null);
  const [actionMsg, setActionMsg] = useState<string | null>(null);
  const [uploadLog, setUploadLog] = useState<string[]>([]);
  const [uploading, setUploading] = useState(false);
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
      .then((cfg) => setAutoplaySec(String((cfg.autoplay_ms ?? 0) / 1000)))
      .catch((e) => setAutoplayMsg(`Failed to load: ${e}`));
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

  const log = useCallback((line: string) => {
    setUploadLog((prev) => [...prev, line]);
  }, []);

  const onUpload: SubmitEventHandler<HTMLFormElement> = useCallback(
    async (e) => {
      e.preventDefault();
      const form = e.currentTarget;
      const fd = new FormData(form);
      const files = fd.getAll("files").filter((f) => f instanceof File);
      if (files.length === 0) return;
      const orientation = fd.get("orient");
      const [dstW, dstH] = orientation === "portrait" ? [240, 320] : [320, 240];

      setUploading(true);
      setUploadLog([]);
      let ok = 0,
        fail = 0;
      for (const file of files) {
        const stem = file.name.replace(/\.[^.]+$/, "");
        const target = `${stem}.bin`;
        try {
          log(`Processing ${file.name}...`);
          const blob = await processFile(file, dstW, dstH);
          log(`  uploading ${target}...`);
          await uploadImage(target, blob);
          log(`  -> ${target}`);
          ok++;
        } catch (err) {
          log(`  x ${(err as Error).message}`);
          fail++;
        }
      }
      log(`Done. ${ok} succeeded${fail ? `, ${fail} failed` : ""}.`);
      setUploading(false);
      form.reset();
      await refresh();
    },
    [log, refresh],
  );

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
        <form onSubmit={onUpload}>
          <p>
            <label>
              Files:{" "}
              <input
                name="files"
                type="file"
                accept="image/*"
                multiple
                required
                disabled={uploading}
              />
            </label>
          </p>
          <p>
            Orientation:{" "}
            <label>
              <input
                name="orient"
                type="radio"
                value="landscape"
                checked
                disabled={uploading}
              />{" "}
              Landscape 320×240
            </label>
            <label>
              <input
                name="orient"
                type="radio"
                value="portrait"
                disabled={uploading}
              />{" "}
              Portrait 240×320
            </label>
          </p>
          <p>
            <button type="submit" disabled={uploading}>
              {uploading ? "Uploading…" : "Convert & upload"}
            </button>
          </p>
        </form>
        {uploadLog.length > 0 && <pre>{uploadLog.join("\n")}</pre>}
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
