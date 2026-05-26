import type { FunctionalComponent } from "preact";
import { renderBin } from "../../utils";
import "./LvglBinViewer.css";

import { useEffect, useRef, useState } from "preact/hooks";
import { LoadingOverlay } from "../LoadingOverlay/LoadingOverlay";

interface ILvglBinViewerProps {
  fileName: string;
  onClose: () => void;
}

export const LvglBinViewer: FunctionalComponent<ILvglBinViewerProps> = ({
  fileName,
  onClose,
}) => {
  const [loading, setLoading] = useState(false);
  const [binBlob, setBinBlob] = useState<Blob | null>(null);
  const [error, setError] = useState<string | null>(null);

  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    if (!fileName) {
      setBinBlob(null);
      setError(null);
      setLoading(false);
      return;
    }

    // Reset both blob and error on a new fileName so the [binBlob] effect
    // can clear the canvas before the new image arrives.
    setBinBlob(null);
    setError(null);
    setLoading(true);

    let cancelled = false;
    fetch(`/api/images/${encodeURIComponent(fileName)}`)
      .then((response) => {
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}`);
        }
        return response.blob();
      })
      .then((blob) => {
        if (cancelled) return;
        // Don't clear loading here — keep the overlay up through renderBin
        // so the user never sees the stale canvas peek through.
        setBinBlob(blob);
      })
      .catch((err) => {
        if (cancelled) return;
        console.error("Failed to fetch bin file:", err);
        setError(String((err as Error).message ?? err));
        setLoading(false);
      });

    return () => {
      cancelled = true;
    };
  }, [fileName]);

  useEffect(() => {
    if (!canvasRef.current) return;
    if (!binBlob) {
      // Clear stale content while a new fetch is in flight or on error.
      const ctx = canvasRef.current.getContext("2d");
      if (ctx) {
        ctx.clearRect(0, 0, canvasRef.current.width, canvasRef.current.height);
      }
      return;
    }
    renderBin(binBlob, canvasRef.current)
      .then(() => setLoading(false))
      .catch((err) => {
        console.error("Failed to render bin file:", err);
        setError(String((err as Error).message ?? err));
        setLoading(false);
      });
  }, [binBlob]);

  if (!fileName) {
    return null;
  }

  return (
    <>
      <div className="lvgl-bin-viewer-backdrop" onClick={onClose}>
        <div className="lvgl-bin-viewer" onClick={(e) => e.stopPropagation()}>
          <div className="title-container">
            <h2>{fileName}</h2>
            <div className="flex-fill" />
            <button type="button" onClick={onClose}>
              Close
            </button>
          </div>
          <div className="canvas-container">
            {error ? (
              <p class="error">Failed to load: {error}</p>
            ) : (
              <canvas ref={canvasRef} />
            )}
          </div>
        </div>
      </div>

      {loading && <LoadingOverlay />}
    </>
  );
};
