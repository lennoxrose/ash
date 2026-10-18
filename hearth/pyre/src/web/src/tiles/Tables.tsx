import { TARGET_COLOR, TARGET_LABEL } from '../colors';
import { DataTable, Empty, Tile } from '../components/ui';
import { fmtDate, fmtDuration, fmtPct } from '../format';
import type { HistoryEntry, Info } from '../types';

export function DevicesTile({ info, detailed = false }: { info: Info | null; detailed?: boolean }) {
  return (
    <Tile title="Devices" className="table-tile">
      {!info ? <Empty>Waiting for the Pyre server…</Empty> : (
        <DataTable head={detailed ? ['Target', 'Device', 'Status', 'Last start-up'] : ['Target', 'Device', 'Status']} widths={detailed ? ['16%', '44%', '20%', '20%'] : ['23%', '53%', '24%']}>
          {info.targets.map(t => {
            const seen = t.name === 'cpu' || !!t.present;
            return (
              <tr key={t.name}>
                <td><i className="swatch" style={{ background: TARGET_COLOR[t.name] }} />{TARGET_LABEL[t.name]}</td>
                <td>{t.name === 'cpu' ? `${info.cpu_threads} threads` : seen ? t.device : '–'}</td>
                <td><span className={`dot ${seen ? 'dot-good' : 'dot-idle'}`} />{seen ? 'Seen' : t.name === 'cuda' && !info.cuda_built ? 'Not built' : 'Not seen'}</td>
                {detailed && <td className="num">{t.init_ms > 0 ? `${t.init_ms.toFixed(0)} ms` : '–'}</td>}
              </tr>
            );
          })}
        </DataTable>
      )}
    </Tile>
  );
}

const VERDICT_CLASS: Record<string, string> = { better: 'v-good', worse: 'v-bad', same: 'v-neutral', unknown: 'v-warn', unverified: 'v-neutral', check: 'v-neutral' };

export function SessionsTile({ history }: { history: HistoryEntry[] }) {
  const recent = [...history].reverse().slice(0, 5);
  return (
    <Tile title="Sessions" className="table-tile">
      {recent.length === 0 ? <Empty>No sessions yet. Every finished training or self-check is logged here.</Empty> : (
        <DataTable head={['Session', 'Time lost', 'Result']} widths={['40%', '36%', '24%']}>
          {recent.map((h, i) => (
            <tr key={i}>
              <td title={fmtDate(h.time)}>{h.kind === 'train' ? `Train · ${h.level}` : 'Self-check'}</td>
              <td className="num">{h.before && h.kind === 'train' ? `${fmtPct(h.before.lost_pct)} → ${h.after ? fmtPct(h.after.lost_pct) : '–'}` : h.after ? fmtPct(h.after.lost_pct) : '–'}</td>
              <td className={VERDICT_CLASS[h.verdict] ?? 'v-neutral'}>{h.verdict}</td>
            </tr>
          ))}
        </DataTable>
      )}
    </Tile>
  );
}

/** Gantt-style rows: one bar per recent session, length = how long it took, colour = verdict. */
export function TimelineTile({ history }: { history: HistoryEntry[] }) {
  const recent = history.slice(-6);
  const longest = Math.max(1, ...recent.map(h => h.seconds));
  const COLOR: Record<string, string> = { better: '#2e9e5b', worse: '#c0392b', same: '#e0a100', unknown: '#8e44ad', unverified: '#7f8c8d', check: '#3b82f6' };
  return (
    <Tile title="Session durations" className="timeline">
      {recent.length === 0 ? <Empty>Sessions appear here as a timeline once you have run one.</Empty> : (
        <ul className="gantt">
          {recent.map((h, i) => (
            <li key={i}>
              <span className="gantt-label" style={{ color: COLOR[h.verdict] ?? '#7f8c8d' }}>{h.kind === 'train' ? `Train · ${h.level}` : 'Self-check'}</span>
              <span className="gantt-track">
                <span className="gantt-bar" style={{ width: `${Math.max(3, (h.seconds / longest) * 100)}%`, background: COLOR[h.verdict] ?? '#7f8c8d' }} />
              </span>
              <span className="gantt-value">{fmtDuration(h.seconds)}</span>
            </li>
          ))}
        </ul>
      )}
      <div className="gantt-axis"><span>0</span><span>duration</span><span>{fmtDuration(longest)}</span></div>
    </Tile>
  );
}
