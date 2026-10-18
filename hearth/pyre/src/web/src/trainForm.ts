import type { OpKey, Target } from './types';

export type Level = 'quick' | 'normal' | 'deep';

export type Mode = 'recommended' | 'custom' | 'forever';

export interface TrainForm {
  mode: Mode;
  roundPause: number;  // seconds between forever rounds
  level: Level;
  ops: Record<OpKey, boolean>;
  targets: Record<Target, boolean>;
  gpuLoad: number;
  cpuLoad: number;
  pause: string;
  maxWork: string;
  maxProbe: string;
  maxMem: string;
  repeats: string;
  step: string;
  reset: boolean;
  verify: boolean;
}

export const PRESETS: Record<Level, { step: number; repeats: number; maxProbe: number; blurb: string }> = {
  quick: { step: 3, repeats: 1, maxProbe: 0.25, blurb: 'Every 3rd size, 1 run each, stops at 0.25 s probes.' },
  normal: { step: 2, repeats: 2, maxProbe: 1, blurb: 'Every 2nd size, 2 runs each, stops at 1 s probes.' },
  deep: { step: 1, repeats: 3, maxProbe: 3, blurb: 'Every size, 3 runs each, stops at 3 s probes.' },
};

export const DEFAULT_FORM: TrainForm = {
  mode: 'recommended',
  roundPause: 30,
  level: 'normal',
  ops: { matmul: true, chaos: true, mc: true },
  targets: { cpu: true, cuda: true, vulkan: true },
  gpuLoad: 100,
  cpuLoad: 100,
  pause: '',
  maxWork: '',
  maxProbe: '',
  maxMem: '',
  repeats: '',
  step: '',
  reset: false,
  verify: true,
};

const STORE_KEY = 'pyre-train-form';

export function loadForm(): TrainForm {
  try {
    const raw = localStorage.getItem(STORE_KEY);
    if (raw) return { ...DEFAULT_FORM, ...(JSON.parse(raw) as Partial<TrainForm>) };
  } catch { /* fall through to defaults */ }
  return DEFAULT_FORM;
}

export function saveForm(f: TrainForm): void {
  try { localStorage.setItem(STORE_KEY, JSON.stringify(f)); } catch { /* not persisted */ }
}

/** Why the form can't be submitted, or '' when it can. */
export function validate(f: TrainForm): string {
  if (!Object.values(f.ops).some(Boolean)) return 'Pick at least one kernel.';
  if (!Object.values(f.targets).some(Boolean)) return 'Pick at least one target.';
  const nums: [string, string, number, number][] = [
    ['Pause', f.pause, 0, 60000], ['Max work', f.maxWork, 12, 40], ['Max probe time', f.maxProbe, 0.001, 600],
    ['Max memory', f.maxMem, 0, 1e6], ['Repeats', f.repeats, 1, 50], ['Step', f.step, 1, 12],
  ];
  for (const [name, raw, lo, hi] of nums) {
    if (raw === '') continue;
    const v = Number(raw);
    if (!Number.isFinite(v) || v < lo || v > hi) return `${name} must be between ${lo} and ${hi}.`;
  }
  return '';
}

/** The `pyre train` argument list for this form (empty fields keep the preset's value). */
export function toArgs(f: TrainForm): string[] {
  const args = ['--level', f.level];
  const on = <K extends string>(m: Record<K, boolean>) => (Object.keys(m) as K[]).filter(k => m[k]).join(',');
  args.push('--ops', on(f.ops), '--targets', on(f.targets));
  if (f.gpuLoad !== 100) args.push('--gpu-load', String(f.gpuLoad));
  if (f.cpuLoad !== 100) args.push('--cpu-load', String(f.cpuLoad));
  const opt: [string, string][] = [
    ['--pause', f.pause], ['--max-work', f.maxWork], ['--max-probe', f.maxProbe],
    ['--max-mem', f.maxMem], ['--repeats', f.repeats], ['--step', f.step],
  ];
  for (const [flag, v] of opt) if (v !== '') args.push(flag, v);
  if (f.reset) args.push('--reset');
  if (!f.verify) args.push('--no-verify');
  return args;
}

/** `pyre train --forever`: adaptive learning with no end, until stopped. Gentle by default (loads come from the form). */
export function toForeverArgs(f: TrainForm): string[] {
  const on = <K extends string>(m: Record<K, boolean>) => (Object.keys(m) as K[]).filter(k => m[k]).join(',');
  const args = ['--forever', '--level', 'quick', '--ops', on(f.ops), '--targets', on(f.targets), '--round-pause', String(f.roundPause)];
  if (f.gpuLoad !== 100) args.push('--gpu-load', String(f.gpuLoad));
  if (f.cpuLoad !== 100) args.push('--cpu-load', String(f.cpuLoad));
  return args;
}
