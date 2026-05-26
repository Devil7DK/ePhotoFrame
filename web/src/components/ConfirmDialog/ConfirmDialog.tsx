import type { FunctionalComponent } from "preact";
import { useEffect } from "preact/hooks";

import "./ConfirmDialog.css";

interface IConfirmDialogProps {
  title: string;
  message: string;
  confirmLabel?: string;
  confirmKind?: "danger" | "warn";
  onConfirm: () => void;
  onCancel: () => void;
}

export const ConfirmDialog: FunctionalComponent<IConfirmDialogProps> = ({
  title,
  message,
  confirmLabel = "Confirm",
  confirmKind = "danger",
  onConfirm,
  onCancel,
}) => {
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") onCancel();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [onCancel]);

  return (
    <div className="confirm-dialog-backdrop" onClick={onCancel}>
      <div
        className="confirm-dialog"
        role="alertdialog"
        aria-labelledby="confirm-dialog-title"
        onClick={(e) => e.stopPropagation()}
      >
        <h2 id="confirm-dialog-title">{title}</h2>
        <p>{message}</p>
        <div className="actions">
          <button type="button" onClick={onCancel}>
            Cancel
          </button>
          <button type="button" class={confirmKind} onClick={onConfirm}>
            {confirmLabel}
          </button>
        </div>
      </div>
    </div>
  );
};
