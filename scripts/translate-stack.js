#!/usr/bin/env node
// Reads stdin line-by-line, finds `bundle.js:LINE:COL` references, and
// replaces them with their original source positions using
// dist/remarkable.bundle.js.map. Pass-through for everything else.

const fs = require("fs");
const path = require("path");
const readline = require("readline");

const MAP_PATH = path.resolve(
  __dirname,
  "..",
  "dist",
  "remarkable.bundle.js.map",
);

const TRACE_MAPPING_PATH = path.resolve(
  __dirname,
  "..",
  "template",
  "node_modules",
  "@jridgewell",
  "trace-mapping",
);

const FRAME_RE = /(?:[\w./-]*remarkable\.bundle\.js|bundle\.js):(\d+):(\d+)/g;

let tm = null;
let originalPositionFor = null;
let warned = false;

function load() {
  if (tm) return tm;
  if (!fs.existsSync(MAP_PATH)) {
    if (!warned) {
      process.stderr.write(
        `[translate-stack] no source map at ${MAP_PATH}; pass-through only\n`,
      );
      warned = true;
    }
    return null;
  }
  const mod = require(TRACE_MAPPING_PATH);
  originalPositionFor = mod.originalPositionFor;
  tm = new mod.TraceMap(JSON.parse(fs.readFileSync(MAP_PATH, "utf8")));
  return tm;
}

function translate(line) {
  if (!load()) return line;
  return line.replace(FRAME_RE, (whole, ln, col) => {
    try {
      const orig = originalPositionFor(tm, {
        line: parseInt(ln, 10),
        column: parseInt(col, 10),
      });
      if (orig && orig.source) {
        const file = orig.source.replace(/^.*\/(template\/)?/, "");
        return `${file}:${orig.line}:${orig.column ?? 0}`;
      }
    } catch {}
    return whole;
  });
}

const rl = readline.createInterface({ input: process.stdin, terminal: false });
rl.on("line", (line) => process.stdout.write(translate(line) + "\n"));
rl.on("close", () => process.exit(0));
