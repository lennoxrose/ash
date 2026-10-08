import { TrendNote } from '../components/TrendNote';
import { Tile } from '../components/ui';
import type { Trend } from '../trend';

/** Big-number tile with an optional sparkline of recent values. */
export function KpiTile({ title, value, unit, spark, tone = 'ink', trend, format }: {
  title: string; value: string; unit?: string; spark?: number[]; tone?: 'ink' | 'good' | 'warn' | 'bad';
  trend?: Trend | null; format?: (v: number) => string;
}) {
  return (
    <Tile title={title} className="kpi">
      <div className={`kpi-value tone-${tone}`}>
        {value}
        {unit && <span className="kpi-unit">{unit}</span>}
      </div>
      {spark && spark.length > 1 && <Sparkline values={spark} />}
      {format && <TrendNote t={trend ?? null} format={format} />}
    </Tile>
  );
}

function Sparkline({ values }: { values: number[] }) {
  const w = 160, h = 26, lo = Math.min(...values), hi = Math.max(...values);
  const pts = values.map((v, i) => `${(i / (values.length - 1)) * w},${h - 3 - (hi === lo ? 0.5 : (v - lo) / (hi - lo)) * (h - 6)}`).join(' ');
  return (
    <svg className="spark" viewBox={`0 0 ${w} ${h}`} preserveAspectRatio="none" aria-hidden>
      <polyline points={pts} fill="none" stroke="var(--blue)" strokeWidth="1.8" strokeLinejoin="round" />
    </svg>
  );
}
