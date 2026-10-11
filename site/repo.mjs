// The GitHub repository the site belongs to. On GitHub Actions it comes from
// GITHUB_REPOSITORY, so renaming the repository moves every GitHub link with it
// on the next build; locally it falls back to the current name.
const fullName = process.env.GITHUB_REPOSITORY || 'sectapunterx/tabless';
export const REPO = fullName;
export const REPO_NAME = fullName.split('/')[1];

// The site is served from its own domain (public/CNAME), at the root. The old
// project-site address https://sectapunterx.github.io/<name>/ redirects here.
export const SITE_ORIGIN = 'https://tabless.dev';
export const BASE = '';
