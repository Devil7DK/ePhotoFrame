import "cropperjs/dist/cropper.css";
import "./CropEditor.css";

import Cropper from "cropperjs";
import type { FunctionalComponent } from "preact";
import { useEffect, useMemo, useRef } from "preact/hooks";

import type { CropRegion } from "../../utils";

interface ICropEditorProps {
  file: File;
  aspectRatio: number;
  initialCrop?: CropRegion;
  onConfirm: (crop: CropRegion) => void;
  onCancel: () => void;
}

export const CropEditor: FunctionalComponent<ICropEditorProps> = ({
  file,
  aspectRatio,
  initialCrop,
  onConfirm,
  onCancel,
}) => {
  const imgRef = useRef<HTMLImageElement>(null);
  const cropperRef = useRef<Cropper | null>(null);
  // Read once on mount — later prop changes shouldn't recreate the cropper.
  const initialCropRef = useRef(initialCrop);

  const sourceUrl = useMemo(() => URL.createObjectURL(file), [file]);

  useEffect(() => () => URL.revokeObjectURL(sourceUrl), [sourceUrl]);

  useEffect(() => {
    if (!imgRef.current) return;
    const cropper = new Cropper(imgRef.current, {
      aspectRatio,
      viewMode: 1,
      dragMode: "move",
      cropBoxMovable: false,
      cropBoxResizable: false,
      autoCropArea: 1,
      background: false,
      guides: true,
      toggleDragModeOnDblclick: false,
      responsive: true,
      ready: () => {
        const initial = initialCropRef.current;
        if (initial) {
          cropper.setData({
            x: initial.sx,
            y: initial.sy,
            width: initial.sw,
            height: initial.sh,
          });
        }
      },
    });
    cropperRef.current = cropper;
    return () => {
      cropper.destroy();
      cropperRef.current = null;
    };
  }, [aspectRatio, sourceUrl]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") onCancel();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [onCancel]);

  const onApply = () => {
    if (!cropperRef.current) return;
    const data = cropperRef.current.getData(true);
    onConfirm({ sx: data.x, sy: data.y, sw: data.width, sh: data.height });
  };

  return (
    <div className="crop-editor-backdrop" onClick={onCancel}>
      <div
        className="crop-editor"
        role="dialog"
        aria-modal="true"
        aria-label="Crop image"
        onClick={(e) => e.stopPropagation()}
      >
        <header>
          <h2>Crop image</h2>
          <span className="muted">{file.name}</span>
        </header>
        <div className="crop-area">
          <img ref={imgRef} src={sourceUrl} alt="" />
        </div>
        <div className="actions">
          <button type="button" onClick={onCancel}>
            Cancel
          </button>
          <button type="button" class="primary" onClick={onApply}>
            Apply crop
          </button>
        </div>
      </div>
    </div>
  );
};
