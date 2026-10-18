import { useCallback, useEffect, useRef, useState } from 'react';
import { getJob, startJob, stopJob } from './api';
import type { DoneEvent, EvalRow, JobStatus, ProbeEvent, PyreEvent, RoundEvent, ScoreEvent, VerdictEvent } from './types';

export interface SessionView {
  cmd: string;
  running: boolean;
  exit: number | null;
  started: number;
  events: PyreEvent[];
  phase: string;
  probes: ProbeEvent[];
  rows: EvalRow[];
  rounds: RoundEvent[];
  mode?: 'auto' | 'forever';
  idle: boolean;  // forever mode, resting between rounds
  converged: boolean;
  scores: Record<string, ScoreEvent>;
  verdict?: VerdictEvent;
  done?: DoneEvent;
  logs: string[];
}

export function summarize(status: Pick<JobStatus, 'running' | 'cmd' | 'started' | 'exit'>, events: PyreEvent[], rounds: RoundEvent[] = []): SessionView {
  const view: SessionView = {
    cmd: status.cmd, running: status.running, exit: status.exit, started: status.started,
    events, phase: '', probes: [], rows: [], rounds, idle: false, converged: false, scores: {}, logs: [],
  };
  for (const e of events) {
    switch (e.event) {
      case 'phase': view.phase = e.name; break;
      case 'probe': view.probes.push(e); view.idle = false; break;
      case 'round': view.idle = false; break;
      case 'mode': view.mode = e.mode; break;
      case 'idle': view.idle = true; break;
      case 'converged': view.converged = true; break;
      case 'eval_row': view.rows.push(e); break;
      case 'score': view.scores[e.phase] = e; break;
      case 'verdict': view.verdict = e; break;
      case 'done': view.done = e; break;
      case 'log': view.logs.push(e.message); break;
      case 'skipped': view.logs.push(`${e.target} is not available and was skipped`); break;
    }
  }
  // A forever session can run for hours: keep the page light by showing only the recent probes and checks.
  view.probes = view.probes.slice(-600);
  view.rows = view.rows.slice(-120);
  return view;
}

/** Polls the dashboard server for the running (or last) session and exposes start / stop. */
export function useSession(onFinished: () => void) {
  const [view, setView] = useState<SessionView | null>(null);
  const [error, setError] = useState('');
  const since = useRef(0);
  const events = useRef<PyreEvent[]>([]);
  const rounds = useRef<RoundEvent[]>([]);
  const startedAt = useRef(0);
  const wasRunning = useRef(false);
  const finished = useRef(onFinished);
  finished.current = onFinished;

  const poll = useCallback(async () => {
    try {
      const s = await getJob(since.current);
      if (s.started !== startedAt.current || s.next < since.current) {  // a different session: start over
        startedAt.current = s.started;
        events.current = [];
        rounds.current = [];
        since.current = 0;
        if (s.next > s.lines.length) return poll();
      }
      events.current = events.current.concat(s.lines);
      for (const l of s.lines) if (l.event === 'round') rounds.current.push(l);
      if (rounds.current.length > 400) rounds.current = rounds.current.slice(-300);
      if (events.current.length > 6000) events.current = events.current.slice(-4000);
      since.current = s.next;
      setView(s.started ? summarize(s, events.current, rounds.current) : null);
      if (wasRunning.current && !s.running) finished.current();
      wasRunning.current = s.running;
      setError('');
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, []);

  useEffect(() => {
    void poll();
    const id = window.setInterval(() => void poll(), view?.running ? 500 : 2500);
    return () => window.clearInterval(id);
  }, [poll, view?.running]);

  const start = useCallback(async (cmd: 'train' | 'check' | 'forget', args: string[]) => {
    try {
      await startJob(cmd, args);
      events.current = [];
      rounds.current = [];
      since.current = 0;
      startedAt.current = 0;
      wasRunning.current = true;
      await poll();
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, [poll]);

  const stop = useCallback(async () => {
    try { await stopJob(); } catch (e) { setError(e instanceof Error ? e.message : String(e)); }
  }, []);

  return { view, error, start, stop };
}
