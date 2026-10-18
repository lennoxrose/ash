import { useState } from 'react';
import { TARGET_COLOR, TARGET_LABEL } from '../colors';
import { LineChart, type Series } from '../components/LineChart';
import { Empty, Segmented, Tile } from '../components/ui';
import { fmtRate } from '../format';
import { OPS, TARGETS, type Info, type OpKey } from '../types';

/** Chart body: effective speed by workload size, background shaded by which target wins. */
function Chart({ info, op }: { info: Info | null; op: OpKey }) {
  const spec = OPS.find(o => o.key === op)!;
  const mine = (info?.rates ?? []).filter(r => r.op === spec.name);
  const series: Series[] = TARGETS.map(t => ({
    name: TARGET_LABEL[t],
    color: TARGET_COLOR[t],
    points: mine.filter(r => r.target === t).map(r => [r.bucket, r.per_second] as [number, number]),
  })).filter(s => s.points.length);

  // Winner per measured size, shaded from the midpoint before it to the midpoint after.
  const buckets = [...new Set(mine.map(r => r.bucket))].sort((a, b) => a - b);
  const bands = buckets.map((b, i) => {
    const best = mine.filter(r => r.bucket === b).reduce((a, c) => (c.per_second > a.per_second ? c : a));
    return {
      from: i === 0 ? b : (buckets[i - 1] + b) / 2,
      to: i === buckets.length - 1 ? b : (b + buckets[i + 1]) / 2,
      color: TARGET_COLOR[best.target],
    };
  });

  return series.length ? (
    <LineChart series={series} bands={bands} xLabel="workload size (2^k units)" yLabel={spec.unit} logY xTick={x => `2^${x}`} yTick={fmtRate} />
  ) : (
    <Empty>Nothing learned for this kernel yet. Run a training session or run programs that use it.</Empty>
  );
}

/** One kernel's chart as its own tile (Performance page). */
export function KernelTile({ info, op }: { info: Info | null; op: OpKey }) {
  return <Tile title={`${OPS.find(o => o.key === op)!.label}: speed by workload size`} className="chart-tile"><Chart info={info} op={op} /></Tile>;
}

/** The landing page's main chart, with a kernel switch under the title. */
export function SpeedTile({ info }: { info: Info | null }) {
  const [picked, setOp] = useState<OpKey | null>(null);
  const op = picked ?? OPS.find(o => (info?.rates ?? []).some(r => r.op === o.name))?.key ?? 'matmul';
  return (
    <Tile
      title={`${OPS.find(o => o.key === op)!.label}: speed by workload size`}
      className="speed"
      actions={<Segmented label="Kernel" value={op} onChange={setOp} options={OPS.map(o => ({ value: o.key, label: o.short }))} />}
    >
      <Chart info={info} op={op} />
    </Tile>
  );
}
