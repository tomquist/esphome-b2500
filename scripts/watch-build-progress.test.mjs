// The progress watcher publishes build status from a background process while
// the firmware compiles, and the workflow stops it before publishing the build
// result. If it could be stopped mid-upload, the upload it left in flight would
// land a stale "building" status on top of that result and the web builder would
// show compile progress for a finished build until it timed out.
//
//   node --test scripts/watch-build-progress.test.mjs

import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { after, test } from 'node:test';
import { fileURLToPath } from 'node:url';

const watcher = fileURLToPath(
  new URL('./watch-build-progress.sh', import.meta.url)
);
const workspaces = [];

after(() => {
  for (const dir of workspaces) {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

/** A build tree with `objects` compiled of `expected` the ninja graph declares. */
const workspace = ({ objects, expected, publishSeconds = 0 }) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'watch-progress-'));
  workspaces.push(dir);
  const build = path.join(dir, '.esphome', 'build', 'b2500', 'build');
  fs.mkdirSync(build, { recursive: true });

  const edges = Array.from(
    { length: expected },
    (_, index) =>
      `build src/file${index}.c.obj: C_COMPILER__idf src/file${index}.c`
  );
  // Link and archive edges must not be counted as compile units.
  edges.push('build b2500.elf: CXX_EXECUTABLE_LINKER src/file0.c.obj');
  fs.writeFileSync(path.join(build, 'build.ninja'), `${edges.join('\n')}\n`);

  for (let index = 0; index < objects; index += 1) {
    fs.writeFileSync(path.join(build, `file${index}.c.obj`), '');
  }

  // Records when each publish starts and finishes, so a publish that overlaps
  // the stop can be told apart from one that started after it.
  fs.writeFileSync(
    path.join(dir, 'publish-stub.sh'),
    [
      '#!/usr/bin/env bash',
      `echo "start $PROGRESS_DONE/$PROGRESS_TOTAL" >> "${path.join(dir, 'published.log')}"`,
      `sleep ${publishSeconds}`,
      `echo "end $PROGRESS_DONE/$PROGRESS_TOTAL" >> "${path.join(dir, 'published.log')}"`,
    ].join('\n'),
    { mode: 0o755 }
  );

  return {
    dir,
    stopFile: path.join(dir, 'stop'),
    published: () =>
      fs.existsSync(path.join(dir, 'published.log'))
        ? fs
            .readFileSync(path.join(dir, 'published.log'), 'utf8')
            .trim()
            .split('\n')
        : [],
  };
};

const start = (space, { intervalSeconds = 1 } = {}) =>
  spawn('bash', [watcher], {
    cwd: space.dir,
    env: {
      ...process.env,
      PROGRESS_INTERVAL_SECONDS: String(intervalSeconds),
      PROGRESS_STOP_FILE: space.stopFile,
      PROGRESS_PUBLISH_COMMAND: path.join(space.dir, 'publish-stub.sh'),
    },
    stdio: 'ignore',
  });

const waitFor = async (predicate, { timeoutMs = 15000 } = {}) => {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (predicate()) {
      return true;
    }
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  return false;
};

const exited = (child) => new Promise((resolve) => child.once('exit', resolve));

test('publishes the compiled and expected object counts', async () => {
  const space = workspace({ objects: 3, expected: 5 });
  const child = start(space);

  assert.ok(
    await waitFor(() => space.published().length > 0),
    'the watcher published nothing'
  );
  fs.writeFileSync(space.stopFile, '');
  await exited(child);

  assert.deepEqual(space.published(), ['start 3/5', 'end 3/5']);
});

test('finishes an upload that overlaps being stopped, and publishes nothing after', async () => {
  const space = workspace({ objects: 2, expected: 4, publishSeconds: 3 });
  const child = start(space);

  assert.ok(
    await waitFor(() => space.published().includes('start 2/4')),
    'the watcher never started publishing'
  );

  // Stop while that upload is still in flight, the way the workflow does.
  fs.writeFileSync(space.stopFile, '');
  assert.deepEqual(
    space.published(),
    ['start 2/4'],
    'the upload finished too early to test'
  );

  // More objects appear meanwhile: a watcher that kept polling would publish them.
  const build = path.join(space.dir, '.esphome', 'build', 'b2500', 'build');
  fs.writeFileSync(path.join(build, 'file2.c.obj'), '');

  const code = await exited(child);

  assert.equal(code, 0, 'the watcher did not exit cleanly');
  assert.deepEqual(
    space.published(),
    ['start 2/4', 'end 2/4'],
    'the in-flight upload must finish, and nothing may be published after the stop'
  );
});
