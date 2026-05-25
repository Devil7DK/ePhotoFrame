import type { SubmitEventHandler } from "preact";
import { useCallback, useState } from "preact/hooks";

import { downloadBlob, processFile } from "./utils";

export const ManageView = () => {
  const [logs, setLogs] = useState<string[]>([]);

  const logLine = useCallback((line: string) => {
    setLogs((logs) => [...logs, line]);
  }, []);

  const onSubmit: SubmitEventHandler<HTMLFormElement> = useCallback(
    async (e) => {
      e.preventDefault();

      const formData = new FormData(e.currentTarget);

      const files = formData.getAll("files").filter((f) => f instanceof File);
      const orientation = formData.get("orient");

      const [dstW, dstH] =
        orientation === "landscape" ? [320, 240] : [240, 320];

      let succeeded = 0,
        failed = 0;
      for (const file of files) {
        try {
          logLine(`Processing ${file.name}...`);
          const blob = await processFile(file, dstW, dstH);
          const stem = file.name.replace(/\.[^.]+$/, "");

          downloadBlob(blob, `${stem}.bin`);
          logLine(`  -> ${stem}.bin`);
          succeeded++;

          await new Promise((r) => setTimeout(r, 50));
        } catch (err) {
          logLine(`  x ${(err as Error).message}`);
          failed++;
        }
      }
      logLine(
        `Done. ${succeeded} succeeded${failed ? `, ${failed} failed` : ""}.`,
      );
    },
    [logLine],
  );

  return (
    <div>
      <h1>ePhotoFrame</h1>
      <p class="muted">
        Image upload and gallery management is coming next. For now this is the
        local converter — drop images here, download the LVGL <code>.bin</code>{" "}
        files, then copy them to <code>/images</code> on the SD card.
      </p>
      <form onSubmit={onSubmit}>
        <p>
          <label>
            Images:{" "}
            <input
              name="files"
              type="file"
              accept="image/*"
              multiple
              required
            />
          </label>
        </p>
        <p>
          Orientation:
          <label>
            <input name="orient" type="radio" value="landscape" checked />{" "}
            Landscape (320x240)
          </label>
          <label>
            <input name="orient" type="radio" value="portrait" /> Portrait
            (240x320)
          </label>
        </p>

        <p>
          <button type="submit">Process &amp; download</button>
        </p>
      </form>

      <pre>{logs.join("\n")}</pre>
    </div>
  );
};
