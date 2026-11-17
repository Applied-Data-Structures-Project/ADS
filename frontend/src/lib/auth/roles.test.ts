import { describe, expect, it } from 'vitest';
import { isAucEmail, isSafeNext, roleFromRoles, roleHome } from './roles.ts';

describe('isAucEmail', () => {
	it('accepts an AUC student address', () => {
		expect(isAucEmail('s.abdelrahman@aucegypt.edu')).toBe(true);
	});

	it('is case-insensitive', () => {
		expect(isAucEmail('S.Abdelrahman@AUCegypt.EDU')).toBe(true);
	});

	it('rejects other domains and empty values', () => {
		expect(isAucEmail('someone@gmail.com')).toBe(false);
		expect(isAucEmail('')).toBe(false);
		expect(isAucEmail(null)).toBe(false);
		expect(isAucEmail(undefined)).toBe(false);
	});
});

describe('roleFromRoles', () => {
	it('reads the Admin app role case-insensitively', () => {
		expect(roleFromRoles(['Admin'])).toBe('admin');
		expect(roleFromRoles(['admin'])).toBe('admin');
	});

	it('reads the Faculty app role', () => {
		expect(roleFromRoles(['Faculty'])).toBe('faculty');
	});

	it('defaults an unassigned account to student', () => {
		expect(roleFromRoles([])).toBe('student');
		expect(roleFromRoles(undefined)).toBe('student');
	});

	it('lets Admin win when both staff roles are present', () => {
		expect(roleFromRoles(['Faculty', 'Admin'])).toBe('admin');
	});
});

describe('roleHome', () => {
	it('sends each role to its home', () => {
		expect(roleHome('admin')).toBe('/admin');
		expect(roleHome('faculty')).toBe('/doctor');
		expect(roleHome('student')).toBe('/apply');
	});
});

describe('isSafeNext', () => {
	it('accepts an internal path', () => {
		expect(isSafeNext('/apply/status?tab=1')).toBe(true);
	});

	it('rejects absolute, protocol-relative, backslash and empty values', () => {
		expect(isSafeNext('https://evil.example')).toBe(false);
		expect(isSafeNext('//evil.example')).toBe(false);
		expect(isSafeNext('/\\evil.example')).toBe(false);
		expect(isSafeNext('apply')).toBe(false);
		expect(isSafeNext('')).toBe(false);
		expect(isSafeNext(null)).toBe(false);
		expect(isSafeNext(undefined)).toBe(false);
	});
});
