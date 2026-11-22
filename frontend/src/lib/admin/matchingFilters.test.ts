import { describe, expect, it } from 'vitest';
import {
	filterAndSortPlacements,
	filterAndSortUniversityFills,
	filterAndSortUnplaced,
	filterShortlists,
	formatShortSeatLabel,
	groupPlacementsByPartner
} from './matchingFilters.ts';
import type {
	AdminPartnerSummary,
	AdminPlacementRow,
	AdminShortlistRow,
	AdminUniversityFillRow,
	AdminUnplacedRow
} from './types.ts';

const SAMPLE_PLACEMENTS: AdminPlacementRow[] = [
	{
		studentEmail: 'maya@aucegypt.edu',
		studentName: 'Maya El-Shazly',
		studentAucId: '900221487',
		studentMajor: 'Political Science',
		studentGpa: 3.89,
		universitySlug: 'sciences-po',
		universityName: 'Sciences Po Paris',
		seatLabel: 'S1',
		prefRank: 1,
		rankLabel: '#1 Choice',
		weight: 0.942
	},
	{
		studentEmail: 'salma@aucegypt.edu',
		studentName: 'Salma Abdelrahman',
		studentAucId: '900224510',
		studentMajor: 'Economics',
		studentGpa: 3.68,
		universitySlug: 'sciences-po',
		universityName: 'Sciences Po Paris',
		seatLabel: 'S2',
		prefRank: 2,
		rankLabel: '#2 Choice',
		weight: 0.854
	},
	{
		studentEmail: 'omar@aucegypt.edu',
		studentName: 'Omar Mansour',
		studentAucId: '900218932',
		studentMajor: 'Computer Science',
		studentGpa: 3.94,
		universitySlug: 'uc-berkeley',
		universityName: 'UC Berkeley',
		seatLabel: 'S1',
		prefRank: 1,
		rankLabel: '#1 Choice',
		weight: 0.961
	},
	{
		studentEmail: 'ali@aucegypt.edu',
		studentName: 'Ali Gaber',
		studentAucId: '900228411',
		studentMajor: 'Computer Science',
		studentGpa: 3.18,
		universitySlug: 'waseda',
		universityName: 'Waseda University',
		seatLabel: 'S1',
		prefRank: 0,
		rankLabel: 'Unranked Eligible',
		weight: 0.634
	}
];

const SAMPLE_FILLS: AdminUniversityFillRow[] = [
	{
		slug: 'sciences-po',
		name: 'Sciences Po Paris',
		country: 'France',
		region: 'Europe',
		totalSeats: 3,
		seatsAssumed: false,
		filledSeats: 2,
		leftSeats: 1,
		demandCount: 19,
		demandRatio: 6.33,
		cutoffScore: 0.854
	},
	{
		slug: 'uc-berkeley',
		name: 'UC Berkeley',
		country: 'United States',
		region: 'North America',
		totalSeats: 1,
		seatsAssumed: false,
		filledSeats: 1,
		leftSeats: 0,
		demandCount: 25,
		demandRatio: 25.0,
		cutoffScore: 0.961
	},
	{
		slug: 'waseda',
		name: 'Waseda University',
		country: 'Japan',
		region: 'Asia',
		totalSeats: 2,
		seatsAssumed: true,
		filledSeats: 1,
		leftSeats: 1,
		demandCount: 3,
		demandRatio: 1.5,
		cutoffScore: 0.634
	}
];

const SAMPLE_PARTNERS: AdminPartnerSummary[] = [];

describe('matchingFilters', () => {
	it('filters placements by rank, major, minGpa, and search query', () => {
		const res = filterAndSortPlacements(SAMPLE_PLACEMENTS, {
			search: 'sciences',
			prefRank: '1',
			partnerSlug: 'all',
			major: 'all',
			minGpa: 3.5,
			sort: 'score_desc'
		});
		expect(res).toHaveLength(1);
		expect(res[0].studentEmail).toBe('maya@aucegypt.edu');
	});

	it('sorts placements by preference rank ascending with unranked last', () => {
		const res = filterAndSortPlacements(SAMPLE_PLACEMENTS, {
			search: '',
			prefRank: 'all',
			partnerSlug: 'all',
			major: 'all',
			minGpa: 0,
			sort: 'rank_asc'
		});
		expect(res.map((r) => r.prefRank)).toEqual([1, 1, 2, 0]);
	});

	it('groups filtered placements by partner while keeping true totalPlacedAtPartner & seat counts', () => {
		const filtered = filterAndSortPlacements(SAMPLE_PLACEMENTS, {
			search: '',
			prefRank: '2',
			partnerSlug: 'all',
			major: 'all',
			minGpa: 0,
			sort: 'score_desc'
		});
		const groups = groupPlacementsByPartner(
			filtered,
			SAMPLE_PLACEMENTS,
			SAMPLE_FILLS,
			SAMPLE_PARTNERS,
			'score_desc'
		);
		expect(groups).toHaveLength(1);
		expect(groups[0].slug).toBe('sciences-po');
		expect(groups[0].placements).toHaveLength(1);
		expect(groups[0].totalPlacedAtPartner).toBe(2);
		expect(groups[0].totalSeats).toBe(3);
		expect(groups[0].openSeats).toBe(1);
		expect(groups[0].cutoffScore).toBe(0.854);
	});

	it('filters unplaced rows by bottleneck category and query', () => {
		const unplaced: AdminUnplacedRow[] = [
			{
				studentEmail: 'rania@aucegypt.edu',
				studentName: 'Rania Fathy',
				studentAucId: '900111',
				studentMajor: 'Economics',
				gpa: 3.58,
				prefsLabel: 'Bocconi · Sciences Po',
				bottleneck: 'Capacity Contention',
				reason: 'All chosen seats allocated to higher scores.'
			},
			{
				studentEmail: 'nadia@aucegypt.edu',
				studentName: 'Nadia Khalil',
				studentAucId: '900222',
				studentMajor: 'Computer Science',
				gpa: 2.74,
				prefsLabel: 'UC Berkeley',
				bottleneck: 'Eligibility / Policy',
				reason: 'GPA below 2.80.'
			}
		];

		const res = filterAndSortUnplaced(unplaced, {
			search: 'berkeley',
			bottleneck: 'Eligibility / Policy',
			major: 'all',
			sort: 'gpa_desc'
		});
		expect(res).toHaveLength(1);
		expect(res[0].studentName).toBe('Nadia Khalil');
	});

	it('filters and sorts university fill rows by capacity status', () => {
		const fullOnly = filterAndSortUniversityFills(SAMPLE_FILLS, {
			search: '',
			region: 'all',
			status: 'full',
			sort: 'demand_desc'
		});
		expect(fullOnly.map((u) => u.slug)).toEqual(['uc-berkeley']);

		const openSorted = filterAndSortUniversityFills(SAMPLE_FILLS, {
			search: '',
			region: 'all',
			status: 'open',
			sort: 'cutoff_desc'
		});
		expect(openSorted.map((u) => u.slug)).toEqual(['sciences-po', 'waseda']);
	});

	it('filters shortlists by partner match, student match, quota tier, and hideEmpty', () => {
		const shortlists: AdminShortlistRow[] = [
			{
				slug: 'sciences-po',
				name: 'Sciences Po Paris',
				country: 'France',
				totalSeats: 2,
				seatsAssumed: false,
				eligibleCount: 3,
				candidates: [
					{
						rank: 1,
						studentEmail: 'maya@aucegypt.edu',
						studentName: 'Maya El-Shazly',
						studentAucId: '9001',
						studentMajor: 'Political Science',
						studentGpa: 3.89,
						score: 0.94,
						minGpaUnknown: false,
						statementMissing: false
					},
					{
						rank: 3,
						studentEmail: 'karim@aucegypt.edu',
						studentName: 'Karim Tawfik',
						studentAucId: '9002',
						studentMajor: 'Finance',
						studentGpa: 3.41,
						score: 0.75,
						minGpaUnknown: false,
						statementMissing: true
					}
				]
			},
			{
				slug: 'uc-berkeley',
				name: 'UC Berkeley',
				country: 'United States',
				totalSeats: 2,
				seatsAssumed: false,
				eligibleCount: 1,
				candidates: [
					{
						rank: 1,
						studentEmail: 'omar@aucegypt.edu',
						studentName: 'Omar Mansour',
						studentAucId: '9003',
						studentMajor: 'Computer Science',
						studentGpa: 3.94,
						score: 0.96,
						minGpaUnknown: false,
						statementMissing: false
					}
				]
			}
		];

		// Searching by partner retains partner's candidates respecting quota filter
		const partnerSearch = filterShortlists(shortlists, {
			search: 'france',
			partnerSlug: 'all',
			major: 'all',
			quota: 'waitlist',
			minGpa: 0,
			hideEmpty: true
		});
		expect(partnerSearch).toHaveLength(1);
		expect(partnerSearch[0].slug).toBe('sciences-po');
		expect(partnerSearch[0].candidates.map((c) => c.studentName)).toEqual(['Karim Tawfik']);

		// Searching by student filters down to partners containing that student
		const studentSearch = filterShortlists(shortlists, {
			search: 'omar',
			partnerSlug: 'all',
			major: 'all',
			quota: 'all',
			minGpa: 0,
			hideEmpty: true
		});
		expect(studentSearch).toHaveLength(1);
		expect(studentSearch[0].slug).toBe('uc-berkeley');
		expect(studentSearch[0].candidates).toHaveLength(1);
	});

	it('shortens long slug#slot seat labels cleanly for compact badges', () => {
		expect(formatShortSeatLabel('aarhus-university#2')).toBe('#2');
		expect(formatShortSeatLabel('sciences-po-paris#12')).toBe('#12');
		expect(formatShortSeatLabel('S1')).toBe('S1');
		expect(formatShortSeatLabel('')).toBe('#1');
		expect(formatShortSeatLabel('   ')).toBe('#1');
	});
});
