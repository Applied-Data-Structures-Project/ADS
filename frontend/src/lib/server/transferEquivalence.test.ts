import { describe, expect, it } from 'vitest';
import {
	buildInstitutionSearchVariants,
	cleanBannerHtml,
	extractAucDepartments,
	filterEquivalencies,
	formatBannerTerm,
	normalizeCourseRow,
	parseHostCourseGroup
} from './transferEquivalence.ts';

describe('transferEquivalence helpers', () => {
	it('strips Banner inline HTML and decodes entities', () => {
		expect(
			cleanBannerHtml(
				'MGMT 5202 Managing Org &amp; Teams, cr.hrs.: 2.5 <b><u>OR</u></b> PPAD 5199 Special Topics'
			)
		).toBe('MGMT 5202 Managing Org & Teams, cr.hrs.: 2.5 OR PPAD 5199 Special Topics');
	});

	it('parses host course code and title from COURSE_GROUP', () => {
		expect(parseHostCourseGroup('(FA 341 History of the Cinema)')).toEqual({
			code: 'FA 341',
			title: 'History of the Cinema',
			cleanRaw: '(FA 341 History of the Cinema)'
		});

		expect(parseHostCourseGroup('(ADV PLAN Adv Planning&Schedulng/Project)')).toEqual({
			code: 'ADV PLAN',
			title: 'Adv Planning&Schedulng/Project',
			cleanRaw: '(ADV PLAN Adv Planning&Schedulng/Project)'
		});
	});

	it('formats Banner term codes into readable semesters', () => {
		expect(formatBannerTerm('202510')).toBe('Fall 2025');
		expect(formatBannerTerm('202620')).toBe('Spring 2026');
		expect(formatBannerTerm('202530')).toBe('Summer 2025');
		expect(formatBannerTerm('')).toBe('Unspecified');
	});

	it('extracts unique AUC department prefixes and flags No equivalent course', () => {
		expect(
			extractAucDepartments(
				'MGMT 5202 Managing Org, cr.hrs.: 2.5 OR PPAD 5199 Special Topics OR CENG 5--- Elective'
			)
		).toEqual(['MGMT', 'PPAD', 'CENG']);

		const noEq = normalizeCourseRow(
			{
				SHBTATC_TERM_CODE_EFF_TRNS: '202320',
				SHBTATC_SBGI_CODE: '200063',
				COURSE_GROUP: '(COM 101 Public Speaking)',
				AUC_EQUIVALENT_COURSE: 'No equivalent course'
			},
			0
		);
		expect(noEq?.hasEquivalent).toBe(false);
		expect(noEq?.aucDepartments).toEqual([]);
	});

	it('builds sensible fallback search variants for partner names', () => {
		const variants = buildInstitutionSearchVariants(
			'Sciences Po (Institute d Etudes Politiques de Paris)'
		);
		expect(variants[0]).toContain('Sciences Po');
		expect(variants).toContain('Sciences Po');
	});

	it('filters equivalencies by AUC course code/title, host course, or both', () => {
		const rows = [
			normalizeCourseRow(
				{
					SHBTATC_TERM_CODE_EFF_TRNS: '202620',
					SHBTATC_SBGI_CODE: '201842',
					COURSE_GROUP: '(SOFT DSGN Software Design)',
					AUC_EQUIVALENT_COURSE: 'CSCE 4701 OO Analysis & Design, cr.hrs.: 3'
				},
				0
			)!,
			normalizeCourseRow(
				{
					SHBTATC_TERM_CODE_EFF_TRNS: '202510',
					SHBTATC_SBGI_CODE: '201842',
					COURSE_GROUP: '(MATH 103 Mathematics I)',
					AUC_EQUIVALENT_COURSE: 'MACT 1121 Calculus I, cr.hrs.: 3'
				},
				1
			)!
		];

		// Search by AUC course code (with or without space) and AUC course title
		expect(filterEquivalencies(rows, 'CSCE 4701', 'auc').map((r) => r.hostCourseCode)).toEqual([
			'SOFT DSGN'
		]);
		expect(filterEquivalencies(rows, 'csce4701', 'auc').map((r) => r.hostCourseCode)).toEqual([
			'SOFT DSGN'
		]);
		expect(filterEquivalencies(rows, 'Calculus', 'auc').map((r) => r.hostCourseCode)).toEqual([
			'MATH 103'
		]);

		// Host-only search does not match AUC-only title
		expect(filterEquivalencies(rows, 'Calculus', 'host')).toEqual([]);
		expect(filterEquivalencies(rows, 'Software Design', 'host').map((r) => r.hostCourseCode)).toEqual([
			'SOFT DSGN'
		]);

		// Both mode matches either side
		expect(filterEquivalencies(rows, 'Software Design', 'all')).toHaveLength(1);
		expect(filterEquivalencies(rows, 'MACT 1121', 'all')).toHaveLength(1);
	});
});
