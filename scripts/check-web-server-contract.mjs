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
//   node scripts/check-web-server-contract.mjs                  # pinned version
//   node scripts/check-web-server-contract.mjs --ref dev         # upcoming release
//   node scripts/check-web-server-contract.mjs --ref dev --accept 'why ids are unchanged'
//   node scripts/check-web-server-contract.mjs --ref 2026.9.0 --update
//
// A hash over set_json_id() cannot tell a rewrite that changes the id format
// from one that leaves it byte-for-byte the same, so a human reads the diff and
// records the verdict. --accept keeps the recorded baseline and adds the new
// body as a known-good variant, with the reasoning stored beside it; --update
// re-baselines onto a version firmware is actually built with. Both rewrite the
// snapshot: do either deliberately, together with whatever change the web UI
// needs.

import crypto from "crypto";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const SNAPSHOT = path.join(__dirname, "web-server-contract.json");
const BUILD_WORKFLOW = path.join(__dirname, "..", ".github", "workflows", "build-esphome.yml");
const SOURCE = (ref) =>
  `https://raw.githubusercontent.com/esphome/esphome/${ref}/esphome/components/web_server/web_server.cpp`;

// Exit 1 means a verdict: the format changed. Exit 2 means the check could not
// reach a verdict at all -- an unreachable source, an unreadable snapshot. The
// nightly workflow files an issue on 1 and only reddens the job on 2, so
// conflating them would report ESPHome changes that never happened.
function bail(message) {
  console.error(`✗ ${message}`);
  process.exit(2);
}

// The ESPHome version firmware is actually built with, so the check follows the
// pinned version without a second place to update. Matches the job-level
// literal; the step-level ESPHome version is an expression with no quotes, so
// it cannot be picked up by accident.
function pinnedRef() {
  const wf = fs.readFileSync(BUILD_WORKFLOW, "utf8");
  const m = wf.match(/^\s*ESPHOME_VERSION:\s*'([^']+)'/m);
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

// Blank out comments and string/char literals, keeping length so offsets still
// line up with the original. Brace matching runs over this: a stray "{" in a
// comment ("the {device} form") would otherwise swallow or truncate the function
// body and report a format change that never happened.
function maskCommentsAndLiterals(source) {
  const out = source.split("");
  const blank = (from, to) => {
    for (let k = from; k < to && k < out.length; k++) if (out[k] !== "\n") out[k] = " ";
  };
  for (let i = 0; i < source.length; i++) {
    const two = source.slice(i, i + 2);
    if (two === "//") {
      const end = source.indexOf("\n", i);
      const stop = end < 0 ? source.length : end;
      blank(i, stop);
      i = stop;
    } else if (two === "/*") {
      const end = source.indexOf("*/", i + 2);
      const stop = end < 0 ? source.length : end + 2;
      blank(i, stop);
      i = stop - 1;
    } else if (source[i] === "R" && source[i + 1] === '"') {
      // Raw string: R"delim( ... )delim", where the body may hold quotes and
      // backslashes that the normal string scan below would misread.
      const open = source.indexOf("(", i + 2);
      const delim = open < 0 ? null : source.slice(i + 2, open);
      const close = delim === null ? -1 : source.indexOf(`)${delim}"`, open);
      if (close < 0) {
        blank(i, source.length);
        i = source.length;
      } else {
        const stop = close + delim.length + 2;
        blank(i, stop);
        i = stop - 1;
      }
    } else if (source[i] === '"' || source[i] === "'") {
      const quote = source[i];
      let j = i + 1;
      while (j < source.length && source[j] !== quote) j += source[j] === "\\" ? 2 : 1;
      blank(i, Math.min(j + 1, source.length));
      i = j;
    }
  }
  return out.join("");
}

// The body of set_json_id(), normalized: comments dropped and whitespace
// collapsed, so formatting churn does not trip the check but a change to how ids
// are assembled does.
function idBuilder(source) {
  const start = source.indexOf("static void set_json_id(");
  // Not an extraction failure but a finding in its own right: the function that
  // builds entity ids is gone, so the id format is anyone's guess.
  if (start < 0) return null;
  const masked = maskCommentsAndLiterals(source);
  let i = masked.indexOf("{", start);
  if (i < 0) return null;
  let depth = 0;
  let end = -1;
  for (; i < masked.length; i++) {
    if (masked[i] === "{") depth++;
    else if (masked[i] === "}" && --depth === 0) {
      end = i + 1;
      break;
    }
  }
  if (end < 0) return null;
  return source
    .slice(start, end)
    .replace(/\/\*[\s\S]*?\*\//g, " ")
    .replace(/\/\/[^\n]*/g, " ")
    .replace(/\s+/g, " ")
    .trim();
}

// Bodies that are not the recorded baseline but were read by a human and found
// to emit the same id string. ESPHome refactors set_json_id() on its own
// schedule -- dev took JsonObject by value and cast id_buf at the assignment
// (issue #317), leaving the id byte-for-byte the same -- and the hash cannot
// tell such a rewrite from a new format. Recording the variant keeps the check
// strict about everything else instead of forcing a re-baseline onto a version
// firmware is not built with.
function acceptedVariants(snapshot) {
  return Array.isArray(snapshot.alsoAccepted) ? snapshot.alsoAccepted : [];
}

// The reasoning is the whole point of an acceptance -- a hash on its own says
// nothing about why the ids are the same -- so a token like "ok" is not one.
// The same bar applies wherever a note arrives, through --accept or a hand edit,
// or the CLI would happily write a record the snapshot check then rejects.
const MIN_NOTE_LENGTH = 20;

function isReasoning(note) {
  return typeof note === "string" && note.trim().length > MIN_NOTE_LENGTH;
}

// Why a recorded acceptance cannot be used, or null if it is well formed.
function variantProblem(variant) {
  if (!variant || typeof variant !== "object") return "is not an object";
  if (!variant.ref) return "has no ref";
  if (!/^[0-9a-f]{64}$/.test(variant.idBuilder ?? "")) return "has no idBuilder hash";
  if (!isReasoning(variant.note)) {
    return `has no reasoning (more than ${MIN_NOTE_LENGTH} characters)`;
  }
  return null;
}

// A malformed acceptance has to stop the run before the verdict, and stop it as
// exit 2: an entry that reads `null` would otherwise throw, and an uncaught
// throw exits 1 -- the code that means the ESPHome format changed, which files
// an issue about a change nobody made.
function snapshotProblems(snapshot) {
  return acceptedVariants(snapshot)
    .map((variant, i) => {
      const problem = variantProblem(variant);
      return problem && `alsoAccepted[${i}] ${problem}`;
    })
    .filter(Boolean);
}

// The verdict, given an already-fetched source: failures are format changes and
// fail the run, notes are informational. Pure, so the fixtures can cover the
// paths a live fetch would only reach on the day ESPHome changes.
function evaluate({ snapshot, ref, keys, builder, builderHash }) {
  const failures = [];
  const notes = [];

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
  if (!builder) {
    failures.push(
      `set_json_id() is gone -- web_server.cpp was restructured, so entity ids are\n` +
        `  built somewhere else now and may have a new format. Compare:\n` +
        `  ${SOURCE(snapshot.ref)}\n  ${SOURCE(ref)}`
    );
  } else if (builderHash !== snapshot.idBuilder) {
    const accepted = acceptedVariants(snapshot).find((v) => v && v.idBuilder === builderHash);
    if (accepted) {
      // Said out loud on every run: an acceptance is a human's verdict on one
      // rewrite, not a permanent exemption, and it should be visible while it
      // stands rather than quietly outliving the version it was made for.
      const when = accepted.recorded ? ` on ${accepted.recorded}` : "";
      notes.push(
        `set_json_id() at ${ref} is not the ${snapshot.ref} baseline but matches the\n` +
          `  variant accepted at ${accepted.ref}${when}: ${accepted.note}`
      );
    } else {
      failures.push(
        `set_json_id() changed -- entity ids may have a new format.\n` +
          `  Recorded at ${snapshot.ref}, now at ${ref}. Compare:\n` +
          `  ${SOURCE(snapshot.ref)}\n  ${SOURCE(ref)}\n` +
          `  Check that parseStorageEntity() still recognizes the ids it produces.\n` +
          `  If the ids are unchanged, record that verdict with\n` +
          `  --ref ${ref} --accept '<why the ids are unchanged>'; re-baseline with\n` +
          `  --ref ${ref} --update once firmware is built with a version carrying it.`
      );
    }
  }

  // Everything else is informational: additions are normal, and a removal outside
  // requiredKeys does not affect this project.
  const added = keys.filter((k) => !snapshot.keys.includes(k));
  const removed = snapshot.keys.filter((k) => !keys.includes(k));
  if (added.length) notes.push(`new event keys at ${ref}: ${added.join(", ")}`);
  if (removed.length) notes.push(`event keys gone at ${ref}: ${removed.join(", ")}`);

  return { failures, notes };
}

export {
  acceptedVariants,
  emittedKeys,
  evaluate,
  isReasoning,
  MIN_NOTE_LENGTH,
  snapshotProblems,
  variantProblem,
  idBuilder,
  maskCommentsAndLiterals,
  pinnedRef,
  SOURCE,
};

// Importable for tests (scripts/check-web-server-contract.test.mjs); everything
// below runs only when the script is the program being executed.
const isMain =
  process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url);
if (isMain) {

const args = process.argv.slice(2);
const update = args.includes("--update");
const acceptArg = args.indexOf("--accept");
const acceptNote = acceptArg >= 0 ? (args[acceptArg + 1] || "").trim() : null;
if (acceptArg >= 0) {
  if (!acceptNote || acceptNote.startsWith("--"))
    bail("--accept needs a note saying why the ids are unchanged");
  // Held to the bar the snapshot itself enforces, so --accept cannot report
  // success and leave behind a record the next run refuses to read.
  if (!isReasoning(acceptNote))
    bail(`--accept needs more than ${MIN_NOTE_LENGTH} characters saying why the ids are unchanged`);
}
if (update && acceptArg >= 0) bail("--update and --accept do different things; pick one");
const refArg = args.indexOf("--ref");

let ref;
try {
  ref = refArg >= 0 ? args[refArg + 1] : pinnedRef();
} catch (e) {
  bail(e.message);
}

let source;
try {
  // A rejected fetch -- DNS, TLS, a refused connection -- must not read as drift.
  const res = await fetch(SOURCE(ref));
  if (!res.ok) bail(`could not fetch web_server.cpp at ${ref}: HTTP ${res.status}`);
  source = await res.text();
} catch (e) {
  bail(`could not fetch web_server.cpp at ${ref}: ${e.message}`);
}

const keys = emittedKeys(source);
const builder = idBuilder(source);
const builderHash = builder && crypto.createHash("sha256").update(builder).digest("hex");

function readSnapshot() {
  let snapshot;
  try {
    snapshot = JSON.parse(fs.readFileSync(SNAPSHOT, "utf8"));
  } catch (e) {
    bail(`could not read ${path.basename(SNAPSHOT)}: ${e.message}`);
  }
  const problems = snapshotProblems(snapshot);
  if (problems.length) {
    bail(
      `${path.basename(SNAPSHOT)} has unusable acceptances: ${problems.join("; ")}\n` +
        `  Record them with --accept rather than by hand.`
    );
  }
  return snapshot;
}

function writeSnapshot(next) {
  // An empty list would only be a leftover key to explain in review.
  if (next.alsoAccepted && !next.alsoAccepted.length) delete next.alsoAccepted;
  fs.writeFileSync(SNAPSHOT, JSON.stringify(next, null, 2) + "\n");
}

const today = () => new Date().toISOString().slice(0, 10);

if (update) {
  if (!builder) bail(`set_json_id() not found at ${ref}; nothing to record`);
  const previous = readSnapshot();
  // An acceptance for the very body now being baselined has served its purpose:
  // carrying it forward would leave the file explaining a difference from
  // itself.
  const alsoAccepted = acceptedVariants(previous).filter((v) => !v || v.idBuilder !== builderHash);
  writeSnapshot({ ...previous, ref, keys, idBuilder: builderHash, alsoAccepted });
  console.log(`updated ${path.basename(SNAPSHOT)} from ${ref} (${keys.length} keys)`);
  process.exit(0);
}

if (acceptNote) {
  if (!builder) {
    bail(
      `set_json_id() not found at ${ref}; there is no body to accept, and ids are\n` +
        `  now built somewhere else -- find them before recording anything`
    );
  }
  const previous = readSnapshot();
  if (builderHash === previous.idBuilder) {
    console.log(
      `set_json_id() at ${ref} already matches the ${previous.ref} baseline; nothing to accept`
    );
    process.exit(0);
  }
  const alsoAccepted = acceptedVariants(previous);
  const already = alsoAccepted.find((v) => v && v.idBuilder === builderHash);
  if (already) {
    console.log(`already accepted at ${already.ref}: ${already.note}`);
    process.exit(0);
  }
  writeSnapshot({
    ...previous,
    alsoAccepted: [
      ...alsoAccepted,
      { ref, recorded: today(), idBuilder: builderHash, note: acceptNote },
    ],
  });
  console.log(`accepted the set_json_id() body at ${ref} in ${path.basename(SNAPSHOT)}`);
  process.exit(0);
}

const snapshot = readSnapshot();
const { failures, notes } = evaluate({ snapshot, ref, keys, builder, builderHash });
for (const n of notes) console.log(`note: ${n}`);

if (failures.length) {
  console.error(`\n✗ ESPHome web_server event contract changed at ${ref}:\n`);
  for (const f of failures) console.error(`  - ${f}\n`);
  process.exit(1);
}

console.log(`✓ web_server event contract unchanged at ${ref} (${keys.length} keys)`);

}
