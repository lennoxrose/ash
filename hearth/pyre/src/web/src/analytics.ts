import { OPS, TARGETS, type HistoryEntry, type RatePoint, type Target } from './types';

/** Workload sizes grouped into five classes (powers of two of the kernel's work units). */
export const SIZE_CLASSES = [
  { id: 'tiny', label: 'Tiny', from: 0, to: 16 },
  { id: 'small', label: 'Small', from: 17, to: 22 },
  { id: 'medium', label: 'Medium', from: 23, to: 28 },
  { id: 'large', label: 'Large', from: 29, to: 32 },
  { id: 'huge', label: 'Huge', from: 33, to: 99 },
] as const;
export type SizeClass = (typeof SIZE_CLASSES)[number];

/** Geometric mean of the measured speeds of one target inside a size class. */
function classRate(rates: RatePoint[], op: string, target: Target, cls: SizeClass): number | undefined {
  const r = rates.filter(x => x.op === op && x.target === target && x.bucket >= cls.from && x.bucket <= cls.to);
  if (!r.length) return undefined;
  return Math.exp(r.reduce((s, x) => s + Math.log(x.per_second), 0) / r.length);
}

export interface Cell {
  winner: Target | null;
  /** Best GPU speed ÷ CPU speed (> 1: a GPU beats the CPU), when both were measured. */
  speedup?: number;
}

export function cellFor(rates: RatePoint[], op: string, cls: SizeClass): Cell {
  const at: Partial<Record<Target, number>> = {};
  for (const t of TARGETS) {
    const v = classRate(rates, op, t, cls);
    if (v !== undefined) at[t] = v;
  }
  const entries = Object.entries(at) as [Target, number][];
  if (!entries.length) return { winner: null };
  const winner = entries.reduce((a, b) => (b[1] > a[1] ? b : a))[0];
  const gpu = Math.max(at.cuda ?? 0, at.vulkan ?? 0);
  return { winner, speedup: at.cpu && gpu ? gpu / at.cpu : undefined };
}

export function heatRows(rates: RatePoint[]) {
  return OPS.map(o => ({ op: o, cells: SIZE_CLASSES.map(c => cellFor(rates, o.name, c)) }));
}

export interface LatestScore { hits: number; shapes: number; lost_pct: number; error_pct: number | null; time: number }

/** The most recent session that produced a self-check score ("after" if it trained, else "before"). */
export function latestScore(history: HistoryEntry[]): LatestScore | null {
  for (let i = history.length - 1; i >= 0; i--) {
    const s = history[i].after ?? history[i].before;
    if (s && s.shapes) return { ...s, time: history[i].time };
  }
  return null;
}

/** Smallest size bucket from which the best GPU stays faster than the CPU at every larger measured size. */
export function crossover(rates: RatePoint[], op: string): { bucket: number | null; measured: boolean } {
  const buckets = [...new Set(rates.filter(r => r.op === op).map(r => r.bucket))].sort((a, b) => a - b);
  const wins = buckets.map(b => {
    const at = (t: Target) => rates.find(r => r.op === op && r.target === t && r.bucket === b)?.per_second;
    const cpu = at('cpu'), gpu = Math.max(at('cuda') ?? 0, at('vulkan') ?? 0);
    return cpu !== undefined && gpu > 0 ? gpu > cpu : null;  // null: cannot compare at this size
  });
  const comparable = buckets.map((b, i) => ({ b, win: wins[i] })).filter(x => x.win !== null);
  if (!comparable.length) return { bucket: null, measured: false };
  let from: number | null = null;
  for (let i = comparable.length - 1; i >= 0 && comparable[i].win; i--) from = comparable[i].b;
  return { bucket: from, measured: true };
}

/** Number of measured sizes and the smallest/largest, per kernel and target. */
export function coverage(rates: RatePoint[]) {
  return OPS.flatMap(o => TARGETS.map(t => {
    const b = rates.filter(r => r.op === o.name && r.target === t).map(r => r.bucket);
    return { op: o, target: t, count: b.length, min: b.length ? Math.min(...b) : null, max: b.length ? Math.max(...b) : null };
  }));
}

/** Per-session numbers (oldest first) for tendencies: the score each scored session ended with. */
export function sessionSeries(history: HistoryEntry[]) {
  const scored = history.map(h => h.after ?? h.before).filter((s): s is NonNullable<typeof s> => !!s && s.shapes > 0);
  return {
    lost: scored.map(s => s.lost_pct),
    hit: scored.map(s => (s.hits / s.shapes) * 100),
    accuracy: scored.filter(s => s.error_pct !== null).map(s => Math.max(0, 100 - (s.error_pct as number))),
  };
}

/** Which way the GPU-vs-CPU advantage moves as work grows, and (if the GPU never wins yet) where it might start to. */
export function speedupTendency(rates: RatePoint[], op: string): string {
  const pts: [number, number][] = [];
  for (const b of new Set(rates.filter(r => r.op === op).map(r => r.bucket))) {
    const at = (t: Target) => rates.find(r => r.op === op && r.target === t && r.bucket === b)?.per_second;
    const cpu = at('cpu'), gpu = Math.max(at('cuda') ?? 0, at('vulkan') ?? 0);
    if (cpu && gpu) pts.push([b, Math.log2(gpu / cpu)]);
  }
  if (pts.length < 4) return 'needs more sizes measured';
  const recent = pts.sort((a, b) => a[0] - b[0]).slice(-6);
  const n = recent.length, mx = recent.reduce((s, p) => s + p[0], 0) / n, my = recent.reduce((s, p) => s + p[1], 0) / n;
  const slope = recent.reduce((s, p) => s + (p[0] - mx) * (p[1] - my), 0) / recent.reduce((s, p) => s + (p[0] - mx) ** 2, 0);
  if (Math.abs(slope) < 0.03) return '→ GPU advantage flat with size';
  if (slope > 0) return `↗ GPU gains ×2 every ~${Math.round(1 / slope)} sizes`;
  return '↘ GPU advantage shrinking with size';
}
