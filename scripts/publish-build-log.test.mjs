// S3 cannot append, so the build output is published as immutable segments and
// the status document only says how many exist. These cover the property the
// whole scheme rests on: every byte the compiler wrote is uploaded exactly
// once, in order, and the count printed never names a segment that is not in
// the bucket.
//
//   node --test scripts/publish-build-log.test.mjs

import assert from 'node:assert/strict';
import { execFileSync, spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { after, test } from 'node:test';
import { fileURLToPath } from 'node:url';

const script = fileURLToPath(
  new URL('./publish-build-log.sh', import.meta.url)
);
const directories = [];

after(() => {
  for (const directory of directories) {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

const workspace = ({ uploadFails = false } = {}) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'publish-log-'));
  directories.push(dir);
  const uploads = path.join(dir, 'uploads');
  fs.mkdirSync(uploads);

  // Stands in for the AWS CLI: records what was uploaded under the key it was
  // uploaded to. Invoked as `aws s3 cp <file> <key> --content-type ...`.
  fs.mkdirSync(path.join(dir, 'bin'));
  fs.writeFileSync(
    path.join(dir, 'bin', 'aws'),
    [
      '#!/usr/bin/env bash',
      `${uploadFails ? 'exit 1' : ''}`,
      `cp "$3" "${uploads}/$(basename "$4")"`,
    ].join('\n'),
    { mode: 0o755 }
  );

  // Stands in for fetch-job-log.sh: prints the log as it stands right now.
  fs.writeFileSync(
    path.join(dir, 'fetch-stub.sh'),
    [
      '#!/usr/bin/env bash',
      `[[ -e "${path.join(dir, 'job.log')}" ]] || exit 1`,
      `cat "${path.join(dir, 'job.log')}"`,
    ].join('\n'),
    { mode: 0o755 }
  );

  return {
    dir,
    logFile: path.join(dir, 'job.log'),
    write: (text) => fs.writeFileSync(path.join(dir, 'job.log'), text),
    /** The segment bodies that reached the bucket, in order. */
    uploaded: () =>
      fs
        .readdirSync(uploads)
        .sort(
          (a, b) => Number(a.split('.log.')[1]) - Number(b.split('.log.')[1])
        )
        .map((name) => [
          name,
          fs.readFileSync(path.join(uploads, name), 'utf8'),
        ]),
  };
};

/** Runs one publish and returns the segment count it printed. */
const publish = (space, env = {}) =>
  execFileSync('bash', [script], {
    cwd: space.dir,
    env: {
      ...process.env,
      PATH: `${path.join(space.dir, 'bin')}:${process.env.PATH}`,
      IDENTIFIER: 'happy-tiny-otter-abc',
      S3_BUCKET: 'a-bucket',
      LOG_FETCH_COMMAND: path.join(space.dir, 'fetch-stub.sh'),
      LOG_STATE_PREFIX: path.join(space.dir, 'state'),
      ...env,
    },
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'ignore'],
  }).trim();

test('uploads each byte of the build output exactly once, in order', () => {
  const space = workspace();

  space.write('Compiling app\n');
  assert.equal(publish(space), '1');

  space.write('Compiling app\nLinking b2500.elf\n');
  assert.equal(publish(space), '2');

  assert.deepEqual(space.uploaded(), [
    ['happy-tiny-otter-abc.log.0', 'Compiling app\n'],
    ['happy-tiny-otter-abc.log.1', 'Linking b2500.elf\n'],
  ]);
});

test('publishes nothing when the build has not said anything new', () => {
  const space = workspace();
  space.write('Compiling app\n');

  assert.equal(publish(space), '1');
  assert.equal(publish(space), '1', 'the count must not move on its own');

  assert.equal(space.uploaded().length, 1);
});

test('reports the segments that exist when there is no log to read', () => {
  const space = workspace();

  assert.equal(publish(space), '0');

  space.write('Compiling app\n');
  assert.equal(publish(space), '1');

  // The API stops serving the log mid-build: what was published stays.
  fs.rmSync(space.logFile);
  assert.equal(publish(space), '1');
  assert.equal(space.uploaded().length, 1);
});

test('skips a read that came back shorter than what was published', () => {
  const space = workspace();
  space.write('Compiling app\nLinking b2500.elf\n');
  assert.equal(publish(space), '1');

  // Offsets are only meaningful while the log grows. A short read is a bad
  // read, not a signal to start over on top of what the page already has.
  space.write('Compiling\n');
  assert.equal(publish(space), '1');

  assert.deepEqual(space.uploaded(), [
    ['happy-tiny-otter-abc.log.0', 'Compiling app\nLinking b2500.elf\n'],
  ]);
});

test('skips a read that is not the log it published before', () => {
  const space = workspace();
  space.write('Compiling app\nLinking b2500.elf\n');
  assert.equal(publish(space), '1');

  // What a moved start marker looks like: same shape, longer than what was
  // published, different bytes. Publishing the difference would splice two
  // unrelated logs together in the page.
  space.write('Installing Python\nCompiling app\nLinking b2500.elf\n');
  assert.equal(publish(space), '1');

  assert.deepEqual(space.uploaded(), [
    ['happy-tiny-otter-abc.log.0', 'Compiling app\nLinking b2500.elf\n'],
  ]);
});

test('sends the same bytes again when the upload failed', () => {
  const failing = workspace({ uploadFails: true });
  failing.write('Compiling app\n');

  assert.equal(publish(failing), '0', 'a failed upload must not be counted');
  assert.equal(failing.uploaded().length, 0);

  // Same state directory, working CLI: the bytes that never landed go out now.
  fs.writeFileSync(
    path.join(failing.dir, 'bin', 'aws'),
    [
      '#!/usr/bin/env bash',
      `cp "$3" "${path.join(failing.dir, 'uploads')}/$(basename "$4")"`,
    ].join('\n'),
    { mode: 0o755 }
  );

  assert.equal(publish(failing), '1');
  assert.deepEqual(failing.uploaded(), [
    ['happy-tiny-otter-abc.log.0', 'Compiling app\n'],
  ]);
});

test('stops publishing a build that prints without end', () => {
  const space = workspace();
  space.write('x'.repeat(100));
  assert.equal(publish(space, { LOG_MAX_TOTAL_BYTES: '50' }), '1');

  space.write('x'.repeat(200));
  assert.equal(publish(space, { LOG_MAX_TOTAL_BYTES: '50' }), '1');

  assert.equal(space.uploaded().length, 1);
});

test('publishes nothing while another publisher holds the lock', () => {
  // The workflow's final flush can start while a publisher the watcher left
  // behind is still uploading. Both would write the same segment key, and
  // whichever finished last would decide what it holds.
  const space = workspace();
  space.write('Compiling app\n');

  const holder = spawn(
    'flock',
    [path.join(space.dir, 'state.lock'), 'sleep', '5'],
    { stdio: 'ignore' }
  );
  try {
    // Give flock a moment to actually take it before racing it.
    execFileSync('bash', ['-c', 'sleep 0.5']);
    assert.equal(publish(space, { LOG_LOCK_WAIT_SECONDS: '1' }), '0');
    assert.equal(space.uploaded().length, 0);
  } finally {
    holder.kill();
  }

  // Once it is free the same bytes go out, exactly once.
  assert.equal(publish(space), '1');
  assert.deepEqual(space.uploaded(), [
    ['happy-tiny-otter-abc.log.0', 'Compiling app\n'],
  ]);
});

test('publishes nothing without a usable identifier or bucket', () => {
  const space = workspace();
  space.write('Compiling app\n');

  assert.equal(publish(space, { IDENTIFIER: '../elsewhere' }), '0');
  assert.equal(publish(space, { S3_BUCKET: '' }), '0');
  assert.equal(space.uploaded().length, 0);
});
