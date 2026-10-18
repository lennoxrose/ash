import { useEffect, useState } from 'react';
import { TARGET_COLOR, TARGET_LABEL } from '../colors';
import { LineChart, type Series } from '../components/LineChart';
import { TrendNote } from '../components/TrendNote';
import { Badge, DataTable, Segmented, Tile } from '../components/ui';
import { fmtDuration, fmtMs, fmtPct } from '../format';
import type { SessionView } from '../session';
import { smooth, trend } from '../trend';
import { OPS, TARGETS, type ScoreEvent } from '../types';

const PHASE: Record<string, string> = {
  seeding: 'Seeding grid', 'check-before': 'Grading (before)', training: 'Training probes', practising: 'Practising weak spots',
  'check-after': 'Grading (after)', check: 'Self-check',
};
const VERDICT: Record<string, { cls: string; text: string }> = {
  better: { cls: 'v-good', text: 'Better: decisions improved, training kept' },
  same: { cls: 'v-neutral', text: 'No measurable change: decisions were already as good as these probes can make them' },
  worse: { cls: 'v-bad', text: 'Worse: training was discarded, previous knowledge kept' },
  unknown: { cls: 'v-warn', text: 'Not enough held-out sizes to compare; training kept' },
  unverified: { cls: 'v-neutral', text: 'Self-check skipped; training kept' },
};

function Score({ label, s }: { label: string; s?: ScoreEvent }) {
  if (!s || !s.shapes) return null;
  return (
    <div className="mini">
      <span className="mini-label">{label}</span>
      <strong className={s.lost_pct < 5 ? 'tone-good' : s.lost_pct < 25 ? 'tone-warn' : 'tone-bad'}>{fmtPct(s.lost_pct)}</strong>
      <small>time lost · right {s.hits}/{s.shapes} · error {fmtPct(s.error_pct)}</small>
    </div>
  );
}

type Tab = 'rounds' | 'probes' | 'results';

/** The running (or last) session: numbers on top, then one of rounds / probes / results. */
export function LiveTile({ view }: { view: SessionView | null }) {
  const [now, setNow] = useState(() => Date.now() / 1000);
  const [picked, setTab] = useState<Tab | null>(null);
  const running = view?.running ?? false;
  useEffect(() => {
    if (!running) return;
    const id = window.setInterval(() => setNow(Date.now() / 1000), 500);
    return () => window.clearInterval(id);
  }, [running]);

  if (!view) return <Tile title="Live session" className="live"><div className="empty">No session yet. Start one on the left.</div></Tile>;

  const last = view.probes[view.probes.length - 1];
  const elapsed = view.done ? view.done.seconds : Math.max(0, now - view.started);
  const tabs: { value: Tab; label: string }[] = [
    ...(view.rounds.length ? [{ value: 'rounds' as Tab, label: 'Rounds' }] : []),
    { value: 'probes', label: 'Probe timings' },
    ...(view.rows.length ? [{ value: 'results' as Tab, label: 'Self-check results' }] : []),
  ];
  const tab: Tab = picked && tabs.some(t => t.value === picked) ? picked : tabs[0].value;

  const chartOp = last?.op ?? view.probes[0]?.op;
  const probeSeries: Series[] = TARGETS.map(t => ({
    name: TARGET_LABEL[t], color: TARGET_COLOR[t],
    points: view.probes.filter(p => p.op === chartOp && p.target === t).map(p => [p.exp, p.ms] as [number, number]),
  })).filter(s => s.points.length);

  // Round curve: time lost per round (noisy: each round grades on different sizes), smoothed, plus a forecast.
  const lost = view.rounds.map(r => r.lost_pct);
  const sm = smooth(lost);
  const rt = trend(sm, 'lower', [0, Infinity]);
  const lastRound = view.rounds.length ? view.rounds[view.rounds.length - 1].round : 0;
  const roundSeries: Series[] = [
    { name: 'Time lost per round (%)', color: '#c0392b', points: view.rounds.map(r => [r.round, r.lost_pct] as [number, number]) },
    { name: 'Smoothed', color: '#2457d6', points: view.rounds.map((r, i) => [r.round, sm[i]] as [number, number]) },
    ...(rt ? [{ name: 'Tendency', color: '#2457d6', dashed: true, points: [[lastRound, sm[sm.length - 1]] as [number, number], ...rt.forecast(3).map((v, k) => [lastRound + k + 1, v] as [number, number])] }] : []),
  ];
  const kept = view.rounds.filter(r => r.round > 0 && r.accepted).length;
  const tried = view.rounds.filter(r => r.round > 0).length;
  const currentFocus = view.rounds.length ? view.rounds[view.rounds.length - 1].focus : [];
  const verdict = view.verdict ? VERDICT[view.verdict.verdict] : undefined;
  const wrong = view.rows.filter(r => !r.right).length;

  const phaseText = !view.running ? 'Done' : view.idle ? 'Resting between rounds' : PHASE[view.phase] ?? 'Starting…';
  const badge = view.running ? <Badge tone="info">{view.mode === 'forever' ? 'Forever' : 'Running'}</Badge>
    : view.exit === 0 ? <Badge tone="good">Finished</Badge>
    : view.done && view.verdict?.discarded ? <Badge tone="warn">Finished, training discarded</Badge>
    : <Badge tone="bad">{view.exit === null ? 'Stopped' : `Exited (${view.exit})`}</Badge>;

  return (
    <Tile title={`Live session: ${view.mode === 'forever' ? 'forever retraining' : view.mode === 'auto' ? 'adaptive' : view.cmd}`} className="live" actions={badge}>
      <div className={`bar ${view.running && !view.idle ? 'bar-run' : ''}`}><span /></div>
      <div className="strip">
        <div className="mini"><span className="mini-label">Phase</span><strong>{phaseText}</strong></div>
        {view.mode && <div className="mini"><span className="mini-label">Rounds</span><strong>{tried}</strong><small>{tried ? `${kept} kept, ${tried - kept} reverted` : 'baseline first'}</small></div>}
        <div className="mini"><span className="mini-label">Probes run</span><strong>{view.probes.length}</strong><small>{last ? `${last.op} · ${TARGET_LABEL[last.target]} · ${last.shape}` : ''}</small></div>
        <div className="mini"><span className="mini-label">Elapsed</span><strong>{fmtDuration(elapsed)}</strong></div>
        <Score label="Before" s={view.scores.before} />
        <Score label="After" s={view.scores.after} />
        <Score label="Current" s={view.scores.now} />
      </div>
      {verdict && <p className={`msg ${verdict.cls}`}>{view.converged ? 'Converged: ' : ''}{verdict.text}</p>}
      {view.running && currentFocus.length > 0 && <p className="note">Last practised: {currentFocus.join(' · ')}</p>}
      {view.logs.map((l, i) => <p key={i} className="msg msg-warn">{l}</p>)}

      <Segmented label="View" value={tab} onChange={setTab} options={tabs} />
      <div className="live-view">
        {tab === 'rounds' && (
          <>
            <LineChart series={roundSeries} xLabel="round" yLabel="time lost (%)" yMin={0} yTick={v => `${v.toFixed(0)}%`} />
            <TrendNote t={rt} format={v => `${v.toFixed(1)}%`} />
          </>
        )}
        {tab === 'probes' && (probeSeries.length
          ? <LineChart series={probeSeries} xLabel={`size (2^k units) · ${chartOp ?? ''}`} yLabel="time per call" logY xTick={x => `2^${x}`} yTick={fmtMs} />
          : <div className="empty">No probes yet.</div>)}
        {tab === 'results' && (
          <>
            <h3 className="sub">{wrong} wrong of {view.rows.length}</h3>
            <div className="scroll">
              <DataTable head={['Phase', 'Kernel', 'Size', 'CPU', 'CUDA', 'Vulkan', 'Picked', '']} widths={['12%', '13%', '19%', '11%', '11%', '11%', '12%', '11%']}>
                {view.rows.map((r, i) => (
                  <tr key={i}>
                    <td>{r.phase}</td><td>{OPS.find(o => o.name === r.op)?.short ?? r.op}</td><td className="mono">{r.shape}</td>
                    {TARGETS.map(t => <td key={t} className="num">{r.times_ms[t] === undefined ? '–' : fmtMs(r.times_ms[t] as number)}</td>)}
                    <td>{TARGET_LABEL[r.picked]}</td>
                    <td className={r.right ? 'v-good' : 'v-bad'}>{r.right ? 'right' : 'wrong'}</td>
                  </tr>
                ))}
              </DataTable>
            </div>
          </>
        )}
      </div>
    </Tile>
  );
}
