// The signed-in person, as told by Microsoft (or, in demo mode, by `demo.ts`).
//
// Identity is real; the application and letter data on the student pages is
// still Group 3's mock until the C++ backend exists. Only this store decides
// who is signed in and which role they have.

import { PUBLIC_DEMO_AUTH } from '$app/env/public';
import type { AccountInfo, AuthenticationResult, PublicClientApplication } from '@azure/msal-browser';
import { authConfigured, getMsal, loginScopes, missingConfig, msalConfig } from './config.ts';
import {
	DEMO_ROLE_KEY,
	demoAccount,
	demoProfile,
	isDemoRole,
	resolveDemoMode
} from './demo.ts';
import {
	isProfileComplete,
	readProfile,
	removeProfile,
	writeProfile,
	validateProfile,
	type ProfileErrors,
	type StudentProfileInput
} from './profile.ts';
import { isAucEmail, isSafeNext, roleFromRoles, type Role } from './roles.ts';

export const NEXT_KEY = 'auc-login-next';
export const AS_KEY = 'auc-login-as';

export type LoginKind = 'student' | 'admin';
export type AuthStatus = 'loading' | 'signedOut' | 'signedIn';

function rolesClaim(account: AccountInfo | null): string[] | undefined {
	const claims = account?.idTokenClaims as Record<string, unknown> | undefined;
	const roles = claims?.roles;
	return Array.isArray(roles) ? roles.filter((role): role is string => typeof role === 'string') : undefined;
}

/** Turn a MSAL/Entra failure into a sentence a student can act on. */
function authErrorMessage(error: unknown): string {
	const e = error as { errorCode?: string; message?: string; errorMessage?: string } | null;
	const code = e?.errorCode ?? '';
	const raw = e?.errorMessage ?? e?.message ?? '';

	if (code.includes('AADSTS50011') || raw.includes('AADSTS50011')) {
		return `Microsoft rejected the redirect address (${msalConfig.redirectUri}). Ask IPSO IT to add it to the app registration.`;
	}
	if (code.includes('AADSTS65001') || raw.includes('AADSTS65001')) {
		return 'An AUC administrator needs to consent to this app before students can sign in.';
	}
	if (code.includes('AADSTS50020') || raw.includes('AADSTS50020')) {
		return 'Use your AUC Microsoft account. Accounts from other organisations are not accepted.';
	}
	if (code === 'interaction_in_progress') {
		return 'A sign-in is already in progress. Close the other tab and try again.';
	}
	if (code === 'access_denied' || code === 'user_cancelled') {
		return 'Sign-in was cancelled.';
	}
	return raw || 'Microsoft sign-in could not be completed. Please try again.';
}

function readSession(key: string): string | null {
	if (typeof sessionStorage === 'undefined') return null;
	try {
		return sessionStorage.getItem(key);
	} catch {
		return null;
	}
}

function writeSession(key: string, value: string): void {
	if (typeof sessionStorage === 'undefined') return;
	try {
		sessionStorage.setItem(key, value);
	} catch {
		// Storage blocked: the value only lasts for this page.
	}
}

class AuthStore {
	status = $state<AuthStatus>('loading');
	error = $state<string | null>(null);
	account = $state<AccountInfo | null>(null);
	profile = $state<StudentProfileInput | null>(null);

	/** Demo identity is an explicit opt-in and never available in a production build. */
	demoEnabled = resolveDemoMode(PUBLIC_DEMO_AUTH, import.meta.env.PROD);
	demoRole = $state<Role | null>(null);

	/** Env values still missing, so the login page can explain itself. */
	configMissing = $state<string[]>(authConfigured ? [] : [...missingConfig]);

	private ready: Promise<void> | null = null;

	role = $derived(roleFromRoles(rolesClaim(this.account)));
	email = $derived(this.account?.username ?? '');
	displayName = $derived(
		this.profile?.name?.trim() ||
			this.account?.name?.trim() ||
			this.account?.username?.split('@')[0] ||
			'Signed-in user'
	);
	initials = $derived(
		this.displayName
			.replace(/^Dr\.\s*/, '')
			.split(/\s+/)
			.map((word) => word[0] ?? '')
			.join('')
			.slice(0, 2)
			.toUpperCase()
	);
	signedIn = $derived(this.status === 'signedIn');
	needsProfile = $derived(this.signedIn && !isProfileComplete(this.profile));

	/** Initialize MSAL and finish a redirect once per page load. */
	init(): Promise<void> {
		this.ready ??= this.bootstrap();
		return this.ready;
	}

	private async bootstrap(): Promise<void> {
		if (this.demoEnabled) {
			const saved = readSession(DEMO_ROLE_KEY);
			if (isDemoRole(saved)) this.applyDemo(saved);
			else this.status = 'signedOut';
			return;
		}

		if (!authConfigured) {
			this.status = 'signedOut';
			return;
		}

		try {
			const client = await getMsal();
			let result: AuthenticationResult | null = null;
			try {
				// The callback route finishes the sign-in itself; keep MSAL from
				// navigating back to /login and skipping that step.
				result = await client.handleRedirectPromise({ navigateToLoginRequestUrl: false });
			} catch (error) {
				this.error = authErrorMessage(error);
			}

			const account =
				result?.account ?? client.getActiveAccount() ?? client.getAllAccounts()[0] ?? null;

			if (!account) {
				this.status = 'signedOut';
				return;
			}

			if (!isAucEmail(account.username)) {
				this.error =
					'Sign in with your AUC Microsoft account (…@aucegypt.edu), not a personal account.';
				this.status = 'signedOut';
				return;
			}

			if (result?.account) client.setActiveAccount(result.account);
			this.account = account;
			this.profile = readProfile(account.homeAccountId);
			this.status = 'signedIn';
		} catch (error) {
			this.error = authErrorMessage(error);
			this.status = 'signedOut';
		}
	}

	/** Sign in as a demo identity. Does nothing unless demo mode is on. */
	signInAsDemo(role: Role): void {
		if (!this.demoEnabled) return;
		this.error = null;
		this.applyDemo(role);
	}

	private applyDemo(role: Role): void {
		this.demoRole = role;
		const account = demoAccount(role);
		this.account = account;
		// Staff never use /signup; demo students go through /signup until saved on the backend.
		this.profile = role === 'student' ? readProfile(account.homeAccountId) : demoProfile();
		this.error = null;
		this.status = 'signedIn';
		writeSession(DEMO_ROLE_KEY, role);
	}

	/** Start the Microsoft redirect. The tab only changes copy and the expected role. */
	async signIn(kind: LoginKind = 'student'): Promise<void> {
		this.error = null;
		if (!authConfigured) {
			this.error = 'Microsoft sign-in is not configured yet.';
			return;
		}
		writeSession(AS_KEY, kind);
		try {
			const client = await getMsal();
			// Let the person pick an account: prevents a cached personal account
			// from silently blocking an AUC one on a shared browser.
			await client.loginRedirect({ scopes: loginScopes, prompt: 'select_account' });
		} catch (error) {
			this.error = authErrorMessage(error);
		}
	}

	/** Sign out of Microsoft and come back to the login page. */
	async signOut(): Promise<void> {
		const wasDemo = this.demoEnabled || this.demoRole !== null;
		this.account = null;
		this.profile = null;
		this.error = null;
		this.status = 'signedOut';
		this.demoRole = null;
		if (wasDemo) {
			writeSession(DEMO_ROLE_KEY, '');
			if (typeof location !== 'undefined') location.assign('/login');
			return;
		}
		try {
			if (!authConfigured) {
				if (typeof location !== 'undefined') location.assign('/login');
				return;
			}
			const client = await getMsal();
			const account = client.getActiveAccount() ?? client.getAllAccounts()[0] ?? undefined;
			await client.logoutRedirect({
				account,
				postLogoutRedirectUri: msalConfig.postLogoutRedirectUri
			});
		} catch {
			if (typeof location !== 'undefined') location.assign('/login');
		}
	}

	/** Validate and store the one-time profile. Returns field errors. */
	saveProfile(input: StudentProfileInput): ProfileErrors {
		const errors = validateProfile(input);
		if (Object.keys(errors).length > 0) return errors;
		if (!this.account) return { major: 'Sign in again to save your profile.' };
		const cleaned: StudentProfileInput = {
			name: input.name?.trim() || undefined,
			level: input.level,
			major: input.major.trim(),
			aucId: input.aucId.trim(),
			phone: input.phone.trim()
		};
		writeProfile(this.account.homeAccountId, cleaned);
		this.profile = cleaned;
		return {};
	}

	/** Keep auth.profile (and displayName/needsProfile) in sync with GET/PUT /api/me. */
	syncFromServerProfile(
		server: {
			name?: string;
			level?: string;
			major?: string;
			aucId?: string;
			phone?: string;
		} | null
	): void {
		if (!this.account || this.role !== 'student') return;
		if (!server) {
			this.profile = null;
			removeProfile(this.account.homeAccountId);
			return;
		}
		const candidate: StudentProfileInput = {
			name: server.name?.trim() || undefined,
			level: server.level === 'UG' || server.level === 'GR' ? server.level : '',
			major: server.major?.trim() ?? '',
			aucId: server.aucId?.trim() ?? '',
			phone: server.phone?.trim() ?? ''
		};
		if (isProfileComplete(candidate)) {
			this.profile = candidate;
			writeProfile(this.account.homeAccountId, candidate);
		} else {
			this.profile = null;
			removeProfile(this.account.homeAccountId);
		}
	}

	clearError(): void {
		this.error = null;
	}

	/** The destination the person was trying to reach before signing in. */
	takePendingNext(): string | null {
		const next = readSession(NEXT_KEY);
		writeSession(NEXT_KEY, '');
		return isSafeNext(next) ? next : null;
	}

	/** Which tab started the sign-in. */
	takePendingKind(): LoginKind | null {
		const kind = readSession(AS_KEY);
		writeSession(AS_KEY, '');
		return kind === 'student' || kind === 'admin' ? kind : null;
	}

	rememberNext(next: string | null): void {
		if (next) writeSession(NEXT_KEY, next);
	}

	/** A Graph token is not requested yet: it needs an API scope from AUC IT. */
	async getToken(client?: PublicClientApplication): Promise<string | null> {
		if (!this.account) return null;
		try {
			const instance = client ?? (await getMsal());
			const result = await instance.acquireTokenSilent({
				scopes: loginScopes,
				account: this.account
			});
			return result.accessToken ?? null;
		} catch {
			return null;
		}
	}
}

/** One shared auth store for the whole app. */
export const auth = new AuthStore();

export type { Role };
