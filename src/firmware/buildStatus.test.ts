import fs from 'fs';
import path from 'path';
import {
  BuildStatus,
  BuildTimeoutError,
  buildStatusUrl,
  firmwareDownloadUrl,
  parseBuildStatus,
  pollBuildStatus,
} from './buildStatus';

// Both object keys are spelled in one shell file and here. A drift means the
// page 404s on a build that actually succeeded - for the status document, that
// it cannot poll at all - so read the producers' copies rather than restating
// them.
const repoFile = (...parts: string[]) =>
  fs.readFileSync(path.join(__dirname, '..', '..', ...parts), 'utf-8');

const escapeForRegExp = (value: string) =>
  value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

describe('the object layout', () => {
  it('matches the key the build workflow uploads the firmware to', () => {
    const uploaded = repoFile(
      '.github',
      'workflows',
      'build-esphome.yml'
    ).match(
      /aws s3 cp \S+ "s3:\/\/\$S3_BUCKET\/firmware\/\$IDENTIFIER(?<suffix>\S*)"/
    );

    expect(uploaded).not.toBeNull();
    expect(firmwareDownloadUrl('a-build')).toMatch(
      new RegExp(
        `/firmware/a-build${escapeForRegExp(uploaded!.groups!.suffix)}$`
      )
    );
  });

  it('matches the key the status document is published to', () => {
    const published = repoFile('scripts', 'publish-build-status.sh').match(
      /s3:\/\/\$\{S3_BUCKET\}\/firmware\/\$\{IDENTIFIER\}(?<suffix>\S*)"/
    );

    expect(published).not.toBeNull();
    expect(buildStatusUrl('a-build')).toMatch(
      new RegExp(
        `/firmware/a-build${escapeForRegExp(published!.groups!.suffix)}$`
      )
    );
  });
});

describe('parseBuildStatus', () => {
  it('maps the workflow status to a build state', () => {
    expect(parseBuildStatus({ status: 'building' })?.state).toBe('building');
    expect(parseBuildStatus({ status: 'queued' })?.state).toBe('building');
    expect(parseBuildStatus({ status: 'success' })?.state).toBe('success');
    expect(parseBuildStatus({ status: 'error' })?.state).toBe('error');
    expect(parseBuildStatus({ status: 'cancelled' })?.state).toBe('error');
  });

  it('reads the optional fields', () => {
    const firmwareUrl = firmwareDownloadUrl('happy-tiny-otter-abc');
    const status = parseBuildStatus({
      status: 'SUCCESS',
      run_url: 'https://github.com/run/1',
      firmware_url: firmwareUrl,
      firmware_name: 'b2500-esp32',
      esphome_version: '2026.8.1',
    });

    expect(status).toEqual({
      state: 'success',
      runUrl: 'https://github.com/run/1',
      message: undefined,
      step: undefined,
      progress: undefined,
      firmwareUrl,
      firmwareName: 'b2500-esp32',
      esphomeVersion: '2026.8.1',
    });
  });

  it('drops URLs that are not https on an expected origin', () => {
    const status = parseBuildStatus({
      status: 'success',
      // eslint-disable-next-line no-script-url
      run_url: 'javascript:alert(1)',
      firmware_url: 'https://evil.example/firmware.zip',
    });

    expect(status?.runUrl).toBeUndefined();
    expect(status?.firmwareUrl).toBeUndefined();
  });

  it('drops a run URL on a look-alike host', () => {
    expect(
      parseBuildStatus({
        status: 'success',
        run_url: 'https://github.com.evil.example/run/1',
      })?.runUrl
    ).toBeUndefined();
    expect(
      parseBuildStatus({
        status: 'success',
        run_url: 'http://github.com/run/1',
      })?.runUrl
    ).toBeUndefined();
  });

  it('reads the build step and its compile progress', () => {
    const status = parseBuildStatus({
      status: 'building',
      step: 'compiling',
      message: 'Compiling the firmware',
      progress: { completed: 842, total: 1505 },
    });

    expect(status?.step).toBe('compiling');
    expect(status?.progress).toEqual({ completed: 842, total: 1505 });
  });

  it('ignores steps and progress it does not understand', () => {
    expect(
      parseBuildStatus({ status: 'building', step: 'polishing' })?.step
    ).toBeUndefined();
    expect(
      parseBuildStatus({ status: 'building', progress: { total: 1505 } })
        ?.progress
    ).toBeUndefined();
  });

  it('drops a total the build can never reach', () => {
    // The workflow counts finished object files against the object files the
    // ninja graph declares; a stale graph must not pin the bar below 100%.
    const status = parseBuildStatus({
      status: 'building',
      progress: { completed: 1600, total: 1505 },
    });

    expect(status?.progress).toEqual({ completed: 1600, total: undefined });
  });

  it('returns null for documents it does not understand', () => {
    expect(parseBuildStatus(null)).toBeNull();
    expect(parseBuildStatus('building')).toBeNull();
    expect(parseBuildStatus({})).toBeNull();
    expect(parseBuildStatus({ status: 'something-else' })).toBeNull();
  });
});

describe('pollBuildStatus', () => {
  const noSleep = () => Promise.resolve();

  it('polls until the build finished', async () => {
    const responses: (BuildStatus | null)[] = [
      null,
      { state: 'building' },
      { state: 'success', firmwareUrl: 'https://example.com/firmware.zip' },
    ];
    const seen: BuildStatus[] = [];

    const result = await pollBuildStatus({
      identifier: 'tiny-happy-cat-1',
      onStatus: (status) => seen.push(status),
      fetchStatus: async () => responses.shift() ?? null,
      sleep: noSleep,
    });

    expect(result.state).toBe('success');
    expect(seen.map((status) => status.state)).toEqual(['building', 'success']);
  });

  it('returns failed builds', async () => {
    const result = await pollBuildStatus({
      identifier: 'tiny-happy-cat-1',
      fetchStatus: async () => ({ state: 'error', message: 'nope' }),
      sleep: noSleep,
    });

    expect(result).toEqual({ state: 'error', message: 'nope' });
  });

  it('reports how often fetching the status failed in a row', async () => {
    const failures: number[] = [];
    let call = 0;

    await pollBuildStatus({
      identifier: 'tiny-happy-cat-1',
      onFetchError: (_error, consecutiveFailures) =>
        failures.push(consecutiveFailures),
      fetchStatus: async () => {
        call += 1;
        if (call <= 2) {
          throw new TypeError('Failed to fetch');
        }
        return { state: 'success' };
      },
      sleep: noSleep,
    });

    expect(failures).toEqual([1, 2]);
  });

  it('ignores transient fetch errors', async () => {
    let call = 0;
    const result = await pollBuildStatus({
      identifier: 'tiny-happy-cat-1',
      fetchStatus: async () => {
        call += 1;
        if (call === 1) {
          throw new TypeError('Failed to fetch');
        }
        return { state: 'success' };
      },
      sleep: noSleep,
    });

    expect(result.state).toBe('success');
  });

  it('gives up on a request that never completes', async () => {
    await expect(
      pollBuildStatus({
        identifier: 'tiny-happy-cat-1',
        timeoutMs: 0,
        requestTimeoutMs: 10,
        fetchStatus: () => new Promise(() => {}),
        sleep: noSleep,
      })
    ).rejects.toBeInstanceOf(BuildTimeoutError);
  });

  it('aborts a request that never completes', async () => {
    const aborted: boolean[] = [];

    await expect(
      pollBuildStatus({
        identifier: 'tiny-happy-cat-1',
        timeoutMs: 0,
        requestTimeoutMs: 10,
        fetchStatus: (_identifier, signal) =>
          new Promise(() => {
            signal?.addEventListener('abort', () => aborted.push(true));
          }),
        sleep: noSleep,
      })
    ).rejects.toBeInstanceOf(BuildTimeoutError);

    expect(aborted).toEqual([true]);
  });

  it('gives up after the timeout', async () => {
    await expect(
      pollBuildStatus({
        identifier: 'tiny-happy-cat-1',
        timeoutMs: 0,
        fetchStatus: async () => null,
        sleep: noSleep,
      })
    ).rejects.toBeInstanceOf(BuildTimeoutError);
  });
});
