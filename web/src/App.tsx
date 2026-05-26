import "./App.css";

import { useEffect, useState } from "preact/hooks";

import { ManageView, SetupView } from "./views";

type Mode = "setup" | "manage";

const Footer = () => (
  <footer class="app-footer">
    <p>
      by{" "}
      <a
        href="https://github.com/Devil7DK"
        target="_blank"
        rel="noopener noreferrer"
      >
        Dineshkumar T
      </a>
      {" · "}
      <a
        href="https://github.com/Devil7DK/ePhotoFrame"
        target="_blank"
        rel="noopener noreferrer"
      >
        Source
      </a>
    </p>
  </footer>
);

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

  let body;
  if (error) {
    body = (
      <div class="status">
        <p>Failed to load: {error}</p>
      </div>
    );
  } else if (!mode) {
    body = (
      <div class="status">
        <p>Loading…</p>
      </div>
    );
  } else {
    body = mode === "setup" ? <SetupView /> : <ManageView />;
  }

  return (
    <>
      {body}
      <Footer />
    </>
  );
};
