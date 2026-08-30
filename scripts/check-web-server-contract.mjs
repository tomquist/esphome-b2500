#!/usr/bin/env node
// Guards the web UI against changes in the ESPHome web_server event format.
//
// The B2500 storage dashboard (tomquist/esphome-webserver-b2500) parses the
// JSON that ESPHome's web_server component pushes over /events. That format is
// not a documented API, and ESPHome 2026.8.0 changed it -- `name_id` was removed
// and its "domain/device/name" form moved into `id` -- which silently emptied
// the dashboard on every rebuilt firmware (discussion #305).
//
// ESPHome's web_server does not build for the `host` platform, so CI cannot boot
// a device and read a real event stream. Instead this reads the source of truth
// one level up: the JSON keys web_server.cpp emits, and the exact code that
// builds an entity id. A removed key or a reworked id builder fails the check,
// which is the moment to look at the web UI's parser.
//
//   node scripts/check-web-server-contract.mjs                 # pinned version
//   node scripts/check-web-server-contract.mjs --ref dev        # upcoming release
//   node scripts/check-web-server-contract.mjs --ref dev --update
//
// --update rewrites the snapshot; do that deliberately, together with whatever
// change the web UI needs.

import crypto from "crypto";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const SNAPSHOT = path.join(__dirname, "web-server-contract.json");
const BUILD_WORKFLOW = path.join(__dirname, "..", ".github", "workflows", "build-esphome.yml");
const SOURCE = (ref) =>
  `https://raw.githubusercontent.com/esphome/esphome/${ref}/esphome/components/web_server/web_server.cpp`;

// The ESPHome version firmware is actually built with, so the check follows the
// pinned version without a second place to update.
function pinnedRef() {
  const wf = fs.readFileSync(BUILD_WORKFLOW, "utf8");
  const m = wf.match(/client_payload\.esphome_version\s*\|\|\s*'([^']+)'/);
  if (!m) throw new Error(`could not read the pinned ESPHome version from ${BUILD_WORKFLOW}`);
  return m[1];
}

// Every entity field web_server.cpp writes goes through root[ESPHOME_F("...")],
// so this enumerates the whole event payload vocabulary.
function emittedKeys(source) {
  const keys = new Set();
  for (const m of source.matchAll(/root\[ESPHOME_F\("([a-z_]+)"\)\]/g)) keys.add(m[1]);
  return [...keys].sort();
}

// The body of set_json_id(), normalized: comments dropped and whitespace
// collapsed, so formatting churn does not trip the check but a change to how ids
// are assembled does.
function idBuilder(source) {
  const start = source.indexOf("static void set_json_id(");
  if (start < 0) throw new Error("set_json_id() not found -- web_server.cpp was restructured");
  let i = source.indexOf("{", start);
  if (i < 0) throw new Error("set_json_id() body not found");
  let depth = 0;
  let end = -1;
  for (; i < source.length; i++) {
    if (source[i] === "{") depth++;
    else if (source[i] === "}" && --depth === 0) {
      end = i + 1;
      break;
    }
  }
  if (end < 0) throw new Error("set_json_id() body is unbalanced");
  return source
    .slice(start, end)
    .replace(/\/\*[\s\S]*?\*\//g, " ")
    .replace(/\/\/[^\n]*/g, " ")
    .replace(/\s+/g, " ")
    .trim();
}

const args = process.argv.slice(2);
const update = args.includes("--update");
const refArg = args.indexOf("--ref");
const ref = refArg >= 0 ? args[refArg + 1] : pinnedRef();

const res = await fetch(SOURCE(ref));
if (!res.ok) {
  console.error(`✗ could not fetch web_server.cpp at ${ref}: HTTP ${res.status}`);
  process.exit(2);
}
const source = await res.text();

const keys = emittedKeys(source);
const builder = idBuilder(source);
const builderHash = crypto.createHash("sha256").update(builder).digest("hex");

if (update) {
  const previous = JSON.parse(fs.readFileSync(SNAPSHOT, "utf8"));
  fs.writeFileSync(
    SNAPSHOT,
    JSON.stringify({ ...previous, ref, keys, idBuilder: builderHash }, null, 2) + "\n"
  );
  console.log(`updated ${path.basename(SNAPSHOT)} from ${ref} (${keys.length} keys)`);
  process.exit(0);
}

const snapshot = JSON.parse(fs.readFileSync(SNAPSHOT, "utf8"));
const failures = [];

// Keys the web UI reads. Losing one of these breaks it outright.
const missing = snapshot.requiredKeys.filter((k) => !keys.includes(k));
if (missing.length) {
  failures.push(
    `keys the web UI depends on are gone: ${missing.join(", ")}\n` +
      `  The dashboard and entity table read these from every state event; see\n` +
      `  parseStorageEntity() in tomquist/esphome-webserver-b2500.`
  );
}

// How an entity id is assembled is the contract that broke in 2026.8: the keys
// were all still there, the id simply meant something else.
if (builderHash !== snapshot.idBuilder) {
  failures.push(
    `set_json_id() changed -- entity ids may have a new format.\n` +
      `  Recorded at ${snapshot.ref}, now at ${ref}. Compare:\n` +
      `  ${SOURCE(snapshot.ref)}\n  ${SOURCE(ref)}\n` +
      `  Check that parseStorageEntity() still recognizes the ids it produces,\n` +
      `  then re-record with --ref ${ref} --update.`
  );
}

// Everything else is informational: additions are normal, and a removal outside
// requiredKeys does not affect this project.
const added = keys.filter((k) => !snapshot.keys.includes(k));
const removed = snapshot.keys.filter((k) => !keys.includes(k));
if (added.length) console.log(`note: new event keys at ${ref}: ${added.join(", ")}`);
if (removed.length) console.log(`note: event keys gone at ${ref}: ${removed.join(", ")}`);

if (failures.length) {
  console.error(`\n✗ ESPHome web_server event contract changed at ${ref}:\n`);
  for (const f of failures) console.error(`  - ${f}\n`);
  process.exit(1);
}

console.log(`✓ web_server event contract unchanged at ${ref} (${keys.length} keys)`);
