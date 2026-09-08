// Fixtures for the contract checker's source extraction.
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
  emittedKeys,
  idBuilder,
  maskCommentsAndLiterals,
  pinnedRef,
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
