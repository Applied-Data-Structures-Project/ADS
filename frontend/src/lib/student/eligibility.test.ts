import { describe, expect, it } from 'vitest';
import {
	evaluateEligibility,
	isEligibilityComplete,
	validateEligibilityProfile,
	type EligibilityProfile
} from './eligibility.ts';

function ug(overrides: Partial<EligibilityProfile> = {}): EligibilityProfile {
	return {
		level: 'UG',
		major: 'Economics (ECON)',
		gpa: 3.2,
		credits: 30,
		majorDeclared: true,
		rhet110: true,
		rhet120: true,
		finalSemester: false,
		academicProbation: false,
		disciplinaryProbation: false,
		englishComplete: false,
		firstSemesterNoGpa: false,
		...overrides
	};
}

function gr(overrides: Partial<EligibilityProfile> = {}): EligibilityProfile {
	return {
		level: 'GR',
		major: 'Economics (ECON)',
		gpa: 3.4,
		credits: 0,
		majorDeclared: true,
		rhet110: false,
		rhet120: false,
		finalSemester: false,
		academicProbation: false,
		disciplinaryProbation: false,
		englishComplete: true,
		firstSemesterNoGpa: false,
		...overrides
	};
}

describe('validateEligibilityProfile', () => {
	it('accepts a complete undergraduate profile', () => {
		expect(validateEligibilityProfile(ug())).toEqual({});
		expect(isEligibilityComplete(ug())).toBe(true);
	});

	it('requires a level, a major, a GPA and completed credits', () => {
		const errors = validateEligibilityProfile(ug({ level: '', major: ' ', gpa: null, credits: null }));
		expect(errors.level).toBeTruthy();
		expect(errors.major).toBeTruthy();
		expect(errors.gpa).toBeTruthy();
		expect(errors.credits).toBeTruthy();
	});

	it('rejects a GPA outside 0.00–4.00', () => {
		expect(validateEligibilityProfile(ug({ gpa: 4.1 })).gpa).toBeTruthy();
		expect(validateEligibilityProfile(ug({ gpa: -0.1 })).gpa).toBeTruthy();
	});

	it('rejects fractional or negative credits', () => {
		expect(validateEligibilityProfile(ug({ credits: 12.5 })).credits).toBeTruthy();
		expect(validateEligibilityProfile(ug({ credits: -1 })).credits).toBeTruthy();
	});

	it('allows a graduate first-semester profile without a GPA', () => {
		expect(validateEligibilityProfile(gr({ gpa: null, firstSemesterNoGpa: true }))).toEqual({});
	});

	it('still requires a GPA for an undergraduate first-semester profile', () => {
		expect(validateEligibilityProfile(ug({ gpa: null, firstSemesterNoGpa: true })).gpa).toBeTruthy();
	});
});

describe('evaluateEligibility — undergraduate', () => {
	it('passes when every rule is met', () => {
		const result = evaluateEligibility(ug());
		expect(result.eligible).toBe(true);
		expect(result.provisional).toBe(false);
		expect(result.checks.every((check) => check.ok)).toBe(true);
	});

	it('fails below the 2.8 GPA minimum', () => {
		const result = evaluateEligibility(ug({ gpa: 2.79 }));
		expect(result.eligible).toBe(false);
		expect(result.checks.find((check) => check.label.includes('GPA'))?.ok).toBe(false);
	});

	it('fails below 24 completed credits', () => {
		const result = evaluateEligibility(ug({ credits: 23 }));
		expect(result.eligible).toBe(false);
		expect(result.checks.find((check) => check.label.includes('24'))?.ok).toBe(false);
	});

	it('fails when the major is not declared', () => {
		expect(evaluateEligibility(ug({ majorDeclared: false })).eligible).toBe(false);
	});

	it('fails when RHET 110/CORE 110 is missing', () => {
		expect(evaluateEligibility(ug({ rhet110: false })).eligible).toBe(false);
	});

	it('fails when RHET 120/1020 is missing', () => {
		expect(evaluateEligibility(ug({ rhet120: false })).eligible).toBe(false);
	});

	it('fails in the final semester', () => {
		expect(evaluateEligibility(ug({ finalSemester: true })).eligible).toBe(false);
	});

	it('fails on academic probation', () => {
		expect(evaluateEligibility(ug({ academicProbation: true })).eligible).toBe(false);
	});

	it('fails on disciplinary probation', () => {
		expect(evaluateEligibility(ug({ disciplinaryProbation: true })).eligible).toBe(false);
	});
});

describe('evaluateEligibility — graduate', () => {
	it('passes at or above 3.2 with English requirements complete', () => {
		const result = evaluateEligibility(gr());
		expect(result.eligible).toBe(true);
		expect(result.provisional).toBe(false);
	});

	it('fails below the 3.2 GPA minimum', () => {
		expect(evaluateEligibility(gr({ gpa: 3.19 })).eligible).toBe(false);
	});

	it('fails when required English courses are incomplete', () => {
		expect(evaluateEligibility(gr({ englishComplete: false })).eligible).toBe(false);
	});

	it('is provisional for a first semester with no GPA', () => {
		const result = evaluateEligibility(gr({ gpa: null, firstSemesterNoGpa: true }));
		expect(result.eligible).toBe(true);
		expect(result.provisional).toBe(true);
		expect(result.checks.find((check) => check.label.includes('GPA'))?.provisional).toBe(true);
	});

	it('is not provisional when another rule fails', () => {
		const result = evaluateEligibility(gr({ gpa: null, firstSemesterNoGpa: true, englishComplete: false }));
		expect(result.eligible).toBe(false);
		expect(result.provisional).toBe(false);
	});

	it('fails on academic probation', () => {
		expect(evaluateEligibility(gr({ academicProbation: true })).eligible).toBe(false);
	});

	it('fails on disciplinary probation', () => {
		expect(evaluateEligibility(gr({ disciplinaryProbation: true })).eligible).toBe(false);
	});
});
