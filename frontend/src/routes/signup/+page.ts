import type { PageLoad } from './$types';

// Reads the MSAL account and localStorage, so client-side only.
export const ssr = false;

export const load: PageLoad = () => ({});
