import preact from "@preact/preset-vite";
import { configDotenv } from "dotenv";
import { existsSync, readFileSync, writeFileSync } from "fs";
import { cyan, green } from "kolorist";
import { join, resolve } from "path";
import { defineConfig, type Plugin } from "vite";
import { viteSingleFile } from "vite-plugin-singlefile";
import { gzipSync } from "zlib";

configDotenv();

function writeProgmem(): Plugin {
  return {
    name: "write-progmem",
    closeBundle() {
      if (this.environment.mode !== "build") return;

      const startTime = Date.now();

      const distPath = resolve(__dirname, "dist");
      const indexPath = join(distPath, "index.html");

      if (!existsSync(indexPath)) {
        console.error("index.html not found in dist folder");
        return;
      }

      const indexContent = readFileSync(indexPath, "utf-8");
      const compressed = gzipSync(Buffer.from(indexContent), { level: 9 });

      if (!compressed) {
        console.error("Gzip compression failed");
        return;
      }

      const headerContent = `#ifndef HTMLDATA_H\n#define HTMLDATA_H\n\n#include <Arduino.h>\n\nextern const uint8_t HTML_DATA[${compressed.length}];\n\n#endif\n`;

      const BYTES_PER_LINE = 30;
      const lines: string[] = [];
      for (let i = 0; i < compressed.length; i += BYTES_PER_LINE) {
        const chunk = compressed.subarray(i, i + BYTES_PER_LINE);
        lines.push(Array.from(chunk).join(","));
      }
      const cppContent = `#include "HtmlData.h"\n\nconst uint8_t HTML_DATA[${compressed.length}] PROGMEM = {\n${lines.join(",\n")}\n};\n`;

      const outputDir = resolve(__dirname, "..");

      writeFileSync(join(outputDir, "HtmlData.h"), headerContent);
      writeFileSync(join(outputDir, "HtmlData.cpp"), cppContent);

      console.log(
        green(
          `✓ Gzipped HTML to ${cyan(new Intl.NumberFormat().format(compressed.length))} bytes in ${cyan(`${Date.now() - startTime}ms`)} and wrote to HtmlData.h/cpp`,
        ),
      );
    },
  };
}

function mockAPIs(): Plugin {
  const MODE: "setup" | "manage" =
    process.env.SIMULATION_MODE === "manage" ? "manage" : "setup";

  // In-memory state, so POST /api/config then GET /api/config reflects the change.
  const state = {
    ssid: MODE === "manage" ? "HomeWiFi" : "",
    password: MODE === "manage" ? "secret123" : "",
    autoplay_ms: 0,
  };

  const FAKE_NETWORKS = [
    { ssid: "HomeWiFi", rssi: -45, encryption: 3 },
    { ssid: "Guest", rssi: -67, encryption: 0 },
    { ssid: "NeighborAP_5G", rssi: -82, encryption: 4 },
    { ssid: "Cafe-Free", rssi: -88, encryption: 0 },
  ];

  // Simulated image library — name → byte count. Bytes themselves are
  // generated on demand for downloads (we don't need to track real content
  // for the dev loop; just plausible sizes).
  const images: Map<string, number> = new Map([
    ["sample1.bin", 153612],
    ["sample2.bin", 153612],
    ["sunset.bin", 153612],
  ]);

  function json(
    res: import("http").ServerResponse,
    status: number,
    body: unknown,
  ) {
    res.statusCode = status;
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(body));
  }

  function readBody(req: import("http").IncomingMessage): Promise<string> {
    return new Promise((resolve, reject) => {
      let buf = "";
      req.on("data", (chunk) => (buf += chunk));
      req.on("end", () => resolve(buf));
      req.on("error", reject);
    });
  }

  function path(url?: string) {
    return (url ?? "").split("?")[0];
  }

  function delay(ms: number) {
    return new Promise((r) => setTimeout(r, ms));
  }

  return {
    name: "mock-apis",
    configureServer(server) {
      server.middlewares.use(async (req, res, next) => {
        const p = path(req.url);
        if (!p.startsWith("/api/")) return next();

        if (p === "/api/mode" && req.method === "GET") {
          return json(res, 200, { mode: MODE });
        }

        if (p === "/api/status" && req.method === "GET") {
          return json(res, 200, {
            uptime_ms: Math.floor(process.uptime() * 1000),
            wifi: MODE === "manage" ? "Connected" : "Setup Mode",
            ip: MODE === "manage" ? "192.168.1.42" : "192.168.4.1",
            rssi: MODE === "manage" ? -55 : 0,
            free_heap: 213000,
          });
        }

        if (p === "/api/networks" && req.method === "GET") {
          await delay(300);
          return json(res, 200, {
            networks: FAKE_NETWORKS,
            count: FAKE_NETWORKS.length,
          });
        }

        if (p === "/api/config" && req.method === "GET") {
          return json(res, 200, {
            wifi: { ssid: state.ssid, configured: !!state.ssid },
            autoplay_ms: state.autoplay_ms,
          });
        }

        if (p === "/api/config" && req.method === "POST") {
          if (MODE !== "setup") {
            return json(res, 403, { error: "setup mode only" });
          }
          const body = await readBody(req);
          let parsed: { wifi?: { ssid?: string; password?: string } };
          try {
            parsed = JSON.parse(body);
          } catch {
            return json(res, 400, { error: "bad json" });
          }
          const ssid = parsed?.wifi?.ssid ?? "";
          const password = parsed?.wifi?.password ?? "";
          if (!ssid) return json(res, 400, { error: "empty ssid" });
          if (ssid.length >= 33 || password.length >= 65) {
            return json(res, 400, { error: "ssid or password too long" });
          }
          await delay(500);
          state.ssid = ssid;
          state.password = password;
          return json(res, 200, { ok: true, restarting: true });
        }

        if (p === "/api/restart" && req.method === "POST") {
          await delay(200);
          return json(res, 200, { ok: true, restarting: true });
        }

        if (p === "/api/config/autoplay" && req.method === "POST") {
          if (MODE !== "manage") return json(res, 403, { error: "manage mode only" });
          const body = await readBody(req);
          let parsed: { autoplay_ms?: number };
          try {
            parsed = JSON.parse(body);
          } catch {
            return json(res, 400, { error: "bad json" });
          }
          const ms = parsed?.autoplay_ms;
          if (typeof ms !== "number") return json(res, 400, { error: "missing autoplay_ms" });
          if (ms < 0) return json(res, 400, { error: "autoplay_ms must be >= 0" });
          if (ms > 0 && ms < 10000) return json(res, 400, { error: "autoplay_ms must be 0 or >= 10000" });
          if (ms > 3600000) return json(res, 400, { error: "autoplay_ms must be <= 3600000" });
          await delay(150);
          state.autoplay_ms = ms;
          return json(res, 200, { ok: true });
        }

        if (p === "/api/wifi-reset" && req.method === "POST") {
          if (MODE !== "manage") return json(res, 403, { error: "manage mode only" });
          await delay(200);
          return json(res, 200, { ok: true, restarting: true });
        }

        if (p === "/api/factory-reset" && req.method === "POST") {
          if (MODE !== "manage") return json(res, 403, { error: "manage mode only" });
          await delay(200);
          return json(res, 200, { ok: true, restarting: true });
        }

        // ----- /api/images endpoints -----

        if (p === "/api/images" && req.method === "GET") {
          if (MODE !== "manage") return json(res, 403, { error: "manage mode only" });
          await delay(150);
          return json(res, 200, {
            images: Array.from(images.entries()).map(([name, size]) => ({
              name, size,
            })),
          });
        }

        if (p === "/api/images" && req.method === "POST") {
          if (MODE !== "manage") return json(res, 403, { error: "manage mode only" });
          const url = new URL(req.url ?? "", "http://localhost");
          const name = url.searchParams.get("name") ?? "";
          if (!name.endsWith(".bin") || name.includes("/") || name.includes("..")) {
            return json(res, 400, { error: "bad name" });
          }
          let received = 0;
          req.on("data", (chunk) => (received += chunk.length));
          await new Promise((r) => req.on("end", r));
          await delay(400);
          images.set(name, received);
          return json(res, 200, { ok: true });
        }

        // /api/images/:name
        const imgMatch = p.match(/^\/api\/images\/(.+)$/);
        if (imgMatch) {
          const name = decodeURIComponent(imgMatch[1]);
          if (req.method === "GET") {
            if (MODE !== "manage") return json(res, 403, { error: "manage mode only" });
            const size = images.get(name);
            if (size === undefined) return json(res, 404, { error: "not found" });
            res.setHeader("Content-Type", "application/octet-stream");
            res.setHeader("Content-Length", String(size));
            res.end(Buffer.alloc(size));
            return;
          }
          if (req.method === "DELETE") {
            if (MODE !== "manage") return json(res, 403, { error: "manage mode only" });
            if (!images.has(name)) return json(res, 404, { error: "not found" });
            images.delete(name);
            return json(res, 200, { ok: true });
          }
        }

        // Unknown /api/* → let it 404 through Vite's default chain.
        next();
      });
    },
  };
}

// https://vite.dev/config/
export default defineConfig({
  css: {
    transformer: "lightningcss",
  },
  plugins: [preact(), viteSingleFile(), writeProgmem(), mockAPIs()],
});
