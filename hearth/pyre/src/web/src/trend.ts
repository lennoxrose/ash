// "Tendency": which way a measure is heading, and a guess at where it lands next.
// Plain least-squares line through the most recent points. It is a guess, so it is only
// offered with at least three points and is always labelled as an estimate in the UI.

export type Direction = 'improving' | 'worsening' | 'steady';

export interface Trend {
  direction: Direction;
  /** Change per step (per session / per round) along the fitted line. */
  slope: number;
  /** Where the line says the next value will be (clamped to the metric's range). */
  next: number;
  /** The fitted line continued `steps` points past the last value. */
  forecast: (steps: number) => number[];
  points: number;
}

const WINDOW = 8;

export function trend(values: number[], betterIs: 'lower' | 'higher', range: [number, number] = [0, Infinity]): Trend | null {
  const y = values.filter(Number.isFinite).slice(-WINDOW);
  const n = y.length;
  if (n < 3) return null;

  const mx = (n - 1) / 2;
  const my = y.reduce((a, b) => a + b, 0) / n;
  let sxy = 0, sxx = 0;
  y.forEach((v, i) => { sxy += (i - mx) * (v - my); sxx += (i - mx) ** 2; });
  const slope = sxy / sxx;
  const at = (i: number) => Math.min(range[1], Math.max(range[0], my + slope * (i - mx)));

  // "Steady" when the line moves less than 3% of the typical size per step (and under 0.2 absolute).
  const scale = Math.max(Math.abs(my), 1);
  const steady = Math.abs(slope) < Math.max(0.03 * scale, 0.2);
  const direction: Direction = steady ? 'steady' : (slope < 0) === (betterIs === 'lower') ? 'improving' : 'worsening';

  return { direction, slope, next: at(n), points: n, forecast: steps => Array.from({ length: steps }, (_, k) => at(n + k)) };
}

/** Moving average (window 3) to calm noisy per-round numbers. */
export function smooth(values: number[]): number[] {
  return values.map((_, i) => {
    const w = values.slice(Math.max(0, i - 1), i + 2);
    return w.reduce((a, b) => a + b, 0) / w.length;
  });
}
