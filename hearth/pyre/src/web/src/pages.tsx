import { crossover, coverage, latestScore, sessionSeries, speedupTendency } from './analytics';
import { Gauge } from './components/Gauge';
import { TrendNote } from './components/TrendNote';
import { trend } from './trend';
import { LineChart } from './components/LineChart';
import { DataTable, Empty, Tile } from './components/ui';
import { fmtDate, fmtDuration, fmtPct, pctTick } from './format';
import type { SessionView } from './session';
import { ControlTile } from './tiles/Control';
import { SpeedupTile, WinnerTile } from './tiles/Heat';
import { KpiTile } from './tiles/Kpi';
import { LiveTile } from './tiles/Live';
import { KernelTile, SpeedTile } from './tiles/Speed';
import { DevicesTile, SessionsTile, TimelineTile } from './tiles/Tables';
import { TARGET_COLOR, TARGET_LABEL } from './colors';
import { OPS, type HistoryEntry, type Info } from './types';

interface Data { info: Info | null; history: HistoryEntry[] }

/** Landing page: analytics only, scaled to fill the screen. */
export function DashboardPage({ info, history }: Data) {
  const score = latestScore(history);
  const series = sessionSeries(history);
  const lostTrend = trend(series.lost, 'lower', [0, Infinity]);
  const hitTrend = trend(series.hit, 'higher', [0, 100]);
  const accTrend = trend(series.accuracy, 'higher', [0, 100]);
  const hitRate = score ? (score.hits / Math.max(1, score.shapes)) * 100 : null;
  const accuracy = score && score.error_pct !== null ? Math.max(0, 100 - score.error_pct) : null;
  const lostTone = !score ? 'ink' : score.lost_pct < 5 ? 'good' : score.lost_pct < 25 ? 'warn' : 'bad';
  const pct = (v: number) => (v < 10 ? `${v.toFixed(1)}%` : `${v.toFixed(0)}%`);
  return (
    <main className="wall">
      <SpeedTile info={info} />
      <KpiTile title="Right picks" value={score ? (hitRate ?? 0).toFixed(0) : '–'} unit={score ? '%' : undefined} tone={hitRate === null ? 'ink' : hitRate >= 80 ? 'good' : hitRate >= 50 ? 'warn' : 'bad'} trend={hitTrend} format={pct} />
      <KpiTile title="Time lost" value={score ? (score.lost_pct < 10 ? score.lost_pct.toFixed(1) : score.lost_pct.toFixed(0)) : '–'} unit={score ? '%' : undefined} tone={lostTone} spark={series.lost.slice(-12)} trend={lostTrend} format={pct} />
      <WinnerTile info={info} />
      <SpeedupTile info={info} />
      <DevicesTile info={info} />
      <SessionsTile history={history} />
      <Tile title="Picked the best target" className="gauge-tile"><Gauge value={hitRate} caption="Picked the best target" /><TrendNote t={hitTrend} format={pct} /></Tile>
      <Tile title="Prediction accuracy" className="gauge-tile"><Gauge value={accuracy} caption="Prediction accuracy" /><TrendNote t={accTrend} format={pct} /></Tile>
    </main>
  );
}

/** Start, watch and stop sessions. */
export function TrainingPage({ info, history, view, error, onStart, onStop }: {
  info: Info | null; history: HistoryEntry[]; view: SessionView | null; error: string;
  onStart: (cmd: 'train' | 'check' | 'forget', args: string[]) => void; onStop: () => void;
}) {
  return (
    <main className="split">
      <ControlTile info={info} history={history} running={view?.running ?? false} error={error} onStart={onStart} onStop={onStop} />
      <LiveTile view={view} />
    </main>
  );
}

/** Per-kernel charts, the two matrices, and where each GPU starts to pay off. */
export function PerformancePage({ info }: { info: Info | null }) {
  const rates = info?.rates ?? [];
  return (
    <main className="perf">
      {OPS.map(o => <KernelTile key={o.key} info={info} op={o.key} />)}
      <WinnerTile info={info} />
      <SpeedupTile info={info} />
      <Tile title="Where the GPU starts to pay off" className="table-tile">
        <DataTable head={['Kernel', 'GPU faster from', 'Tendency']} widths={['24%', '30%', '46%']}>
          {OPS.map(o => {
            const c = crossover(rates, o.name);
            return (
              <tr key={o.key}>
                <td>{o.label}</td>
                <td>{!c.measured ? 'not measured' : c.bucket === null ? 'not at any size' : `2^${c.bucket} and up`}</td>
                <td title="Straight line through GPU-vs-CPU speed ratio across sizes; an estimate">{speedupTendency(rates, o.name)}</td>
              </tr>
            );
          })}
        </DataTable>
      </Tile>
    </main>
  );
}

/** What hardware Pyre found and how much it has learned about each device. */
export function DevicesPage({ info }: { info: Info | null }) {
  const cov = coverage(info?.rates ?? []);
  return (
    <main className="devices">
      <DevicesTile info={info} detailed />
      <Tile title="What has been learned" className="table-tile">
        <DataTable head={['Kernel', 'Target', 'Sizes measured', 'Smallest', 'Largest']} widths={['26%', '20%', '22%', '16%', '16%']}>
          {cov.map((c, i) => (
            <tr key={i}>
              <td>{c.op.label}</td>
              <td><i className="swatch" style={{ background: TARGET_COLOR[c.target] }} />{TARGET_LABEL[c.target]}</td>
              <td className="num">{c.count}</td>
              <td className="num">{c.min === null ? '–' : `2^${c.min}`}</td>
              <td className="num">{c.max === null ? '–' : `2^${c.max}`}</td>
            </tr>
          ))}
        </DataTable>
      </Tile>
      <Tile title="Profile" className="table-tile">
        {!info ? <Empty>Waiting for the Pyre server…</Empty> : (
          <DataTable head={['Setting', 'Value']} widths={['32%', '68%']}>
            <tr><td>Profile file</td><td className="mono" title={info.profile}>{info.profile || '(memory only)'}</td></tr>
            <tr><td>CPU threads</td><td>{info.cpu_threads}</td></tr>
            <tr><td>CUDA support</td><td>{info.cuda_built ? 'built in' : 'built without CUDA'}</td></tr>
            <tr><td>Learned data points</td><td>{info.rates.length}</td></tr>
          </DataTable>
        )}
      </Tile>
    </main>
  );
}

const V: Record<string, string> = { better: 'v-good', worse: 'v-bad', same: 'v-neutral', unknown: 'v-warn', unverified: 'v-neutral', check: 'v-neutral' };

/** Every recorded session, with trends over time. */
export function HistoryPage({ history }: { history: HistoryEntry[] }) {
  const scored = history.map((h, i) => ({ i: i + 1, s: h.after ?? h.before })).filter(x => x.s);
  const sr = sessionSeries(history);
  const lostT = trend(sr.lost, 'lower', [0, Infinity]);
  const hitT = trend(sr.hit, 'higher', [0, 100]);
  const lastX = scored.length ? scored[scored.length - 1].i : 0;
  const fc = (t: ReturnType<typeof trend>, last?: number) => (t && last !== undefined ? [[lastX, last] as [number, number], ...t.forecast(3).map((v, k) => [lastX + k + 1, v] as [number, number])] : []);
  return (
    <main className="hist">
      <Tile title="Time lost and right picks, by session" className="hist-chart">
        {scored.length ? (
          <LineChart
            series={[{ name: 'Time lost (%)', color: '#c62828', points: scored.map(x => [x.i, x.s!.lost_pct] as [number, number]) },
                     { name: 'Right picks (%)', color: '#2e7d32', points: scored.map(x => [x.i, (x.s!.hits / Math.max(1, x.s!.shapes)) * 100] as [number, number]) },
                     ...(lostT ? [{ name: 'Time lost, tendency', color: '#c62828', dashed: true, points: fc(lostT, sr.lost[sr.lost.length - 1]) }] : []),
                     ...(hitT ? [{ name: 'Right picks, tendency', color: '#2e7d32', dashed: true, points: fc(hitT, sr.hit[sr.hit.length - 1]) }] : [])]}
            xLabel="session #" yLabel="%" yMin={0} yMax={Math.max(100, ...scored.map(x => x.s!.lost_pct))} yTick={pctTick}
          />
        ) : <Empty>No scored sessions yet.</Empty>}
      </Tile>
      <TimelineTile history={history} />
      <Tile title="All sessions" className="hist-table">
        {history.length === 0 ? <Empty>No sessions recorded yet.</Empty> : (
          <div className="scroll fill">
            <DataTable head={['#', 'When', 'Session', 'Loads', 'Probes', 'Took', 'Lost before', 'Lost after', 'Right', 'Result']} widths={['5%', '17%', '14%', '10%', '8%', '8%', '10%', '10%', '8%', '10%']}>
              {history.map((h, i) => (
                <tr key={i}>
                  <td className="num">{i + 1}</td><td>{fmtDate(h.time)}</td>
                  <td>{h.kind === 'train' ? `Train · ${h.level}` : 'Self-check'}</td>
                  <td className="num">{h.kind === 'train' ? `${h.gpu_load}/${h.cpu_load}%` : '–'}</td>
                  <td className="num">{h.probes || '–'}</td><td className="num">{fmtDuration(h.seconds)}</td>
                  <td className="num">{h.before ? fmtPct(h.before.lost_pct) : '–'}</td>
                  <td className="num">{h.after ? fmtPct(h.after.lost_pct) : '–'}</td>
                  <td className="num">{h.after ? `${h.after.hits}/${h.after.shapes}` : '–'}</td>
                  <td className={V[h.verdict] ?? 'v-neutral'}>{h.verdict}</td>
                </tr>
              )).reverse()}
            </DataTable>
          </div>
        )}
      </Tile>
    </main>
  );
}
