import "./SetupView.css";

import type { SubmitEventHandler } from "preact";
import { useCallback, useEffect, useState } from "preact/hooks";

type Network = {
  ssid: string;
  rssi: number;
  encryption: number;
};

type NetworksResponse = {
  networks: Network[];
  count: number;
};

export const SetupView = () => {
  const [networks, setNetworks] = useState<Network[]>([]);
  const [scanning, setScanning] = useState(false);
  const [ssid, setSsid] = useState("");
  const [password, setPassword] = useState("");
  const [submitting, setSubmitting] = useState(false);
  const [submitted, setSubmitted] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const loadNetworks = useCallback(async () => {
    setScanning(true);
    setError(null);
    try {
      const r = await fetch("/api/networks");
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      const data: NetworksResponse = await r.json();
      setNetworks(data.networks ?? []);
    } catch (e) {
      setError(`Scan failed: ${e}`);
    } finally {
      setScanning(false);
    }
  }, []);

  useEffect(() => {
    loadNetworks();
  }, [loadNetworks]);

  const onSubmit: SubmitEventHandler<HTMLFormElement> = useCallback(
    async (e) => {
      e.preventDefault();
      setSubmitting(true);
      setError(null);
      try {
        const r = await fetch("/api/config", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ wifi: { ssid, password } }),
        });
        const data = await r.json().catch(() => ({}));
        if (!r.ok) {
          setError(data.error ?? `HTTP ${r.status}`);
          setSubmitting(false);
          return;
        }
        setSubmitted(true);
      } catch (e) {
        setError(String(e));
        setSubmitting(false);
      }
    },
    [ssid, password],
  );

  if (submitted) {
    return (
      <div class="status">
        <h2>Saved.</h2>
        <p>The device is restarting and will connect to {ssid}.</p>
        <p>You can disconnect from the setup network now.</p>
      </div>
    );
  }

  return (
    <form class="setup-view" onSubmit={onSubmit}>
      <h1>ePhotoFrame Setup</h1>

      <div class="networks">
        <div class="row">
          <strong>Available networks</strong>
          <button type="button" disabled={scanning} onClick={loadNetworks}>
            {scanning ? "Scanning…" : "Refresh"}
          </button>
        </div>
        {networks.length === 0 ? (
          <p class="muted">{scanning ? "Scanning…" : "No networks found."}</p>
        ) : (
          <ul>
            {networks.map((n) => (
              <li
                key={n.ssid}
                class={n.ssid === ssid ? "selected" : undefined}
                onClick={() => setSsid(n.ssid)}
              >
                <span>{n.ssid}</span>
                <span class="muted">{n.rssi} dBm</span>
              </li>
            ))}
          </ul>
        )}
      </div>

      <p>
        <label>
          SSID:{" "}
          <input
            value={ssid}
            onInput={(e) => setSsid(e.currentTarget.value)}
            required
            maxLength={32}
          />
        </label>
      </p>
      <p>
        <label>
          Password:{" "}
          <input
            type="password"
            value={password}
            onInput={(e) => setPassword(e.currentTarget.value)}
            maxLength={64}
          />
        </label>
      </p>

      {error && <p class="error">{error}</p>}

      <p>
        <button type="submit" disabled={submitting || !ssid}>
          {submitting ? "Saving…" : "Save & restart"}
        </button>
      </p>
    </form>
  );
};
