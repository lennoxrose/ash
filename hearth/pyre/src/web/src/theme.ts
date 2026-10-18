import { useEffect, useState } from 'react';

export type Theme = 'light' | 'dark';

function initial(): Theme {
  const set = document.documentElement.dataset.theme;
  if (set === 'light' || set === 'dark') return set;
  return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
}

export function useTheme(): [Theme, () => void] {
  const [theme, setTheme] = useState<Theme>(initial);
  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    try { localStorage.setItem('pyre-theme', theme); } catch { /* private mode: not persisted */ }
  }, [theme]);
  return [theme, () => setTheme(t => (t === 'light' ? 'dark' : 'light'))];
}
