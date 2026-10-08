export function fmtRate(perSecond: number): string {
  const units = [[1e12, 'T'], [1e9, 'G'], [1e6, 'M'], [1e3, 'k']] as const;
  for (const [v, s] of units) if (perSecond >= v) return `${(perSecond / v).toFixed(perSecond / v >= 100 ? 0 : 1)} ${s}`;
  return perSecond.toFixed(0);
}

export function fmtMs(ms: number): string {
  if (ms >= 1000) return `${(ms / 1000).toFixed(2)} s`;
  if (ms >= 1) return `${ms.toFixed(ms >= 100 ? 0 : 1)} ms`;
  return `${(ms * 1000).toFixed(0)} µs`;
}

export function fmtPct(v: number | null | undefined): string {
  if (v === null || v === undefined) return 'n/a';
  return `${v >= 100 ? v.toFixed(0) : v.toFixed(1)}%`;
}

export function fmtDuration(seconds: number): string {
  if (seconds < 60) return `${seconds.toFixed(1)} s`;
  return `${Math.floor(seconds / 60)} min ${Math.round(seconds % 60)} s`;
}

export function fmtDate(epoch: number): string {
  return new Date(epoch * 1000).toLocaleString(undefined, { dateStyle: 'medium', timeStyle: 'short' });
}

/** Axis label for a percentage that may be tiny (0.04) or large (208). */
export function pctTick(v: number): string {
  if (v === 0) return '0%';
  if (Math.abs(v) < 1) return `${v.toFixed(2)}%`;
  if (Math.abs(v) < 10) return `${v.toFixed(1)}%`;
  return `${v.toFixed(0)}%`;
}
