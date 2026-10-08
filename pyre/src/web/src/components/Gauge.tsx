// Half-circle gauge with five colour zones and a needle, in the style of the reference dashboard.
const ZONES = ['#f26b6b', '#f9a561', '#f6e27f', '#c5e1a5', '#8bbf6e'];
const CX = 100, CY = 96, R = 78, R_IN = 50;

function polar(r: number, pct: number): [number, number] {
  const a = Math.PI * (1 - pct / 100);  // 0% = left, 100% = right
  return [CX + r * Math.cos(a), CY - r * Math.sin(a)];
}

function zonePath(from: number, to: number): string {
  const [x1, y1] = polar(R, from), [x2, y2] = polar(R, to), [x3, y3] = polar(R_IN, to), [x4, y4] = polar(R_IN, from);
  return `M${x1} ${y1} A${R} ${R} 0 0 1 ${x2} ${y2} L${x3} ${y3} A${R_IN} ${R_IN} 0 0 0 ${x4} ${y4} Z`;
}

export function Gauge({ value, caption }: { value: number | null; caption: string }) {
  const v = value === null ? null : Math.max(0, Math.min(100, value));
  const [nx, ny] = polar(R - 6, v ?? 0);
  return (
    <figure className="gauge">
      <svg viewBox="0 0 200 124" preserveAspectRatio="xMidYMid meet" role="img" aria-label={`${caption}: ${v === null ? 'no data' : `${v.toFixed(0)}%`}`}>
        {ZONES.map((c, i) => <path key={c} d={zonePath(i * 20, (i + 1) * 20)} fill={c} stroke="var(--tile)" strokeWidth="1" />)}
        {[0, 20, 40, 60, 80, 100].map(t => {
          const [x, y] = polar(R + 10, t);
          return <text key={t} className="gauge-tick" x={x} y={y + 3} textAnchor="middle">{t}</text>;
        })}
        {v !== null && <line x1={CX} y1={CY} x2={nx} y2={ny} stroke="var(--ink)" strokeWidth="3.4" strokeLinecap="round" />}
        <circle cx={CX} cy={CY} r="5.5" fill="var(--ink)" />
        <text className="gauge-value" x={CX} y={CY + 22} textAnchor="middle">{v === null ? '–' : `${v.toFixed(0)}%`}</text>
      </svg>
    </figure>
  );
}
