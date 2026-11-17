import type { PageLoad } from './$types';

// The redirect lands here; MSAL must run in the browser.
export const ssr = false;

export const load: PageLoad = () => ({});
