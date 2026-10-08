import { useEffect, useId, useRef, useState } from 'react';

export interface Series { name: string; color: string; points: [number, number][]; dashed?: boolean }

interface Props {
  series: Series[];
  xLabel: string;
  yLabel: string;
  logY?: boolean;
  height?: number;  // fixed pixel height; omit to fill the tile
  xTick?: (x: number) => string;
  yTick?: (y: number) => string;
  yMin?: number;
  bands?: { from: number; to: number; color: string }[];  // shaded x-ranges behind the lines
  yMax?: number;  // fixed top of a linear axis (e.g. 100 for percentages): no padding above it
}

const M = { top: 10, right: 14, bottom: 34, left: 58 };

/** Plain SVG line chart: linear X, linear or log10 Y, legend, native tooltips. No dependencies. */
export function LineChart({ series, xLabel, yLabel, logY = false, height, xTick = x => String(x), yTick = y => String(y), yMin, yMax, bands }: Props) {
  const id = useId();
  const box = useRef<HTMLDivElement>(null);
  // Drawn at the tile's real pixel size so text keeps its size and nothing overflows.
  const [size, setSize] = useState({ w: 520, h: 240 });
  useEffect(() => {
    const el = box.current;
    if (!el) return;
    const measure = () => setSize({ w: Math.max(240, Math.round(el.clientWidth)), h: Math.max(120, Math.round(el.clientHeight)) });
    const ro = new ResizeObserver(measure);
    ro.observe(el);
    measure();
    return () => ro.disconnect();
  }, []);
  const W = size.w;
  const H = height ?? size.h;
  const pts = series.flatMap(s => s.points).filter(([, y]) => (logY ? y > 0 : true));
  if (pts.length === 0) return <div className="chart"><div className="chart-area" ref={box} style={height ? { height } : undefined}><div className="chart-empty">No data yet</div></div></div>;

  const xs = pts.map(p => p[0]);
  const ys = pts.map(p => p[1]);
  const xMin = Math.min(...xs), xMax = Math.max(...xs);
  const fy = (y: number) => (logY ? Math.log10(y) : y);
  let lo = fy(yMin ?? Math.min(...ys)), hi = fy(Math.max(...ys));
  let niceStep = 0;
  if (logY) { lo = Math.floor(lo); hi = Math.ceil(hi); if (hi === lo) hi = lo + 1; }
  else {
    if (yMin === undefined) lo = Math.min(0, lo);
    if (yMax !== undefined) hi = yMax;
    if (hi === lo) hi = lo + 1;
    // Round-number ticks (1, 2, 5 x 10^n), so axes read 0 / 50 / 100 / 150, not 0 / 51 / 102.
    const raw = (hi - lo) / 4, mag = 10 ** Math.floor(Math.log10(raw));
    niceStep = [1, 2, 5, 10].map(m => m * mag).find(st => st >= raw) ?? raw;
    lo = Math.floor(lo / niceStep) * niceStep;
    hi = Math.ceil(hi / niceStep) * niceStep;
  }

  const iw = W - M.left - M.right, ih = H - M.top - M.bottom;
  const px = (x: number) => M.left + (xMax === xMin ? iw / 2 : ((x - xMin) / (xMax - xMin)) * iw);
  const py = (y: number) => M.top + ih - ((fy(y) - lo) / (hi - lo)) * ih;

  const yTicks: number[] = [];
  if (logY) for (let e = lo; e <= hi; e++) yTicks.push(10 ** e);
  else for (let v = lo; v <= hi + niceStep / 2; v += niceStep) yTicks.push(v);
  const xCount = Math.min(xMax - xMin, 8);
  const xTicks = Array.from(new Set(Array.from({ length: xCount + 1 }, (_, i) => Math.round(xMin + ((xMax - xMin) * i) / Math.max(1, xCount)))));

  return (
    <div className="chart">
      <div className="chart-area" ref={box} style={height ? { height } : undefined}>
      <svg width={W} height={H} viewBox={`0 0 ${W} ${H}`} role="img" aria-labelledby={id}>
        <title id={id}>{`${yLabel} by ${xLabel}`}</title>
        {bands?.map((b, i) => (
          <rect key={i} x={px(b.from)} y={M.top} width={Math.max(0, px(b.to) - px(b.from))} height={ih} fill={b.color} opacity={0.16} />
        ))}
        {yTicks.map(t => (
          <g key={t}>
            <line className="grid" x1={M.left} x2={W - M.right} y1={py(t)} y2={py(t)} />
            <text className="tick" x={M.left - 8} y={py(t) + 4} textAnchor="end">{yTick(t)}</text>
          </g>
        ))}
        {xTicks.map(t => (
          <text key={t} className="tick" x={px(t)} y={H - M.bottom + 18} textAnchor="middle">{xTick(t)}</text>
        ))}
        <text className="axis" x={M.left + iw / 2} y={H - 4} textAnchor="middle">{xLabel}</text>
        <text className="axis" transform={`translate(14 ${M.top + ih / 2}) rotate(-90)`} textAnchor="middle">{yLabel}</text>
        {series.map(s => {
          const p = s.points.filter(([, y]) => (logY ? y > 0 : true)).sort((a, b) => a[0] - b[0]);
          if (!p.length) return null;
          return (
            <g key={s.name}>
              <polyline fill="none" stroke={s.color} strokeWidth={2} strokeLinejoin="round" strokeDasharray={s.dashed ? '6 5' : undefined} opacity={s.dashed ? 0.8 : 1} points={p.map(([x, y]) => `${px(x)},${py(y)}`).join(' ')} />
              {p.map(([x, y]) => (
                <circle key={x} cx={px(x)} cy={py(y)} r={3.2} fill={s.dashed ? 'var(--tile)' : s.color} stroke={s.color} strokeWidth={s.dashed ? 1.6 : 0}>
                  <title>{`${s.name}: ${xTick(x)} → ${yTick(y)}`}</title>
                </circle>
              ))}
            </g>
          );
        })}
      </svg>
      </div>
      <ul className="legend">
        {series.map(s => (
          <li key={s.name}><i style={{ background: s.dashed ? 'transparent' : s.color, borderTop: s.dashed ? `3px dotted ${s.color}` : undefined }} />{s.name}</li>
        ))}
      </ul>
    </div>
  );
}
