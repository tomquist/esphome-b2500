// Fixtures for the contract checker: source extraction, then the verdict it
// reaches from what it extracted.
//
// The checker's whole value is precision: it fails CI and files an issue when it
// says the ESPHome event format changed. Extraction that miscounts braces would
// cry wolf, so every lexical construct that can carry a brace past the parser
// gets a case here.
//
//   node --test scripts/check-web-server-contract.test.mjs

import assert from "node:assert/strict";
import fs from "node:fs";
import { test } from "node:test";
import {
  acceptedVariants,
  emittedKeys,
  evaluate,
  idBuilder,
  isReasoning,
  maskCommentsAndLiterals,
  MIN_NOTE_LENGTH,
  pinnedRef,
  snapshotProblems,
  variantProblem,
} from "./check-web-server-contract.mjs";

// A miniature stand-in for web_server.cpp: `extra` is dropped into the function
// body, and the body is otherwise identical every time, so a correct extraction
// always yields the same normalized text.
const source = (extra = "") => `
static void set_json_id(JsonObject &root, EntityBase *obj, const char *prefix) {
  ${extra}
  if (device_name) {
    root[ESPHOME_F("device")] = device_name;
  }
  root[ESPHOME_F("id")] = id_buf;
}

static void other_function() {
  root[ESPHOME_F("value")] = 1;
}
`;

const baseline = idBuilder(source());

test("extracts the function body and stops at its closing brace", () => {
  assert.ok(baseline);
  assert.match(baseline, /^static void set_json_id\(/);
  assert.match(baseline, /root\[ESPHOME_F\("id"\)\] = id_buf; \}$/);
  // The following function must not be swallowed.
  assert.doesNotMatch(baseline, /other_function/);
});

test("nested braces in code are matched, not counted as the end", () => {
  assert.match(baseline, /if \(device_name\) \{/);
});

// Comments are dropped by normalization, so a correct extraction is
// byte-identical to the baseline however many braces they carry.
for (const [name, extra] of [
  ["line comment", "// the id reads {domain/device/name -- stray brace"],
  ["block comment", "/* closing } brace in a block comment */"],
  ["comment ending in a backslash", "// trailing backslash and a brace } \\"],
]) {
  test(`ignores braces in a ${name}`, () => {
    assert.equal(idBuilder(source(extra)), baseline);
  });
}

// Literals stay in the recorded body -- they are part of the code -- so what
// matters is that their braces do not move the boundary: the whole body is
// captured and the next function is not swallowed.
for (const [name, extra] of [
  ["string literal", 'const char *fmt = "}{";'],
  ["char literal", "char c = '}';"],
  ["escaped quote then brace", 'const char *q = "say \\" then }";'],
  ["raw string literal", 'const char *r = R"(unbalanced } in a raw string)";'],
  ["raw string with delimiter and quote", 'const char *r = R"x(a " and } here)x";'],
]) {
  test(`ignores braces in a ${name}`, () => {
    const body = idBuilder(source(extra));
    assert.ok(body, `no body extracted for ${name}`);
    assert.match(body, /root\[ESPHOME_F\("id"\)\] = id_buf; \}$/);
    assert.doesNotMatch(body, /other_function/);
    assert.equal(body, baseline.replace("{ if (device_name)", `{ ${extra} if (device_name)`));
  });
}

test("reports a missing function rather than throwing", () => {
  assert.equal(idBuilder("int main() { return 0; }"), null);
});

test("reports an unterminated function rather than guessing", () => {
  assert.equal(idBuilder("static void set_json_id(JsonObject &root) { if (x) {"), null);
});

test("masking preserves offsets so slices still line up", () => {
  const src = source('// } brace\nconst char *s = "}";');
  assert.equal(maskCommentsAndLiterals(src).length, src.length);
});

test("collects every emitted event key, and nothing else", () => {
  assert.deepEqual(emittedKeys(source()), ["device", "id", "value"]);
});

// pinnedRef reads the version out of the build workflow, so a change to how the
// workflow spells it breaks this check silently - the nightly job passes --ref
// explicitly and stays green while the pinned job dies. That already happened
// once, when the version stopped being a `client_payload || default` expression
// and became a literal.
test("reads the version the build workflow actually pins", () => {
  const workflow = fs.readFileSync(
    new URL("../.github/workflows/build-esphome.yml", import.meta.url),
    "utf8",
  );
  const declared = workflow.match(/^\s*ESPHOME_VERSION:\s*'(?<version>[^']+)'/m);
  assert.ok(declared, "the build workflow no longer pins a literal version");
  assert.equal(pinnedRef(), declared.groups.version);
  assert.match(pinnedRef(), /^\d{4}\.\d{1,2}\.\d{1,2}$/);
});

// The verdict itself. A live run only reaches most of these branches on the day
// ESPHome changes something, which is the worst moment to find out the reporting
// is wrong, so they are exercised against fixtures instead.

const BASELINE = "baseline-hash";
const KEYS = ["domain", "id", "name"];

const snapshotOf = (overrides = {}) => ({
  requiredKeys: ["id", "domain", "name"],
  ref: "2026.8.1",
  keys: KEYS,
  idBuilder: BASELINE,
  ...overrides,
});

const verdict = (overrides = {}, snapshotOverrides = {}) =>
  evaluate({
    snapshot: snapshotOf(snapshotOverrides),
    ref: "dev",
    keys: KEYS,
    builder: "static void set_json_id(JsonObject root) { }",
    builderHash: BASELINE,
    ...overrides,
  });

test("passes silently when nothing moved", () => {
  assert.deepEqual(verdict(), { failures: [], notes: [] });
});

test("an unrecognized set_json_id() body fails, and says how to settle it", () => {
  const { failures } = verdict({ builderHash: "rewritten" });
  assert.equal(failures.length, 1);
  assert.match(failures[0], /set_json_id\(\) changed/);
  assert.match(failures[0], /--ref dev --accept/);
  assert.match(failures[0], /--ref dev --update/);
});

test("a body recorded as emitting the same ids passes, saying whose verdict that was", () => {
  const accepted = {
    ref: "dev",
    recorded: "2026-09-19",
    idBuilder: "refactored",
    note: "pass by value; ids unchanged",
  };
  const { failures, notes } = verdict(
    { builderHash: "refactored" },
    { alsoAccepted: [accepted] },
  );
  assert.deepEqual(failures, []);
  assert.equal(notes.length, 1);
  assert.match(notes[0], /not the 2026\.8\.1 baseline/);
  assert.match(notes[0], /accepted at dev on 2026-09-19/);
  assert.match(notes[0], /pass by value; ids unchanged/);
});

test("an acceptance covers one body, not every later one", () => {
  const { failures } = verdict(
    { builderHash: "changed-again" },
    { alsoAccepted: [{ ref: "dev", idBuilder: "refactored", note: "ids unchanged" }] },
  );
  assert.equal(failures.length, 1);
  assert.match(failures[0], /set_json_id\(\) changed/);
});

test("a missing set_json_id() is reported, not thrown", () => {
  const { failures } = verdict({ builder: null, builderHash: null });
  assert.equal(failures.length, 1);
  assert.match(failures[0], /set_json_id\(\) is gone/);
});

test("a key the web UI reads going missing fails on its own", () => {
  const { failures } = verdict({ keys: ["domain", "name"] });
  assert.equal(failures.length, 1);
  assert.match(failures[0], /keys the web UI depends on are gone: id/);
});

test("keys coming and going outside requiredKeys are notes, not failures", () => {
  const { failures, notes } = verdict({ keys: ["domain", "id", "name", "sorting_group"] });
  assert.deepEqual(failures, []);
  assert.deepEqual(notes, ["new event keys at dev: sorting_group"]);

  const gone = verdict({ keys: KEYS }, { keys: [...KEYS, "tilt"] });
  assert.deepEqual(gone.failures, []);
  assert.deepEqual(gone.notes, ["event keys gone at dev: tilt"]);
});

test("a snapshot with no accepted variants is not a broken snapshot", () => {
  assert.deepEqual(acceptedVariants(snapshotOf()), []);
  assert.deepEqual(acceptedVariants(snapshotOf({ alsoAccepted: null })), []);
});

// Acceptances are written by --accept, but nothing stops a hand edit, and an
// entry missing its reasoning is the one thing this record cannot do without.
test("every recorded acceptance carries a ref, a hash and its reasoning", () => {
  const snapshot = JSON.parse(
    fs.readFileSync(new URL("./web-server-contract.json", import.meta.url), "utf8"),
  );
  assert.deepEqual(snapshotProblems(snapshot), []);
});

// The rules the CLI applies to a --accept note and the ones the snapshot is held
// to are the same rules, or --accept reports success and writes a record the
// next run refuses.
test("a note has to be reasoning, not a token", () => {
  assert.equal(isReasoning("ids unchanged"), false);
  assert.equal(isReasoning("x".repeat(MIN_NOTE_LENGTH)), false);
  assert.equal(isReasoning(`${"x".repeat(MIN_NOTE_LENGTH)}y`), true);
  assert.equal(isReasoning(`  ${"x".repeat(MIN_NOTE_LENGTH)}  `), false);
  assert.equal(isReasoning(undefined), false);
});

test("an acceptance is reported unusable, field by field", () => {
  const good = {
    ref: "dev",
    idBuilder: "a".repeat(64),
    note: "pass by value only; the id bytes are identical",
  };
  assert.equal(variantProblem(good), null);
  assert.equal(variantProblem(null), "is not an object");
  assert.equal(variantProblem({ ...good, ref: undefined }), "has no ref");
  assert.equal(variantProblem({ ...good, idBuilder: "short" }), "has no idBuilder hash");
  assert.match(variantProblem({ ...good, note: "unchanged" }), /^has no reasoning/);
  assert.deepEqual(snapshotProblems({ alsoAccepted: [good, null] }), [
    "alsoAccepted[1] is not an object",
  ]);
});

// A hand-edited record that reads `null` used to throw here, and an uncaught
// throw exits 1 -- the code the nightly job files an issue on. The verdict has
// to stay a verdict about ESPHome.
test("a malformed acceptance cannot turn the verdict into a crash", () => {
  const { failures } = verdict({ builderHash: "changed" }, { alsoAccepted: [null] });
  assert.equal(failures.length, 1);
  assert.match(failures[0], /set_json_id\(\) changed/);
});
