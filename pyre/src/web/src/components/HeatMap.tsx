import type { ReactNode } from 'react';

export interface HeatCell { color: string; text?: string; title?: string }

/** Small coloured matrix with row and column labels (the reference's risk-matrix tiles). */
export function HeatMap({ rows, cols, cells, xLabel, legend }: {
  rows: string[]; cols: string[]; cells: HeatCell[][]; xLabel: string; legend?: ReactNode;
}) {
  return (
    <div className="heat">
      <div className="heat-grid" style={{ gridTemplateColumns: `auto repeat(${cols.length}, minmax(0, 1fr))`, gridTemplateRows: `auto repeat(${rows.length}, minmax(26px, 1fr))` }}>
        <span />
        {cols.map(c => <span key={c} className="heat-col">{c}</span>)}
        {rows.map((r, i) => (
          <div key={r} style={{ display: 'contents' }}>
            <span className="heat-row">{r}</span>
            {cells[i].map((c, j) => (
              <span key={j} className="heat-cell" style={{ background: c.color }} title={c.title}>{c.text}</span>
            ))}
          </div>
        ))}
      </div>
      <div className="heat-x">{xLabel}</div>
      {legend && <div className="heat-legend">{legend}</div>}
    </div>
  );
}
