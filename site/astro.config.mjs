// @ts-check
import { defineConfig } from 'astro/config';
import { GITHUB_URL } from './src/lib/site.ts';
import { BASE, SITE_ORIGIN } from './repo.mjs';

// Served from GitHub Pages on its own domain, at the root (repo.mjs, public/CNAME).
const base = BASE;
const repo = GITHUB_URL;

// Pages of the heap 0.7 site that the one-page site replaced. Old links (the README, posts)
// land on the closest place instead of a 404.
const docs = ['tutorial', 'hotkeys', 'integrations', 'oauth-setup', 'data', 'distribution', 'packaging'];
const areas = ['plan', 'time', 'know', 'connect', 'flow'];
const redirects = {
  '/demo': `${base}/#try`,
  '/privacy': `${base}/#trust`,
  '/compare': `${base}/`,
  '/features': `${base}/`,
  ...Object.fromEntries(areas.map((a) => [`/features/${a}`, `${base}/`])),
  '/docs': `${repo}/tree/master/docs`,
  ...Object.fromEntries(docs.map((d) => [`/docs/${d}`, `${repo}/tree/master/docs`])),
  '/changelog': `${repo}/releases`,
  '/brand': `${base}/`,
};

export default defineConfig({
  site: SITE_ORIGIN,
  base: base || '/',
  trailingSlash: 'always',
  devToolbar: { enabled: false },
  redirects,
});
