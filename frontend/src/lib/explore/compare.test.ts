import { describe, expect, it } from 'vitest';
import { bestColumns, COMPARE_ROWS } from './compare.ts';
import { demoMetrics } from './metrics.ts';
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

function record(slug: string, city: string): UniversityRecord {
	return {
		slug,
		official_name: sourced(slug),
		short_name: sourced(slug),
		city: sourced(city),
		country: sourced('Demo'),
		agreement_status: sourced('active'),
		agreement_student_levels: sourced(['UG']),
		agreement_scope_raw: sourced('University-wide'),
		min_gpa_host: sourced(2.5),
		tuition_model: sourced('auc_exchange_waiver'),
		est_monthly_living_local: sourced(null),
		required_application_docs: sourced(['A', 'B', 'C']),
		english_proficiency_and_rhet1020: sourced('IELTS 6.5')
	} as unknown as UniversityRecord;
}

describe('bestColumns', () => {
	it('picks the highest value', () => {
		expect(bestColumns([1, 9, 4], 'high')).toEqual([false, true, false]);
	});

	it('picks the lowest value', () => {
		expect(bestColumns([100, 20, 55], 'low')).toEqual([false, true, false]);
	});

	it('picks the latest timestamp', () => {
		expect(bestColumns([10, 30, 20], 'latest')).toEqual([false, true, false]);
	});

	it('highlights every tied winner', () => {
		expect(bestColumns([5, 5, 2], 'high')).toEqual([true, true, false]);
	});

	it('never lets a missing value win', () => {
		expect(bestColumns([null, 7, null], 'high')).toEqual([false, true, false]);
		expect(bestColumns([null, null], 'low')).toEqual([false, false]);
	});

	it('marks nothing when a row is informational', () => {
		expect(bestColumns([1, 2, 3], 'none')).toEqual([false, false, false]);
	});
});

describe('COMPARE_ROWS', () => {
	const columns = [
		{ record: record('alpha', 'Athens'), metrics: demoMetrics(record('alpha', 'Athens')) },
		{ record: record('beta', 'Berlin'), metrics: demoMetrics(record('beta', 'Berlin')) }
	];

	it('gives every row a valid direction', () => {
		const directions = ['high', 'low', 'latest', 'earliest', 'none'];
		for (const row of COMPARE_ROWS) {
			expect(directions).toContain(row.direction);
		}
	});

	it('produces a value and label for each column', () => {
		for (const row of COMPARE_ROWS) {
			for (const column of columns) {
				expect(() =>
					row.value(column.record, column.metrics, 5000)
				).not.toThrow();
				expect(row.format(column.record, column.metrics, 5000).length).toBeGreaterThan(0);
			}
		}
	});

	it('keeps city and budget informational', () => {
		const city = COMPARE_ROWS.find((row) => row.id === 'city');
		const budget = COMPARE_ROWS.find((row) => row.id === 'budget');

		expect(city?.direction).toBe('none');
		expect(budget?.direction).toBe('none');
	});

	it('does not mark "not confirmed" as a best qualification', () => {
		const qualification = COMPARE_ROWS.find((row) => row.id === 'qualification');
		const metrics = demoMetrics(record('alpha', 'Athens'));

		expect(qualification?.value(record('alpha', 'Athens'), metrics, 0)).toBeNull();
		expect(bestColumns([null, null], 'high')).toEqual([false, false]);
	});

	it('describes over-budget and under-budget states', () => {
		const budgetRow = COMPARE_ROWS.find((row) => row.id === 'budget');
		const metrics = demoMetrics(record('alpha', 'Athens'));

		expect(budgetRow?.format(record('alpha', 'Athens'), metrics, 0)).toBe('Set a budget on Me');
		expect(
			budgetRow?.format(record('alpha', 'Athens'), metrics, metrics.semesterCostUsd + 500)
		).toContain('under');
		expect(
			budgetRow?.format(record('alpha', 'Athens'), metrics, metrics.semesterCostUsd - 500)
		).toContain('over');
	});
});
