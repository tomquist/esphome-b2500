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
