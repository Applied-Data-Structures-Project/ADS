import { describe, expect, it } from 'vitest';
import { evaluateUniversityEligibility } from './eligibility.ts';
import type { Sourced, UniversityRecord } from './universities.ts';
import type { EligibilityProfile } from '../student/eligibility.ts';

function s<T>(value: T): Sourced<T> {
	return { value, status: 'verified', source_type: 'fact_sheet', source_ref: null, notes: null };
}

function empty<T>(): Sourced<T> {
	return { value: null, status: 'unknown', source_type: 'unknown', source_ref: null, notes: null };
}

function makeUniversity(overrides: Partial<UniversityRecord> = {}): UniversityRecord {
	return {
		slug: 'test-university',
		official_name: s('Test University'),
		short_name: s('TU'),
		country: s('Denmark'),
		city: s('Aarhus'),
		region: s('Europe'),
		agreement_status: s('active'),
		academic_style: empty(),
		campus_type: empty(),
		international_support_services: empty(),
		host_grants_or_stipends: empty(),
		min_standing_note: empty(),
		fall_term_dates: empty(),
		spring_term_dates: empty(),
		mandatory_host_fees_and_insurance: empty(),
		credit_system_and_load: empty(),
		eligible_auc_schools: s([]),
		agreement_student_levels: s(['UG', 'GR']),
		agreement_scope_raw: s('University-wide'),
		eligible_majors: s(['ALL']),
		excluded_majors: s([]),
		min_gpa_host: s(3.0),
		instruction_languages: s(['English']),
		english_proficiency_and_rhet1020: empty(),
		host_language_requirement: empty(),
		available_terms: s(['Fall', 'Spring']),
		tuition_model: s('auc_exchange_waiver'),
		est_monthly_living_local: empty(),
		housing_availability: empty(),
		housing_notes: empty(),
		visa_lead_time_and_financial_proof: empty(),
		required_application_docs: s([]),
		official_website_url: empty(),
		exchange_page_url: empty(),
		course_catalog_url: empty(),
		course_restrictions_note: empty(),
		nomination_deadlines: empty(),
		...overrides
	};
}

const eligibleUG: EligibilityProfile = {
	level: 'UG',
	major: 'Computer Science (B.S.)',
	gpa: 3.4,
	credits: 60,
	majorDeclared: true,
	rhet110: true,
	rhet120: true,
	finalSemester: false,
	academicProbation: false,
	disciplinaryProbation: false,
	englishComplete: false,
	firstSemesterNoGpa: false
};

describe('evaluateUniversityEligibility', () => {
	it('returns unknown when no profile exists', () => {
		const res = evaluateUniversityEligibility(makeUniversity(), null);
		expect(res.status).toBe('unknown');
	});

	it('marks an eligible UG student as eligible for an open university', () => {
		const res = evaluateUniversityEligibility(makeUniversity(), eligibleUG);
		expect(res.status).toBe('eligible');
		expect(res.reasons).toEqual([]);
	});

	it('marks a student ineligible if AUC IPSO rules fail', () => {
		const res = evaluateUniversityEligibility(makeUniversity(), {
			...eligibleUG,
			academicProbation: true
		});
		expect(res.status).toBe('ineligible');
		expect(res.reason).toContain('Good academic standing');
	});

	it('checks host student level', () => {
		const grOnly = makeUniversity({ agreement_student_levels: s(['GR']) });
		const res = evaluateUniversityEligibility(grOnly, eligibleUG);
		expect(res.status).toBe('ineligible');
		expect(res.reason).toContain('Accepts GR only');
	});

	it('checks verified host minimum GPA', () => {
		const strictGpa = makeUniversity({ min_gpa_host: s(3.6) });
		const res = evaluateUniversityEligibility(strictGpa, eligibleUG);
		expect(res.status).toBe('ineligible');
		expect(res.reason).toContain('3.60');
	});

	it('enforces specialized eligible_majors and matches related AUC majors', () => {
		const businessSchool = makeUniversity({ eligible_majors: s(['Business', 'Economics']) });

		const csResult = evaluateUniversityEligibility(businessSchool, eligibleUG);
		expect(csResult.status).toBe('ineligible');
		expect(csResult.reason).toContain('Restricted to Business, Economics');

		const econResult = evaluateUniversityEligibility(businessSchool, {
			...eligibleUG,
			major: 'Economics (B.A.)'
		});
		expect(econResult.status).toBe('eligible');

		const financeResult = evaluateUniversityEligibility(businessSchool, {
			...eligibleUG,
			major: 'Business in Finance (B.B.F.)'
		});
		expect(financeResult.status).toBe('eligible');
	});

	it('respects excluded_majors without falsely blocking UG students on graduate exclusions', () => {
		const ugOnlyGradExcluded = makeUniversity({ excluded_majors: s(['Graduate-level programs']) });
		expect(evaluateUniversityEligibility(ugOnlyGradExcluded, eligibleUG).status).toBe('eligible');

		const lawExcluded = makeUniversity({ excluded_majors: s(['School of Law']) });
		expect(evaluateUniversityEligibility(lawExcluded, eligibleUG).status).toBe('eligible');
		expect(
			evaluateUniversityEligibility(lawExcluded, { ...eligibleUG, major: 'Law' }).status
		).toBe('ineligible');
	});

	it('marks GR first-semester without GPA as provisionally eligible', () => {
		const grProvisional: EligibilityProfile = {
			level: 'GR',
			major: 'Economics',
			gpa: null,
			credits: 0,
			majorDeclared: false,
			rhet110: false,
			rhet120: false,
			finalSemester: false,
			academicProbation: false,
			disciplinaryProbation: false,
			englishComplete: true,
			firstSemesterNoGpa: true
		};
		const res = evaluateUniversityEligibility(makeUniversity(), grProvisional);
		expect(res.status).toBe('provisional');
	});
});
