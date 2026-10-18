import { OPS, TARGETS, type HistoryEntry, type Info } from './types';
import { latestScore } from './analytics';

export interface Recommendation {
  /** `pyre train` arguments for the recommended session. */
  args: string[];
  headline: string;
  reasons: string[];
}

/** Sizes a kernel is expected to cover when it is well learned (2^12 .. about 2^34, every other size). */
const EXPECTED_SIZES = 10;

/**
 * Picks a session from what is known right now: which kernels have the least data, which targets
 * exist on this machine, and how the last self-check went. It always runs the adaptive learner
 * (`--auto`), which then finds the weak sizes by itself; this only chooses the depth and scope.
 */
export function recommend(info: Info | null, history: HistoryEntry[]): Recommendation {
  const rates = info?.rates ?? [];
  const targets = TARGETS.filter(t => t === 'cpu' || info?.targets.find(x => x.name === t)?.present);

  const coverage = OPS.map(o => ({
    op: o,
    sizes: new Set(rates.filter(r => r.op === o.name).map(r => r.bucket)).size,
  }));
  const learnedAnything = rates.length > 0;
  const thin = coverage.filter(c => c.sizes < EXPECTED_SIZES);
  const focus = (thin.length ? thin : coverage).map(c => c.op);

  const score = latestScore(history);
  const reasons: string[] = [];
  let level: 'quick' | 'normal' | 'deep' = 'normal';

  if (!learnedAnything) {
    reasons.push('Nothing is learned on this machine yet: it starts with a grid of probes, then switches to practising where it is weakest.');
  } else if (score && (score.lost_pct > 25 || (score.error_pct ?? 0) > 40)) {
    level = 'deep';
    reasons.push(`The last self-check lost ${score.lost_pct.toFixed(1)}% of time to wrong placement (prediction error ${score.error_pct === null ? 'n/a' : `${score.error_pct.toFixed(0)}%`}), so it practises more per round.`);
  } else if (score && score.lost_pct < 2 && (score.error_pct ?? 100) < 20) {
    level = 'quick';
    reasons.push(`Decisions are already sharp (${score.lost_pct.toFixed(1)}% time lost), so this is a light tune-up.`);
  } else {
    reasons.push('Decisions are decent but not sharp: a normal-depth adaptive pass.');
  }

  if (thin.length && learnedAnything) {
    reasons.push(`Least data so far: ${thin.map(c => `${c.op.label} (${c.sizes} sizes)`).join(', ')}.`);
  }
  const missing = TARGETS.filter(t => !targets.includes(t));
  reasons.push(`Targets: ${targets.map(t => t.toUpperCase()).join(', ')}${missing.length ? `. Not seen on this machine: ${missing.map(t => t.toUpperCase()).join(', ')}.` : '.'}`);
  reasons.push('Runs at full speed to learn fastest (lower the loads under Custom if you need the machine). It stops by itself once decisions stop improving.');

  return {
    headline: level === 'deep' ? 'Deep adaptive session' : level === 'quick' ? 'Quick tune-up' : 'Adaptive session',
    reasons,
    args: ['--auto', '--level', level, '--ops', focus.map(o => o.key).join(','), '--targets', targets.join(','), '--rounds', level === 'deep' ? '10' : '6'],
  };
}
