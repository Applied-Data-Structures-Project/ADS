// Rows and "best value" rules for the comparison table.

import type { UniversityRecord } from './universities.ts';
import { formatDeadline, formatUsd, type DemoMetrics } from './metrics.ts';

export type BestDirection = 'high' | 'low' | 'latest' | 'earliest' | 'none';

/**
 * Marks the winning column(s) for one row.
 * Missing values (null) never win, and ties highlight every winner.
 */
export function bestColumns(
	values: readonly (number | null)[],
	direction: BestDirection
): boolean[] {
	if (direction === 'none') return values.map(() => false);

	const present = values.filter((value): value is number => value !== null && Number.isFinite(value));
	if (present.length === 0) return values.map(() => false);

	const best =
		direction === 'high' || direction === 'latest'
			? Math.max(...present)
			: Math.min(...present);
	return values.map((value) => value !== null && value === best);
}

export interface CompareRow {
	id: string;
	label: string;
	/** Which direction wins this row. `none` means the row is informational. */
	direction: BestDirection;
	value: (record: UniversityRecord, metrics: DemoMetrics, budget: number) => number | null;
	format: (record: UniversityRecord, metrics: DemoMetrics, budget: number) => string;
}

export const COMPARE_ROWS: CompareRow[] = [
	{
		id: 'qualification',
		label: 'You qualify (sample)',
		direction: 'high',
		// null (not false) so "not confirmed" cells are never highlighted as best.
		value: (_record, metrics) => (metrics.qualification ? 1 : null),
		format: (_record, metrics) => (metrics.qualification ? 'Yes' : 'Not confirmed')
	},
	{
		id: 'fit',
		label: 'Fit score',
		direction: 'high',
		value: (_record, metrics) => metrics.fit,
		format: (_record, metrics) => `${metrics.fit}%`
	},
	{
		id: 'city',
		label: 'City',
		direction: 'none',
		value: () => null,
		format: (record) =>
			[record.city.value, record.country.value].filter(Boolean).join(', ') || 'Not available'
	},
	{
		id: 'tuition',
		label: 'Tuition (sample)',
		direction: 'low',
		value: (_record, metrics) => metrics.tuitionUsd,
		format: (_record, metrics) =>
			metrics.tuitionUsd === 0 ? 'Exchange waiver' : formatUsd(metrics.tuitionUsd)
	},
	{
		id: 'living',
		label: 'Living (monthly)',
		direction: 'low',
		value: (_record, metrics) => metrics.livingMonthlyUsd,
		format: (_record, metrics) => `${formatUsd(metrics.livingMonthlyUsd)}/mo`
	},
	{
		id: 'semester',
		label: 'Semester cost (sample)',
		direction: 'low',
		value: (_record, metrics) => metrics.semesterCostUsd,
		format: (_record, metrics) => formatUsd(metrics.semesterCostUsd)
	},
	{
		id: 'budget',
		label: 'Vs. your budget',
		direction: 'none',
		value: () => null,
		format: (_record, metrics, budget) => {
			if (!(budget > 0)) return 'Set a budget on Me';
			const difference = budget - metrics.semesterCostUsd;
			return difference >= 0
				? `${formatUsd(difference)} under`
				: `${formatUsd(-difference)} over`;
		}
	},
	{
		id: 'gpa',
		label: 'GPA needed',
		direction: 'low',
		value: (_record, metrics) => metrics.gpaRequired,
		format: (_record, metrics) => metrics.gpaRequired.toFixed(2)
	},
	{
		id: 'ielts',
		label: 'IELTS needed (sample)',
		direction: 'low',
		value: (_record, metrics) => metrics.ielts,
		format: (_record, metrics) => metrics.ielts.toFixed(1)
	},
	{
		id: 'deadline',
		label: 'Deadline (sample)',
		direction: 'latest',
		value: (_record, metrics) => Date.parse(metrics.deadline),
		format: (_record, metrics) => formatDeadline(metrics.deadline)
	},
	{
		id: 'seats',
		label: 'Seats (sample)',
		direction: 'high',
		value: (_record, metrics) => metrics.seats,
		format: (_record, metrics) => `${metrics.seats}`
	},
	{
		id: 'papers',
		label: 'Required papers',
		direction: 'low',
		value: (_record, metrics) => metrics.papers,
		format: (_record, metrics) => `${metrics.papers} items`
	}
];
