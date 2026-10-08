import { useCallback, useEffect, useState } from 'react';
import { getHistory, getStamp, getState } from './api';
import logoUrl from '../../../../logo.svg';
import { Button } from './components/ui';
import { DashboardPage, DevicesPage, HistoryPage, PerformancePage, TrainingPage } from './pages';
import { useSession } from './session';
import { useTheme } from './theme';
import type { HistoryEntry, Info } from './types';

const PAGES = [
  { id: 'dashboard', label: 'Dashboard' },
  { id: 'training', label: 'Training' },
  { id: 'performance', label: 'Performance' },
  { id: 'devices', label: 'Devices' },
  { id: 'history', label: 'History' },
] as const;
type Page = (typeof PAGES)[number]['id'];

function pageFromHash(): Page {
  const h = window.location.hash.slice(1);
  return PAGES.some(p => p.id === h) ? (h as Page) : 'dashboard';
}

export function App() {
  const [page, setPage] = useState<Page>(pageFromHash);
  const [info, setInfo] = useState<Info | null>(null);
  const [history, setHistory] = useState<HistoryEntry[]>([]);
  const [online, setOnline] = useState(true);
  const [theme, toggleTheme] = useTheme();

  useEffect(() => {
    const onHash = () => setPage(pageFromHash());
    window.addEventListener('hashchange', onHash);
    return () => window.removeEventListener('hashchange', onHash);
  }, []);

  const refresh = useCallback(async () => {
    try {
      const [i, h] = await Promise.all([getState(), getHistory()]);
      setInfo(i);
      setHistory(h);
      setOnline(true);
    } catch {
      setOnline(false);
    }
  }, []);

  const { view, error, start, stop } = useSession(() => void refresh());

  // Live refresh without reloading the page: ask the server (cheaply) whether the profile or the
  // history changed -- training, a forever session, or any program that learned something -- and
  // refetch only then. A slow full refresh is the fallback if stamps ever stop changing.
  useEffect(() => {
    let last = '';
    let ticks = 0;
    void refresh();
    const id = window.setInterval(async () => {
      try {
        const st = await getStamp();
        const key = `${st.profile}/${st.history}`;
        ticks++;
        if (key !== last || ticks % 40 === 0) { last = key; await refresh(); }
        else setOnline(true);
      } catch {
        setOnline(false);
      }
    }, 1500);
    return () => window.clearInterval(id);
  }, [refresh]);

  const running = view?.running ?? false;
  const lastRound = view && view.rounds.length ? view.rounds[view.rounds.length - 1].round : 0;

  return (
    <div className="shell">
      <header className="topbar">
        <div className="brand"><img className="logo" src={logoUrl} alt="" /><h1>Pyre</h1></div>
        <nav aria-label="Pages">
          {PAGES.map(p => (
            <a key={p.id} href={`#${p.id}`} className={page === p.id ? 'on' : ''}>
              {p.label}{p.id === 'training' && running ? ' ●' : ''}
            </a>
          ))}
        </nav>
        <div className="topbar-right">
          {running && page !== 'training' && (
            <a className="running-link" href="#training">● {view?.mode === 'forever' ? 'Forever retraining' : 'Training'} running{lastRound ? ` · round ${lastRound}` : ''}</a>
          )}
          {!online && <span className="offline">Server unreachable</span>}
          <Button onClick={toggleTheme} aria-label="Toggle light or dark theme">{theme === 'dark' ? 'Light mode' : 'Dark mode'}</Button>
        </div>
      </header>

      {page === 'dashboard' && <DashboardPage info={info} history={history} />}
      {page === 'training' && <TrainingPage info={info} history={history} view={view} error={error} onStart={(c, a) => void start(c, a)} onStop={() => void stop()} />}
      {page === 'performance' && <PerformancePage info={info} />}
      {page === 'devices' && <DevicesPage info={info} />}
      {page === 'history' && <HistoryPage history={history} />}
    </div>
  );
}
