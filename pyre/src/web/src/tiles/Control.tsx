import { useEffect, useMemo, useState } from 'react';
import { TARGET_LABEL } from '../colors';
import { Button, Check, Chip, Segmented, Tile } from '../components/ui';
import { recommend } from '../recommend';
import { DEFAULT_FORM, PRESETS, loadForm, saveForm, toArgs, toForeverArgs, validate, type Level, type Mode, type TrainForm } from '../trainForm';
import { OPS, TARGETS, type HistoryEntry, type Info, type OpKey, type Target } from '../types';

interface Props {
  info: Info | null;
  history: HistoryEntry[];
  running: boolean;
  error: string;
  onStart: (cmd: 'train' | 'check' | 'forget', args: string[]) => void;
  onStop: () => void;
}

const LOADS = [25, 50, 75, 100].map(v => ({ value: v, label: `${v}%` }));
const PAUSES = [{ value: 10, label: '10 s' }, { value: 30, label: '30 s' }, { value: 60, label: '1 min' }, { value: 300, label: '5 min' }];
type NumKey = 'step' | 'repeats' | 'maxProbe' | 'maxWork' | 'maxMem' | 'pause';

/** Everything `pyre train` can do, as plain controls (no sliders). Lives on the Training page. */
export function ControlTile({ info, history, running, error, onStart, onStop }: Props) {
  const [form, setForm] = useState<TrainForm>(loadForm);
  const [confirm, setConfirm] = useState(false);
  useEffect(() => saveForm(form), [form]);

  const set = <K extends keyof TrainForm>(k: K, v: TrainForm[K]) => setForm(f => ({ ...f, [k]: v }));
  const rec = useMemo(() => recommend(info, history), [info, history]);
  const problem = form.mode === 'recommended' ? '' : validate(form);
  const preset = PRESETS[form.level];
  const num = (k: NumKey, label: string, placeholder: string) => (
    <label className="num-field">
      <span>{label}</span>
      <input type="number" inputMode="decimal" value={form[k]} placeholder={placeholder} onChange={e => set(k, e.target.value)} />
    </label>
  );

  const scope = (
    <div className="ctl">
      <span className="ctl-label">Kernels</span>
      <div className="chips">
        {OPS.map(o => <Chip key={o.key} label={o.short} on={form.ops[o.key]} onClick={() => set('ops', { ...form.ops, [o.key as OpKey]: !form.ops[o.key] })} />)}
      </div>
      <span className="ctl-label">Targets</span>
      <div className="chips">
        {TARGETS.map(t => {
          const seen = t === 'cpu' || info?.targets.find(x => x.name === t)?.present;
          return <Chip key={t} label={TARGET_LABEL[t]} on={form.targets[t]} hint={seen ? undefined : 'not seen on this machine yet'}
            onClick={() => set('targets', { ...form.targets, [t as Target]: !form.targets[t] })} />;
        })}
      </div>
      <span className="ctl-label">GPU load</span>
      <Segmented label="GPU load" value={form.gpuLoad} onChange={v => set('gpuLoad', v)} options={LOADS} />
      <span className="ctl-label">CPU load</span>
      <Segmented label="CPU load" value={form.cpuLoad} onChange={v => set('cpuLoad', v)} options={LOADS} />
    </div>
  );

  const startBar = (label: string, args: string[]) => (
    <div className="btn-row">
      {running ? <Button variant="danger" onClick={onStop}>Stop session</Button>
        : <Button variant="primary" disabled={!!problem} onClick={() => onStart('train', args)}>{label}</Button>}
    </div>
  );

  return (
    <Tile title="Start a session" className="control">
      <Segmented<Mode> label="Mode" value={form.mode} onChange={m => set('mode', m)}
        options={[{ value: 'recommended', label: 'Recommended' }, { value: 'custom', label: 'Custom' }, { value: 'forever', label: 'Forever' }]} />

      {form.mode === 'recommended' && (
        <>
          <div className="rec">
            <h3>{rec.headline}</h3>
            <p className="note">Learns the way a model does: it grades itself on sizes it has not seen, practises exactly where its decisions are weakest, grades again, and keeps a round only if it helped.</p>
            <ul>{rec.reasons.map((r, i) => <li key={i}>{r}</li>)}</ul>
          </div>
          {error && <p className="msg msg-bad">{error}</p>}
          {startBar('Start recommended session', rec.args)}
        </>
      )}

      {form.mode === 'forever' && (
        <>
          <p className="note">Keeps learning until you stop it: every round it re-checks itself on fresh sizes and practises where it is weakest, so it also notices drift (a hotter GPU, a driver update). Saves after every round that helped. Safe to leave running; lower the loads to keep the machine free.</p>
          {scope}
          <div className="ctl">
            <span className="ctl-label">Rest between rounds</span>
            <Segmented label="Rest between rounds" value={form.roundPause} onChange={v => set('roundPause', v)} options={PAUSES} />
          </div>
          {problem && <p className="msg msg-warn">{problem}</p>}
          {error && <p className="msg msg-bad">{error}</p>}
          {startBar('Start forever retraining', toForeverArgs(form))}
        </>
      )}

      {form.mode === 'custom' && (
        <>
          <div className="ctl">
            <span className="ctl-label">Level</span>
            <Segmented label="Level" value={form.level} onChange={(v: Level) => set('level', v)}
              options={[{ value: 'quick', label: 'Quick' }, { value: 'normal', label: 'Normal' }, { value: 'deep', label: 'Deep' }]} />
          </div>
          {scope}
          <p className="note">{preset.blurb} Empty fields below use these preset values.</p>
          <div className="num-grid">
            {num('step', 'Size step', String(preset.step))}
            {num('repeats', 'Repeats', String(preset.repeats))}
            {num('maxProbe', 'Max probe time (s)', String(preset.maxProbe))}
            {num('maxWork', 'Max work (2^k)', '34')}
            {num('maxMem', 'Max memory (MB)', 'no cap')}
            {num('pause', 'Pause (ms)', '0')}
          </div>
          <div className="checks">
            <Check label="Forget everything first" checked={form.reset} onChange={v => set('reset', v)} hint="Train from an empty profile." />
            <Check label="Self-check before and after" checked={form.verify} onChange={v => set('verify', v)} hint="Grades training on sizes it did not train on and undoes it if it made things worse." />
          </div>
          {problem && <p className="msg msg-warn">{problem}</p>}
          {error && <p className="msg msg-bad">{error}</p>}
          <div className="btn-row">
            {running ? <Button variant="danger" onClick={onStop}>Stop session</Button> : (
              <>
                <Button variant="primary" disabled={!!problem} onClick={() => onStart('train', toArgs(form))}>Start training</Button>
                <Button onClick={() => onStart('check', [])}>Run self-check</Button>
                <Button onClick={() => setForm(f => ({ ...DEFAULT_FORM, mode: f.mode }))}>Reset options</Button>
              </>
            )}
          </div>
        </>
      )}

      <div className="danger-row">
        <span>Delete everything learned on this machine (relearned as programs run).</span>
        {confirm
          ? <Button variant="danger" disabled={running} onClick={() => { setConfirm(false); onStart('forget', []); }}>Yes, forget</Button>
          : <Button variant="danger" disabled={running} onClick={() => setConfirm(true)}>Forget</Button>}
      </div>
    </Tile>
  );
}
