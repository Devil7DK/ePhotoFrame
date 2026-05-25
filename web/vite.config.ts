import preact from "@preact/preset-vite";
import { existsSync, readFileSync, writeFileSync } from "fs";
import { cyan, green } from "kolorist";
import { join, resolve } from "path";
import { defineConfig } from "vite";
import { viteSingleFile } from "vite-plugin-singlefile";
import { gzipSync } from "zlib";

function writeProgmem() {
  return {
    name: "write-progmem",
    closeBundle() {
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
        green(`✓ Gzipped HTML to ${cyan(new Intl.NumberFormat().format(compressed.length))} bytes in ${cyan(`${((Date.now() - startTime))}ms`)} and wrote to HtmlData.h/cpp`),
      );
    },
  };
}

// https://vite.dev/config/
export default defineConfig({
  css: {
    transformer: "lightningcss",
  },
  plugins: [preact(), viteSingleFile(), writeProgmem()],
});
