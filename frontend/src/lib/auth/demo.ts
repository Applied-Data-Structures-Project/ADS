// Demo identity for showing the portal before the Entra app registration exists.
//
// This is NOT authentication. It exists only while PUBLIC_DEMO_AUTH is on and
// never in a production build, so the product can be demonstrated end to end
// without AUC IT. Everything the real sign-in produces has the same shape, so
// the guards, headers and profile step work unchanged.

import type { AccountInfo } from '@azure/msal-browser';
import type { StudentProfileInput } from './profile.ts';
import type { Role } from './roles.ts';

export const DEMO_ROLE_KEY = 'auc-demo-role';

/** The roles offered on the demo panel, in display order. */
export const DEMO_ROLES: Role[] = ['student', 'admin', 'faculty'];

/** Demo mode needs an explicit flag and must never run in a production build. */
export function resolveDemoMode(flag: string | undefined, isProd: boolean): boolean {
	return flag === 'true' && !isProd;
}

export function isDemoRole(value: unknown): value is Role {
	return DEMO_ROLES.some((role) => role === value);
}

/** The roles claim a demo identity presents, so `roleFromRoles` derives the role. */
export function demoRolesClaim(role: Role): string[] {
	if (role === 'admin') return ['Admin'];
	if (role === 'faculty') return ['Faculty'];
	return [];
}

export function demoLabel(role: Role): string {
	if (role === 'admin') return 'Administrator';
	if (role === 'faculty') return 'Faculty';
	return 'Student';
}

const DEMO_PEOPLE: Record<Role, { name: string; username: string; id: string }> = {
	student: { name: 'Salma Abdelrahman', username: 's.abdelrahman@aucegypt.edu', id: 'demo-student' },
	admin: { name: 'Nadia Hassan', username: 'n.hassan@aucegypt.edu', id: 'demo-admin' },
	faculty: { name: 'Dr. Youssef Kamal', username: 'y.kamal@aucegypt.edu', id: 'demo-faculty' }
};

/** A Microsoft-shaped account for a demo role. */
export function demoAccount(role: Role): AccountInfo {
	const person = DEMO_PEOPLE[role];
	return {
		homeAccountId: person.id,
		environment: 'demo',
		tenantId: 'demo-tenant',
		localAccountId: person.id,
		username: person.username,
		name: person.name,
		idTokenClaims: { roles: demoRolesClaim(role) }
	};
}

/** A complete profile so a demo identity lands on its home, not on /signup. */
export function demoProfile(): StudentProfileInput {
	return {
		level: 'UG',
		major: 'Economics (ECON)',
		aucId: '900219004',
		phone: '+20 100 555 2190'
	};
}
