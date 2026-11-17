import { describe, expect, it } from 'vitest';
import { isProfileComplete, profileKey, validateProfile, type StudentProfileInput } from './profile.ts';

const valid: StudentProfileInput = {
	level: 'UG',
	major: 'Economics (ECON)',
	aucId: '900219004',
	phone: '+20 100 555 2190'
};

describe('validateProfile', () => {
	it('accepts a complete profile', () => {
		expect(validateProfile(valid)).toEqual({});
	});

	it('requires a level', () => {
		expect(validateProfile({ ...valid, level: '' })).toHaveProperty('level');
	});

	it('requires a major of at least two characters', () => {
		expect(validateProfile({ ...valid, major: ' ' })).toHaveProperty('major');
	});

	it('requires exactly nine digits for the AUC ID', () => {
		expect(validateProfile({ ...valid, aucId: '90021' })).toHaveProperty('aucId');
		expect(validateProfile({ ...valid, aucId: '90021900x' })).toHaveProperty('aucId');
	});

	it('requires a phone with country code length', () => {
		expect(validateProfile({ ...valid, phone: '123' })).toHaveProperty('phone');
	});
});

describe('isProfileComplete', () => {
	it('is false for null or invalid profiles', () => {
		expect(isProfileComplete(null)).toBe(false);
		expect(isProfileComplete({ ...valid, major: '' })).toBe(false);
	});

	it('is true for a valid profile', () => {
		expect(isProfileComplete(valid)).toBe(true);
	});
});

describe('profileKey', () => {
	it('scopes the stored profile to the Microsoft account', () => {
		expect(profileKey('abc-123')).toBe('auc-profile:abc-123');
	});
});
