// Microsoft (Entra ID) configuration for the SPA.
//
// These values are public by design: a single-page app uses the authorization
// code flow with PKCE and must never hold a client secret. AUC IT creates the
// app registration; a free dev tenant works for testing the same code.
//
// Read at runtime from $app/env/public (declared in src/env.ts), so a
// deployment can change the tenant or client id without rebuilding.

import { PUBLIC_MSAL_CLIENT_ID, PUBLIC_MSAL_REDIRECT_URI, PUBLIC_MSAL_TENANT_ID } from '$app/env/public';
import type { PublicClientApplication } from '@azure/msal-browser';

const clientId = (PUBLIC_MSAL_CLIENT_ID ?? '').trim();
const tenantId = (PUBLIC_MSAL_TENANT_ID ?? '').trim();
const explicitRedirect = (PUBLIC_MSAL_REDIRECT_URI ?? '').trim();

function currentOrigin(): string {
	return typeof location === 'undefined' ? '' : location.origin;
}

/** Environment variables that still need a value. */
export const missingConfig: string[] = [
	...(clientId ? [] : ['PUBLIC_MSAL_CLIENT_ID']),
	...(tenantId ? [] : ['PUBLIC_MSAL_TENANT_ID'])
];

export const authConfigured = missingConfig.length === 0;

/** Scopes: the ID token carries the person's name, email and app roles. */
export const loginScopes = ['openid', 'profile', 'email'];

export const msalConfig = {
	clientId,
	tenantId,
	authority: `https://login.microsoftonline.com/${tenantId || 'organizations'}`,
	redirectUri: explicitRedirect || `${currentOrigin()}/auth/callback`,
	postLogoutRedirectUri: `${currentOrigin()}/login`
};

let clientPromise: Promise<PublicClientApplication> | null = null;

/**
 * One shared MSAL instance, imported lazily so the module stays safe to import
 * during server rendering (MSAL is browser-only).
 */
export function getMsal(): Promise<PublicClientApplication> {
	clientPromise ??= (async () => {
		const { PublicClientApplication } = await import('@azure/msal-browser');
		const client = new PublicClientApplication({
			auth: {
				clientId: msalConfig.clientId,
				authority: msalConfig.authority,
				redirectUri: msalConfig.redirectUri,
				postLogoutRedirectUri: msalConfig.postLogoutRedirectUri
			},
			cache: {
				cacheLocation: 'sessionStorage'
			}
		});
		await client.initialize();
		return client;
	})();
	return clientPromise;
}
