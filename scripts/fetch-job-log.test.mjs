// The build's own job log is the only place the compiler's output exists, and
// what this prints goes straight into the status document the web builder
// shows. These cover the shaping of it: timestamps and terminal escapes off,
// runner markers out of the way, and the failure tail ending at the error
// rather than at whatever the workflow did afterwards.
//
//   node --test scripts/fetch-job-log.test.mjs

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { after, test } from 'node:test';
import { fileURLToPath } from 'node:url';

const script = fileURLToPath(new URL('./fetch-job-log.sh', import.meta.url));
const directories = [];

after(() => {
  for (const directory of directories) {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

/** Writes a job log the way the Actions API returns one. */
const jobLog = (lines) => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'job-log-'));
  directories.push(directory);
  const file = path.join(directory, 'job.log');
  fs.writeFileSync(
    file,
    lines
      .map((line, index) => {
        const seconds = String(index % 60).padStart(2, '0');
        return `2026-09-09T10:00:${seconds}.1234567Z ${line}\r\n`;
      })
      .join('')
  );
  return file;
};

const run = (sourceFile, env = {}) =>
  execFileSync('bash', [script], {
    env: { ...process.env, LOG_SOURCE_FILE: sourceFile, ...env },
    encoding: 'utf8',
  });

const compile = [
  '##[group]Run esphome/build-action@0000000',
  '[32mINFO[0m Compiling app...',
  '[12/345] Building CXX object src/foo.cpp.o',
  'src/main.cpp:42:1: error: expected primary-expression',
  '##[error]Process completed with exit code 1.',
  '##[group]Run ./scripts/fetch-job-log.sh',
  'Publishing the build failure',
];

test('strips timestamps, terminal escapes and runner markers', () => {
  const printed = run(jobLog(compile));

  assert.equal(
    printed,
    [
      'INFO Compiling app...',
      '[12/345] Building CXX object src/foo.cpp.o',
      'src/main.cpp:42:1: error: expected primary-expression',
      'ERROR: Process completed with exit code 1.',
      'Publishing the build failure',
      '',
    ].join('\n')
  );
});

test('ends the failure tail at the error, not at what the workflow did next', () => {
  const printed = run(jobLog(compile), { LOG_END_AT_ERROR: 'true' });

  assert.ok(
    printed.includes('src/main.cpp:42:1: error: expected primary-expression'),
    'the failure tail dropped the error that explains it'
  );
  assert.ok(
    !printed.includes('Publishing the build failure'),
    'the failure tail kept the workflow steps that ran after the failure'
  );
  assert.ok(printed.trimEnd().endsWith('exit code 1.'));
});

test('starts where the compile does', () => {
  const printed = run(jobLog(compile), {
    LOG_START_AT: '##[group]Run esphome/build-action',
  });

  assert.ok(
    printed.startsWith('INFO Compiling app...'),
    `kept the runner setup that comes before the compile: ${printed}`
  );
});

test('prints nothing until the start marker is there', () => {
  // Falling back to the whole log would move where the published prefix
  // starts the moment the marker appeared, and every segment after that
  // would be a slice of a different log.
  const printed = run(jobLog(compile), { LOG_START_AT: 'nothing matches' });

  assert.equal(printed, '');
});

test('matches the start marker literally, not as a pattern', () => {
  // `awk -v` eats escape sequences, and gawk and mawk disagree about which
  // survive: a regex written to find `[group]` reaches gawk as a character
  // class that cannot match it. The runner has gawk, so this passing under
  // whichever awk is to hand is the point.
  assert.equal(
    run(jobLog(compile), { LOG_START_AT: '##[group]Run esphome/build-.ction' }),
    '',
    'the marker was matched as a pattern rather than compared as text'
  );
});

test('strips the whole CSI form, not just colour codes', () => {
  // Docker's buildx output inside the compile writes sub-parameter forms like
  // `ESC[1:2m`, which a digits-and-semicolons pattern walks past - and what it
  // leaves behind is rendered as text on the page.
  const escape = String.fromCharCode(27);
  const printed = run(
    jobLog([
      `${escape}[31mred${escape}[0m ${escape}[1:2mfancy${escape}[0m ${escape}[38;2;1;2;3mtruecolor${escape}[0m`,
    ])
  );

  assert.equal(printed, 'red fancy truecolor\n');
});

test('holds back a line the runner has not finished writing', () => {
  // sed and awk end their output with a newline whether the input had one or
  // not, so a partial line would otherwise look finished - and read
  // differently on the next fetch, after the rest of it arrived.
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'job-log-'));
  directories.push(directory);
  const file = path.join(directory, 'job.log');
  const stamp = '2026-09-09T10:00:00.1234567Z ';
  fs.writeFileSync(file, `${stamp}[1/2] done\r\n${stamp}[2/2] half`);

  assert.equal(run(file), '[1/2] done\n');

  fs.appendFileSync(file, '-written\r\n');
  assert.equal(run(file), '[1/2] done\n[2/2] half-written\n');
});

test('holds back a line cut inside its timestamp', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'job-log-'));
  directories.push(directory);
  const file = path.join(directory, 'job.log');
  fs.writeFileSync(
    file,
    '2026-09-09T10:00:00.1234567Z done\r\n2026-09-09T10:0'
  );

  assert.equal(run(file), 'done\n');
});

test('keeps only the end of a long log', () => {
  const source = jobLog(
    Array.from({ length: 500 }, (_, index) => `line ${index}`)
  );

  const lines = run(source, { LOG_TAIL_LINES: '5' }).trimEnd().split('\n');

  assert.deepEqual(lines, [
    'line 495',
    'line 496',
    'line 497',
    'line 498',
    'line 499',
  ]);
});

test('caps what it prints in bytes as well as in lines', () => {
  const source = jobLog([`a line that is ${'x'.repeat(200)} long`]);

  const printed = run(source, { LOG_TAIL_LINES: '50', LOG_MAX_BYTES: '32' });

  assert.equal(printed.length, 32);
  assert.ok(printed.endsWith('long\n'));
});

test('fails rather than printing nothing when there is no log to read', () => {
  assert.throws(
    () => run(path.join(os.tmpdir(), 'no-such-job.log')),
    /Command failed/,
    'an unreadable log must exit non-zero, so the caller keeps the last one'
  );
});

test('does not reach for the API without the environment it needs', () => {
  assert.throws(
    () =>
      execFileSync('bash', [script], {
        env: {
          ...process.env,
          LOG_SOURCE_FILE: '',
          GITHUB_TOKEN: '',
          GITHUB_REPOSITORY: 'tomquist/esphome-b2500',
          GITHUB_RUN_ID: '1',
        },
        encoding: 'utf8',
      }),
    /Command failed/
  );
});
