import {
  BuildStatus,
  BuildTimeoutError,
  parseBuildStatus,
  pollBuildStatus,
} from './buildStatus';

describe('parseBuildStatus', () => {
  it('maps the workflow status to a build state', () => {
    expect(parseBuildStatus({ status: 'building' })?.state).toBe('building');
    expect(parseBuildStatus({ status: 'queued' })?.state).toBe('building');
    expect(parseBuildStatus({ status: 'success' })?.state).toBe('success');
    expect(parseBuildStatus({ status: 'error' })?.state).toBe('error');
    expect(parseBuildStatus({ status: 'cancelled' })?.state).toBe('error');
  });

  it('reads the optional fields', () => {
    const status = parseBuildStatus({
      status: 'SUCCESS',
      run_url: 'https://github.com/run/1',
      firmware_url: 'https://example.com/firmware.zip',
      firmware_name: 'b2500-esp32',
      esphome_version: '2026.8.1',
    });

    expect(status).toEqual({
      state: 'success',
      runUrl: 'https://github.com/run/1',
      message: undefined,
      firmwareUrl: 'https://example.com/firmware.zip',
      firmwareName: 'b2500-esp32',
      esphomeVersion: '2026.8.1',
    });
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
