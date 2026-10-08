import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// The built dashboard lands next to the `pyre` executable (pyre/src/web ->
// <repo>/bin/extensions/pyre/web), which is where `pyre dashboard` serves it
// from. Relative base: the page must work from any port, fully offline.
export default defineConfig({
  plugins: [react()],
  base: './',
  build: {
    outDir: '../../../bin/extensions/pyre/web',
    emptyOutDir: true,
    sourcemap: false,
  },
  server: {
    fs: { allow: ['../../..'] },  // logo.svg lives in the repo root
    // `pnpm dev` against a running `pyre dashboard --no-open` (port 1198).
    proxy: { '/api': 'http://127.0.0.1:1198' },
  },
});
