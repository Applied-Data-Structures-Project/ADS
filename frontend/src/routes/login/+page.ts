import type { PageLoad } from './$types';

// MSAL runs in the browser only.
export const ssr = false;

export const load: PageLoad = () => ({});
