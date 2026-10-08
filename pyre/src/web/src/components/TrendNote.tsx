import type { Trend } from '../trend';

const GLYPH = { improving: '↗', worsening: '↘', steady: '→' } as const;
const WORD = { improving: 'Improving', worsening: 'Getting worse', steady: 'Holding steady' } as const;

/** One line: which way a measure is heading and where the fitted line says it goes next (a guess). */
export function TrendNote({ t, format }: { t: Trend | null; format: (v: number) => string }) {
  if (!t) return <div className="trend trend-none" title="Needs at least three sessions to guess a direction">Tendency: needs 3+ sessions</div>;
  const tone = t.direction === 'improving' ? 'good' : t.direction === 'worsening' ? 'bad' : 'neutral';
  return (
    <div className={`trend trend-${tone}`} title={`Straight line through the last ${t.points} values; an estimate, not a promise`}>
      <span aria-hidden>{GLYPH[t.direction]}</span> {WORD[t.direction]} · next ≈ {format(t.next)}
    </div>
  );
}
