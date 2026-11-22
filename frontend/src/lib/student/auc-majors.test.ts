import { describe, expect, it } from 'vitest';
import {
	AUC_MAJORS,
	findAucMajor,
	groupedMajorsForLevel,
	isValidAucMajor,
	majorsForLevel
} from './auc-majors.ts';

describe('auc-majors', () => {
	it('contains UG and GR majors grouped by AUC school', () => {
		expect(AUC_MAJORS.length).toBeGreaterThan(20);
		const ugGroups = groupedMajorsForLevel('UG');
		const grGroups = groupedMajorsForLevel('GR');
		expect(ugGroups.length).toBeGreaterThanOrEqual(4);
		expect(grGroups.length).toBeGreaterThanOrEqual(4);
		expect(majorsForLevel('UG').every((m) => m.levels.includes('UG'))).toBe(true);
		expect(majorsForLevel('GR').every((m) => m.levels.includes('GR'))).toBe(true);
	});

	it('validates official AUC majors and rejects arbitrary free text', () => {
		expect(isValidAucMajor('Economics (B.A.)', 'UG')).toBe(true);
		expect(isValidAucMajor('Computer Science (B.S.)', 'UG')).toBe(true);
		expect(isValidAucMajor('Business Administration (M.B.A.)', 'GR')).toBe(true);
		expect(isValidAucMajor('Applied Sciences (Ph.D.)', 'GR')).toBe(true);
		expect(isValidAucMajor('Business Administration (M.B.A.)', 'UG')).toBe(false);
		expect(isValidAucMajor('Arabic Studies (Graduate Diploma)', 'GR')).toBe(false);
		expect(isValidAucMajor('Random Unlisted Major', 'UG')).toBe(false);
		expect(isValidAucMajor('', 'UG')).toBe(false);
	});

	it('looks up domains and school for known majors', () => {
		const econ = findAucMajor('Economics (B.A.)');
		expect(econ?.school).toBe('School of Business');
		expect(econ?.domains).toContain('Economics');
	});
});
