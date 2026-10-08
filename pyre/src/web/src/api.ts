import type { HistoryEntry, Info, JobStatus } from './types';

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(path, init);
  if (!res.ok) throw new Error(`${res.status} ${await res.text()}`);
  return (await res.json()) as T;
}

export const getState = () => request<Info>('/api/state');
export const getHistory = () => request<HistoryEntry[]>('/api/history');
export interface Stamp { profile: number; history: number }
export const getStamp = () => request<Stamp>('/api/stamp');
export const getJob = (since: number) => request<JobStatus>(`/api/job?since=${since}`);
export const stopJob = () => request<{ stopping: boolean }>('/api/job/stop', { method: 'POST' });
export const startJob = (cmd: 'train' | 'check' | 'forget', args: string[]) =>
  request<{ started: boolean }>('/api/job', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ cmd, args }),
  });
