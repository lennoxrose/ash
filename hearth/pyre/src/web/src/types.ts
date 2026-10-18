export type Target = 'cpu' | 'cuda' | 'vulkan';
export const TARGETS: Target[] = ['cpu', 'cuda', 'vulkan'];
export type OpKey = 'matmul' | 'chaos' | 'mc';

// Kernel names as `pyre` reports them, with display labels and work units.
export const OPS: { name: string; key: OpKey; label: string; short: string; unit: string }[] = [
  { name: 'matrix_multiply', key: 'matmul', label: 'Matrix multiply', short: 'MatMul', unit: 'mul-add/s' },
  { name: 'chaos_iterate', key: 'chaos', label: 'Chaos iterate', short: 'Chaos', unit: 'element-iter/s' },
  { name: 'monte_carlo_risk', key: 'mc', label: 'Monte Carlo', short: 'Monte Carlo', unit: 'iter/s' },
];

export interface InfoTarget { name: Target; present: number; device: string; init_ms: number }
export interface RatePoint { op: string; target: Target; bucket: number; per_second: number }
export interface Info {
  event: 'info';
  profile: string;
  cpu_threads: number;
  cuda_built: number;
  targets: InfoTarget[];
  rates: RatePoint[];
}

export interface ProbeEvent { event: 'probe'; op: string; target: Target; exp: number; shape: string; ms: number }
export interface EvalRow {
  event: 'eval_row';
  phase: string;
  op: string;
  shape: string;
  times_ms: Partial<Record<Target, number>>;
  picked: Target;
  predicted_ms?: number;
  right: boolean;
}
export interface ScoreEvent {
  event: 'score';
  phase: string;
  shapes: number;
  hits: number;
  lost_pct: number;
  error_pct: number | null;
}
export interface RoundEvent {
  event: 'round';
  round: number;
  lost_pct: number;
  error_pct: number | null;
  hits: number;
  shapes: number;
  accepted: boolean;
  probes: number;
  cost_before: number;
  cost_after: number;
  focus: string[];
}
export interface ModeEvent { event: 'mode'; mode: 'auto' | 'forever' }
export interface IdleEvent { event: 'idle'; seconds: number }
export interface ConvergedEvent { event: 'converged'; round: number }
export interface PhaseEvent { event: 'phase'; name: string }
export interface VerdictEvent { event: 'verdict'; verdict: 'better' | 'worse' | 'same' | 'unknown' | 'unverified'; discarded: boolean }
export interface DoneEvent { event: 'done'; probes: number; seconds: number; saved: boolean }
export interface LogEvent { event: 'log'; message: string }
export interface SkippedEvent { event: 'skipped'; target: Target }
export type PyreEvent = ProbeEvent | EvalRow | ScoreEvent | PhaseEvent | VerdictEvent | DoneEvent | LogEvent | SkippedEvent | RoundEvent | ModeEvent | IdleEvent | ConvergedEvent;

export interface JobStatus {
  running: boolean;
  cmd: string;
  started: number;
  exit: number | null;
  next: number;
  lines: PyreEvent[];
}

export interface ScoreSummary { shapes: number; hits: number; lost_pct: number; error_pct: number | null }
export interface HistoryEntry {
  time: number;
  kind: 'train' | 'check';
  level: string;
  verdict: string;
  probes: number;
  seconds: number;
  gpu_load: number;
  cpu_load: number;
  step: number;
  repeats: number;
  before?: ScoreSummary;
  after?: ScoreSummary;
}
