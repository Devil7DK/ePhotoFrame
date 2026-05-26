import "./App.css";

import { useEffect, useState } from "preact/hooks";

import { ManageView, SetupView } from "./views";

type Mode = "setup" | "manage";

export const App = () => {
  const [mode, setMode] = useState<Mode | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    fetch("/api/mode")
      .then((r) => {
        if (!r.ok) throw new Error(`HTTP ${r.status}`);
        return r.json();
      })
      .then((d: { mode: Mode }) => setMode(d.mode))
      .catch((e) => setError(String(e)));
  }, []);

  if (error) {
    return (
      <div class="status">
        <p>Failed to load: {error}</p>
      </div>
    );
  }
  if (!mode) {
    return (
      <div class="status">
        <p>Loading…</p>
      </div>
    );
  }
  return mode === "setup" ? <SetupView /> : <ManageView />;
};
