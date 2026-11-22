import { describe, expect, it } from 'vitest';
import { demoMetrics, hashSeed, type FitProfile } from './metrics.ts';
import type { Sourced, UniversityRecord } from './universities.ts';

function sourced<T>(value: T | null): Sourced<T> {
	return {
		value,
		status: 'verified',
		source_type: 'excel_list',
		source_ref: null,
		notes: null
	};
}

type Fixture = Partial<{
	slug: string;
	agreement_status: string;
	levels: string[];
	minGpa: number | null;
	tuition: string;
	living: { currency: string; min: number; max: number } | null;
	docs: string[] | null;
	english: string | null;
}>;

function record(fixture: Fixture = {}): UniversityRecord {
	return {
		slug: fixture.slug ?? 'demo-university',
		agreement_status: sourced(fixture.agreement_status ?? 'active'),
		agreement_student_levels: sourced(fixture.levels ?? ['UG', 'GR']),
		min_gpa_host: sourced(fixture.minGpa === undefined ? 2.5 : fixture.minGpa),
		tuition_model: sourced(fixture.tuition ?? 'auc_exchange_waiver'),
		est_monthly_living_local: sourced(fixture.living === undefined ? null : fixture.living),
		required_application_docs: sourced(fixture.docs === undefined ? null : fixture.docs),
		english_proficiency_and_rhet1020: sourced(
			fixture.english === undefined ? 'IELTS 6.5' : fixture.english
		)
	} as unknown as UniversityRecord;
}

const strongStudent: FitProfile = {
	level: 'UG',
	gpa: 3.9,
	englishComplete: false,
	rhet120: true
};

describe('demoMetrics', () => {
	it('is deterministic for the same slug', () => {
		const first = demoMetrics(record({ slug: 'aarhus-university' }), strongStudent, 7000);
		const second = demoMetrics(record({ slug: 'aarhus-university' }), strongStudent, 7000);
		expect(first).toEqual(second);
	});

	it('changes values between slugs', () => {
		const first = demoMetrics(record({ slug: 'one-university' }));
		const second = demoMetrics(record({ slug: 'two-university' }));
		expect(first).not.toEqual(second);
	});

	it('keeps fit inside 35–98 even with an extreme profile', () => {
		const perfect = demoMetrics(record(), {
			level: 'UG',
			gpa: 4,
			englishComplete: true
		});
		const weak = demoMetrics(
			record({ minGpa: 4 }),
			{ level: 'GR', gpa: 0, englishComplete: false },
			0
		);

		expect(perfect.fit).toBeLessThanOrEqual(98);
		expect(perfect.fit).toBeGreaterThanOrEqual(35);
		expect(weak.fit).toBeGreaterThanOrEqual(35);
		expect(weak.fit).toBeLessThanOrEqual(98);
	});

	it('never lowers fit when the GPA is higher', () => {
		const low = demoMetrics(record(), {
			level: 'UG',
			gpa: 2.6,
			englishComplete: false,
			rhet120: true
		});
		const high = demoMetrics(record(), {
			level: 'UG',
			gpa: 3.9,
			englishComplete: false,
			rhet120: true
		});

		expect(high.fit).toBeGreaterThanOrEqual(low.fit);
	});

	it('marks a school as qualified only when level, GPA and English all pass', () => {
		const ugOnly = record({ levels: ['UG'], minGpa: 2.5 });
		expect(demoMetrics(ugOnly, strongStudent).qualification).toBe(true);

		const graduateOnly = record({ levels: ['GR'], minGpa: 2.5 });
		expect(demoMetrics(graduateOnly, strongStudent).qualification).toBe(false);

		const noEnglish = record({ levels: ['UG'], minGpa: 2.5 });
		expect(
			demoMetrics(noEnglish, {
				level: 'UG',
				gpa: 3.9,
				englishComplete: false,
				rhet120: false
			}).qualification
		).toBe(false);
	});

	it('counts RHET 1020 as the undergraduate English requirement', () => {
		const profile: FitProfile = {
			level: 'UG',
			gpa: 3.5,
			englishComplete: false,
			rhet120: true
		};
		expect(demoMetrics(record(), profile).qualification).toBe(true);
	});

	it('does not charge tuition for an exchange waiver', () => {
		expect(demoMetrics(record({ tuition: 'auc_exchange_waiver' })).tuitionUsd).toBe(0);
		expect(
			demoMetrics(record({ tuition: 'fee_paying_study_abroad' })).tuitionUsd
		).toBeGreaterThan(0);
	});

	it('adds the within-budget bonus to fit', () => {
		const profile: FitProfile = {
			level: 'UG',
			gpa: 3.0,
			englishComplete: false,
			rhet120: true
		};
		const metrics = demoMetrics(record(), profile, 0);
		const withBudget = demoMetrics(record(), profile, metrics.semesterCostUsd + 1);

		expect(withBudget.fit).toBeGreaterThan(metrics.fit);
	});

	it('hashes the same text to the same number', () => {
		expect(hashSeed('aarhus-university')).toBe(hashSeed('aarhus-university'));
		expect(hashSeed('aarhus-university')).not.toBe(hashSeed('bocconi-university'));
	});
});
