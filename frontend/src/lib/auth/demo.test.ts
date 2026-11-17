import { describe, expect, it } from 'vitest';
import { demoAccount, demoProfile, demoRolesClaim, resolveDemoMode } from './demo.ts';
import { isProfileComplete } from './profile.ts';
import { roleFromRoles } from './roles.ts';

describe('resolveDemoMode', () => {
	it('is on only for an explicit true in a non-production build', () => {
		expect(resolveDemoMode('true', false)).toBe(true);
	});

	it('is off in production even when flagged', () => {
		expect(resolveDemoMode('true', true)).toBe(false);
	});

	it('is off for anything else', () => {
		expect(resolveDemoMode('false', false)).toBe(false);
		expect(resolveDemoMode('TRUE', false)).toBe(false);
		expect(resolveDemoMode('', false)).toBe(false);
		expect(resolveDemoMode(undefined, false)).toBe(false);
	});
});

describe('demo identity', () => {
	it('maps each role to its Entra roles claim', () => {
		expect(roleFromRoles(demoRolesClaim('admin'))).toBe('admin');
		expect(roleFromRoles(demoRolesClaim('faculty'))).toBe('faculty');
		expect(roleFromRoles(demoRolesClaim('student'))).toBe('student');
	});

	it('uses an AUC address', () => {
		expect(demoAccount('student').username.endsWith('@aucegypt.edu')).toBe(true);
	});

	it('supplies a complete profile', () => {
		expect(isProfileComplete(demoProfile())).toBe(true);
	});
});
