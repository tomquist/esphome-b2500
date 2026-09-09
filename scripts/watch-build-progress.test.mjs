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

  // Aged, so "the newest object" is unambiguous in the tests that care. Files
  // written in one go land in the same clock tick, where the tie-break is the
  // name - true to the build, but not what those tests are about.
  const aged = new Date(Date.now() - 60_000);
  for (let index = 0; index < objects; index += 1) {
    const object = path.join(build, `file${index}.c.obj`);
    fs.writeFileSync(object, '');
    fs.utimesSync(object, aged, aged);
  }

  // Records when each publish starts and finishes, so a publish that overlaps
  // the stop can be told apart from one that started after it, and what build
  // output each one carried.
  fs.writeFileSync(
    path.join(dir, 'publish-stub.sh'),
    [
      '#!/usr/bin/env bash',
      `echo "start $PROGRESS_DONE/$PROGRESS_TOTAL" >> "${path.join(dir, 'published.log')}"`,
      `echo "current ${'$'}{PROGRESS_CURRENT:-none} segments ${'$'}{LOG_SEGMENTS:-none}" >> "${path.join(dir, 'published-current.log')}"`,
      `sleep ${publishSeconds}`,
      `echo "end $PROGRESS_DONE/$PROGRESS_TOTAL" >> "${path.join(dir, 'published.log')}"`,
    ].join('\n'),
    { mode: 0o755 }
  );

  const lines = (name) =>
    fs.existsSync(path.join(dir, name))
      ? fs.readFileSync(path.join(dir, name), 'utf8').trim().split('\n')
      : [];

  // Stands in for publish-build-log.sh: reports how many segments it has
  // published, and fails while there is nothing to read - the way a build
  // whose output file does not exist yet behaves.
  fs.writeFileSync(
    path.join(dir, 'log-stub.sh'),
    [
      '#!/usr/bin/env bash',
      `[[ -s "${path.join(dir, 'segments')}" ]] || { echo "no build output yet" >&2; exit 1; }`,
      `cat "${path.join(dir, 'segments')}"`,
    ].join('\n'),
    { mode: 0o755 }
  );

  return {
    dir,
    stopFile: path.join(dir, 'stop'),
    build,
    segmentsFile: path.join(dir, 'segments'),
    published: () => lines('published.log'),
    publishedCurrent: () => lines('published-current.log'),
  };
};

// Publishing the build output is off unless a test asks for it: it is best
// effort in the workflow too, and the counting has to hold up without it.
const start = (space, { intervalSeconds = 1, logIntervalSeconds = 0 } = {}) =>
  spawn('bash', [watcher], {
    cwd: space.dir,
    env: {
      ...process.env,
      PROGRESS_INTERVAL_SECONDS: String(intervalSeconds),
      PROGRESS_STOP_FILE: space.stopFile,
      PROGRESS_PUBLISH_COMMAND: path.join(space.dir, 'publish-stub.sh'),
      PROGRESS_LOG_COMMAND: path.join(space.dir, 'log-stub.sh'),
      PROGRESS_LOG_INTERVAL_SECONDS: String(logIntervalSeconds),
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

test('names the unit the compiler is on, and republishes when it moves', async () => {
  const space = workspace({ objects: 2, expected: 4 });
  fs.writeFileSync(path.join(space.build, 'sha256.c.obj'), '');
  const child = start(space);

  const named = await waitFor(() =>
    space.publishedCurrent().some((line) => line.startsWith('current sha256.c'))
  );

  // A unit finishing between two polls moves the name as well as the count.
  fs.writeFileSync(path.join(space.build, 'wifi_component.cpp.o'), '');
  const moved = await waitFor(() =>
    space
      .publishedCurrent()
      .some((line) => line.startsWith('current wifi_component.cpp'))
  );

  fs.writeFileSync(space.stopFile, '');
  await exited(child);

  assert.ok(
    named,
    `never named the newest object: ${space.publishedCurrent()}`
  );
  assert.ok(moved, 'the name did not follow the object written after it');
});

test('reports no current unit before the first object lands', async () => {
  // Nothing is published at all until there is something to say: the counts
  // are zero and there is no name, which is the configure phase.
  const space = workspace({ objects: 0, expected: 4 });
  const child = start(space);

  const publishedEarly = await waitFor(() => space.published().length > 0, {
    timeoutMs: 2500,
  });
  fs.writeFileSync(path.join(space.build, 'first.c.obj'), '');
  const publishedLater = await waitFor(() =>
    space.publishedCurrent().some((line) => line.startsWith('current first.c'))
  );

  fs.writeFileSync(space.stopFile, '');
  await exited(child);

  assert.equal(publishedEarly, false, 'published a status with nothing to say');
  assert.ok(publishedLater, 'never reported the first object once it appeared');
});

test('republishes when a new build output segment appears', async () => {
  const space = workspace({ objects: 2, expected: 4 });
  fs.writeFileSync(space.segmentsFile, '1\n');
  const child = start(space, { logIntervalSeconds: 1 });

  const announced = await waitFor(() =>
    space.publishedCurrent().some((line) => line.endsWith('segments 1'))
  );
  // The counts stand still through a link step, which is exactly when the
  // output is the only thing left to show.
  fs.writeFileSync(space.segmentsFile, '2\n');
  const republished = await waitFor(() =>
    space.publishedCurrent().some((line) => line.endsWith('segments 2'))
  );

  fs.writeFileSync(space.stopFile, '');
  await exited(child);

  assert.ok(announced, 'the watcher never published a segment count');
  assert.ok(republished, 'the watcher sat on a segment it had just published');
});

test('keeps reporting progress when the build output cannot be published', async () => {
  // No segment count at all, so the stub fails the way publish-build-log.sh
  // does before the build step has written anything.
  const space = workspace({ objects: 3, expected: 6 });
  const child = start(space, { logIntervalSeconds: 1 });

  const reported = await waitFor(() => space.published().includes('end 3/6'));

  fs.writeFileSync(space.stopFile, '');
  await exited(child);

  assert.ok(
    reported,
    'a build whose output cannot be published stopped reporting progress'
  );
  assert.ok(
    space.publishedCurrent().every((line) => line.endsWith('segments 0'))
  );
});
