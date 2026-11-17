// Public configuration for real Microsoft (Entra ID) sign-in.
//
// These values are public by design: a single-page app uses PKCE and must
// never hold a client secret. AUC IT creates the app registration; a free
// dev tenant works for testing the same code.
//
// SvelteKit exposes these to the browser because they are marked public.

import { defineEnvVars } from '@sveltejs/kit/env';

export const variables = defineEnvVars({
	PUBLIC_MSAL_CLIENT_ID: {
		public: true,
		schema: (value: string | undefined) => value ?? '',
		description: 'Application (client) ID of the AUC Entra ID app registration.'
	},
	PUBLIC_MSAL_TENANT_ID: {
		public: true,
		schema: (value: string | undefined) => value ?? '',
		description: 'Directory (tenant) ID of the AUC Entra ID tenant.'
	},
	PUBLIC_MSAL_REDIRECT_URI: {
		public: true,
		schema: (value: string | undefined) => value ?? '',
		description: 'Optional redirect URI. Defaults to <origin>/auth/callback.'
	},
	PUBLIC_DEMO_AUTH: {
		public: true,
		schema: (value: string | undefined) => value ?? '',
		description:
			'Set to "true" to offer demo identities instead of Microsoft sign-in. Ignored in production builds.'
	}
});
