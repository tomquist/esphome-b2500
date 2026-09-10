// The build step tees the compiler's output to a file; this is what turns that
// file into the bytes the page is shown. What matters is that it prints whole
// lines only - publish-build-log.sh sends the difference since last time, so a
// line that reads differently once the rest of it arrives corrupts everything
// published after it.
//
//   node --test scripts/read-build-log.test.mjs

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { after, test } from 'node:test';
import { fileURLToPath } from 'node:url';

const script = fileURLToPath(new URL('./read-build-log.sh', import.meta.url));
const directories = [];

after(() => {
  for (const directory of directories) {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

/** A log file as the build step tees it. */
const logFile = (text) => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'build-log-'));
  directories.push(directory);
  const file = path.join(directory, 'build.log');
  fs.writeFileSync(file, text);
  return file;
};

const run = (file, env = {}) =>
  execFileSync('bash', [script], {
    env: { ...process.env, BUILD_LOG_FILE: file, ...env },
    encoding: 'utf8',
  });

const escape = String.fromCharCode(27);

test('keeps the group titles and drops the machinery around them', () => {
  // entrypoint.py writes workflow commands to its own stdout, so they reach
  // the file in their `::` form rather than as the runner renders them.
  const printed = run(
    logFile(
      [
        '::group::Compile firmware',
        'INFO Compiling app',
        '::endgroup::',
        '::notice::something',
        '::warning::a warning',
        '',
      ].join('\n')
    )
  );

  assert.equal(
    printed,
    ['Compile firmware', 'INFO Compiling app', 'WARNING: a warning', ''].join(
      '\n'
    )
  );
});

test('strips the whole CSI form, not just colour codes', () => {
  const printed = run(
    logFile(
      `${escape}[31mred${escape}[0m ${escape}[1:2mfancy${escape}[0m ${escape}[38;2;1;2;3mtrue${escape}[0m\n`
    )
  );

  assert.equal(printed, 'red fancy true\n');
});

test('gives each of ninja\'s progress updates its own line', () => {
  // Ninja means its updates to overwrite one another on a terminal, so it ends
  // them with CR and writes no newline until the phase does. Dropping the CRs
  // would leave one line per phase, thousands of characters wide.
  const printed = run(
    logFile('[1/3] Building a.c.obj\r[2/3] Building b.c.obj\r[3/3] Linking\n')
  );

  assert.deepEqual(printed.trimEnd().split('\n'), [
    '[1/3] Building a.c.obj',
    '[2/3] Building b.c.obj',
    '[3/3] Linking',
  ]);
});

test('publishes an update the moment it is overwritten, not when the phase ends', () => {
  // A phase can compile for minutes without writing a newline. Waiting for one
  // is what made the output arrive in lumps rather than as the build ran.
  const file = logFile('[1/2] Building a.c.obj\r');

  assert.equal(run(file), '[1/2] Building a.c.obj\n');

  fs.appendFileSync(file, '[2/2] Building b.c');
  assert.equal(
    run(file),
    '[1/2] Building a.c.obj\n',
    'the update still being written must wait for its terminator'
  );

  fs.appendFileSync(file, 'pp.obj\r');
  assert.deepEqual(run(file).trimEnd().split('\n'), [
    '[1/2] Building a.c.obj',
    '[2/2] Building b.cpp.obj',
  ]);
});

test('holds back a line the compiler has not finished writing', () => {
  const file = logFile('[1/2] done\n[2/2] half');

  assert.equal(run(file), '[1/2] done\n');

  fs.appendFileSync(file, '-written\n');
  assert.equal(run(file), '[1/2] done\n[2/2] half-written\n');
});

test('ends at the error, not at what the workflow did next', () => {
  const printed = run(
    logFile(
      [
        'src/main.cpp:42:1: error: expected primary-expression',
        '::error::Compiling the firmware failed',
        'Packaging the firmware',
        '',
      ].join('\n')
    ),
    { LOG_END_AT_ERROR: 'true' }
  );

  assert.ok(printed.includes('src/main.cpp:42:1: error:'));
  assert.ok(printed.trimEnd().endsWith('ERROR: Compiling the firmware failed'));
  assert.ok(!printed.includes('Packaging'));
});

test('keeps only the end when asked for an excerpt', () => {
  const file = logFile(
    `${Array.from({ length: 500 }, (_, index) => `line ${index}`).join('\n')}\n`
  );

  assert.deepEqual(run(file, { LOG_TAIL_LINES: '2' }).trimEnd().split('\n'), [
    'line 498',
    'line 499',
  ]);
});

test('fails rather than printing nothing when there is no log yet', () => {
  // The file does not exist until the build step has started writing it, and
  // the caller keeps what it already published rather than treating an absent
  // log as an empty one.
  assert.throws(
    () => run(path.join(os.tmpdir(), 'no-such-build.log')),
    /Command failed/
  );
  assert.throws(() => run(logFile('')), /Command failed/);
});
