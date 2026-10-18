import { TARGET_COLOR } from '../colors';
import { HeatMap, type HeatCell } from '../components/HeatMap';
import { Tile } from '../components/ui';
import { SIZE_CLASSES, heatRows } from '../analytics';
import type { Info, Target } from '../types';

const SHORT: Record<string, string> = { matrix_multiply: 'MatMul', chaos_iterate: 'Chaos', monte_carlo_risk: 'Monte Carlo' };
const CELL_TEXT: Record<Target, string> = { cpu: 'CPU', cuda: 'CUDA', vulkan: 'VK' };
const SOFT: Record<Target, string> = { cpu: '#b8c4d6', cuda: '#8bbf6e', vulkan: '#f9a561' };
const EMPTY = 'var(--cell-empty)';

/** Which target is fastest, per kernel and size class. */
export function WinnerTile({ info }: { info: Info | null }) {
  const rows = heatRows(info?.rates ?? []);
  const cells: HeatCell[][] = rows.map(r => r.cells.map(c => ({
    color: c.winner ? SOFT[c.winner] : EMPTY,
    text: c.winner ? CELL_TEXT[c.winner] : '',
    title: c.winner ? `${c.winner} is fastest` : 'not measured',
  })));
  return (
    <Tile title="Fastest target by size" className="heatmap">
      <HeatMap
        rows={rows.map(r => SHORT[r.op.name])}
        cols={SIZE_CLASSES.map(c => c.label)}
        cells={cells}
        xLabel="Workload size"
        legend={(['cpu', 'cuda', 'vulkan'] as Target[]).map(t => (
          <span key={t}><i style={{ background: SOFT[t], borderColor: TARGET_COLOR[t] }} />{t === 'cpu' ? 'CPU' : t === 'cuda' ? 'CUDA' : 'Vulkan'}</span>
        ))}
      />
    </Tile>
  );
}

/** How much faster the best GPU is than the CPU (green) or slower (red). */
export function SpeedupTile({ info }: { info: Info | null }) {
  const rows = heatRows(info?.rates ?? []);
  const cells: HeatCell[][] = rows.map(r => r.cells.map(c => {
    if (c.speedup === undefined) return { color: EMPTY, text: '' };
    const s = c.speedup;
    const color = s >= 2 ? '#8bbf6e' : s >= 1.1 ? '#c5e1a5' : s >= 0.9 ? '#f6e27f' : s >= 0.5 ? '#f9a561' : '#f26b6b';
    return { color, text: s >= 10 ? `${s.toFixed(0)}×` : `${s.toFixed(1)}×`, title: `best GPU is ${s.toFixed(2)}× the CPU` };
  }));
  return (
    <Tile title="GPU speed vs CPU" className="heatmap">
      <HeatMap
        rows={rows.map(r => SHORT[r.op.name])}
        cols={SIZE_CLASSES.map(c => c.label)}
        cells={cells}
        xLabel="Workload size"
        legend={
          <>
            <span><i style={{ background: '#f26b6b' }} />slower</span>
            <span><i style={{ background: '#f6e27f' }} />even</span>
            <span><i style={{ background: '#8bbf6e' }} />faster</span>
          </>
        }
      />
    </Tile>
  );
}
