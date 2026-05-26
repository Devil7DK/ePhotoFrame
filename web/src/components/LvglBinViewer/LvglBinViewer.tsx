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

  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    if (!fileName) {
      setBinBlob(null);
      setLoading(false);
      return;
    }

    setLoading(true);
    fetch(`/api/images/${fileName}`)
      .then((response) => response.blob())
      .then((blob) => {
        setBinBlob(blob);
        setLoading(false);
      })
      .catch((err) => {
        console.error("Failed to fetch bin file:", err);
        setBinBlob(null);
        setLoading(false);
      });
  }, [fileName]);

  useEffect(() => {
    if (!binBlob || !canvasRef.current) return;

    renderBin(binBlob, canvasRef.current);
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
            <button onClick={onClose}>Close</button>
          </div>
          <div className="canvas-container">
            <canvas ref={canvasRef} />
          </div>
        </div>
      </div>

      {loading && <LoadingOverlay />}
    </>
  );
};
