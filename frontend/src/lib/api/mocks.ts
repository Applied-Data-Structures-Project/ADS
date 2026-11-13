// Mock responses for frontend work before a backend endpoint exists.
// Shapes must match docs/api.md. Swap a mock for `api.get(...)` once the route lands.

import type { HealthResponse } from './client.ts';

export const mockHealth: HealthResponse = {
	status: 'ok',
	db: 'ok',
	sqlite_version: 'mock'
};

export type AuditStatus = 'Verified' | 'In Review' | 'Ineligible';
export type ClassStanding = 'Sophomore' | 'Junior' | 'Senior';
export type AllocationStrategy = 'lex' | 'hungarian' | 'hk';

export interface AdminStudentApplication {
	id: string;
	name: string;
	email: string;
	standing: ClassStanding;
	major: string;
	gpa: number;
	prefs: [string, string, string];
	llm: number;
	prior: boolean;
	status: AuditStatus;
	budget: number;
	statement: string;
	selected?: boolean;
}

export interface PartnerUniversity {
	id: string;
	name: string;
	short: string;
	country: string;
	region: string;
	minGpa: number;
	cost: number;
	majors: string;
	seats: number;
	demand: number;
	fit: number;
}

export interface WeightConfig {
	pref: number;
	gpa: number;
	llm: number;
	equity: number;
	allowUnranked: boolean;
	mode: AllocationStrategy;
}

export interface SavedMatchRun {
	id: string;
	title: string;
	meta: string;
	weightsLabel: string;
	weights: WeightConfig;
	placed: number;
	hkMax: number;
	firstChoicePct: number;
	status: 'Draft' | 'Archived' | 'Published';
}

export interface MatchedAllocationRow {
	studentName: string;
	studentId: string;
	merit: number;
	assignedUni: string;
	slotLabel: string;
	rankLabel: '#1 Choice' | '#2 Choice' | 'Unranked Eligible';
	weight: number;
}

export interface UnplacedStudentRow {
	studentName: string;
	gpa: number;
	prefsLabel: string;
	bottleneck: 'Capacity Contention' | 'No Eligible Seats';
	reason: string;
}

export const DEFAULT_WEIGHT_CONFIG: WeightConfig = {
	pref: 0.65,
	gpa: 0.45,
	llm: 0.35,
	equity: 0.2,
	allowUnranked: true,
	mode: 'lex'
};

export const MOCK_STUDENTS: AdminStudentApplication[] = [
	{
		id: 'AUC-900214',
		name: 'Youssef El-Masry',
		email: 'y.elmasry@aucegypt.edu',
		standing: 'Junior',
		major: 'Computer Science (CSCE)',
		gpa: 3.78,
		prefs: ['Univ. of Amsterdam', 'TU Munich', 'Yonsei Univ.'],
		llm: 8.9,
		prior: false,
		status: 'Verified',
		budget: 6000,
		statement: 'Focus on distributed systems electives and leading ACM student chapter workshops.'
	},
	{
		id: 'AUC-900219',
		name: 'Salma Abdelrahman',
		email: 's.abdelrahman@aucegypt.edu',
		standing: 'Junior',
		major: 'Economics (ECON)',
		gpa: 3.92,
		prefs: ['Sciences Po', 'Bocconi Univ.', 'Univ. of Amsterdam'],
		llm: 9.2,
		prior: false,
		status: 'Verified',
		budget: 7500,
		statement: 'Researching Mediterranean trade policy; bilingual English/French applicant.'
	},
	{
		id: 'AUC-900224',
		name: 'Omar Tarek',
		email: 'o.tarek@aucegypt.edu',
		standing: 'Sophomore',
		major: 'Computer Engineering (CENG)',
		gpa: 3.64,
		prefs: ['ETH Zurich', 'TU Munich', 'UC Berkeley'],
		llm: 8.4,
		prior: true,
		status: 'Verified',
		budget: 8000,
		statement: 'Embedded robotics track; previously attended a summer program in Germany.'
	},
	{
		id: 'AUC-900231',
		name: 'Mariam Nabil',
		email: 'm.nabil@aucegypt.edu',
		standing: 'Junior',
		major: 'Business Admin (FINC)',
		gpa: 3.51,
		prefs: ['Bocconi Univ.', 'Sciences Po', 'Yonsei Univ.'],
		llm: 8.7,
		prior: false,
		status: 'In Review',
		budget: 5500,
		statement: 'Interested in European sustainable finance and venture capital coursework.'
	},
	{
		id: 'AUC-900238',
		name: 'Karim El-Sayed',
		email: 'k.elsayed@aucegypt.edu',
		standing: 'Senior',
		major: 'Mechanical Engineering (MENG)',
		gpa: 2.68,
		prefs: ['TU Munich', 'Yonsei Univ.', 'Univ. of Amsterdam'],
		llm: 7.5,
		prior: false,
		status: 'Ineligible',
		budget: 5000,
		statement: 'GPA 2.68 below AUC 2.80 minimum threshold; flagged for academic advisor review.'
	},
	{
		id: 'AUC-900245',
		name: 'Laila Mansour',
		email: 'l.mansour@aucegypt.edu',
		standing: 'Junior',
		major: 'Architecture (ARCH)',
		gpa: 3.85,
		prefs: ['ETH Zurich', 'Univ. of Amsterdam', 'Bocconi Univ.'],
		llm: 9.0,
		prior: false,
		status: 'Verified',
		budget: 6500,
		statement: 'Urban sustainability studio portfolio aligned with ETH Zurich spatial design.'
	},
	{
		id: 'AUC-900252',
		name: 'Nourhan Sherif',
		email: 'n.sherif@aucegypt.edu',
		standing: 'Junior',
		major: 'Mechanical Engineering (MENG)',
		gpa: 3.44,
		prefs: ['ETH Zurich', 'TU Munich', 'UC Berkeley'],
		llm: 8.1,
		prior: false,
		status: 'Verified',
		budget: 5800,
		statement: 'Renewable energy systems concentration seeking German/Swiss engineering labs.'
	}
];

export const MOCK_UNIVERSITIES: PartnerUniversity[] = [
	{
		id: 'uva',
		name: 'University of Amsterdam',
		short: 'Univ. of Amsterdam',
		country: 'Netherlands',
		region: 'Europe',
		minGpa: 3.2,
		cost: 5200,
		majors: 'CSCE, ECON, BADM',
		seats: 5,
		demand: 19,
		fit: 94
	},
	{
		id: 'tum',
		name: 'TU Munich',
		short: 'TU Munich',
		country: 'Germany',
		region: 'Europe',
		minGpa: 3.3,
		cost: 4600,
		majors: 'CSCE, CENG, MENG',
		seats: 4,
		demand: 16,
		fit: 89
	},
	{
		id: 'eth',
		name: 'ETH Zurich',
		short: 'ETH Zurich',
		country: 'Switzerland',
		region: 'Europe',
		minGpa: 3.6,
		cost: 7200,
		majors: 'CSCE, CENG, ARCH',
		seats: 2,
		demand: 14,
		fit: 82
	},
	{
		id: 'sciences-po',
		name: 'Sciences Po Paris',
		short: 'Sciences Po',
		country: 'France',
		region: 'Europe',
		minGpa: 3.4,
		cost: 5800,
		majors: 'ECON, POLS, BADM',
		seats: 4,
		demand: 15,
		fit: 78
	},
	{
		id: 'bocconi',
		name: 'Bocconi University',
		short: 'Bocconi Univ.',
		country: 'Italy',
		region: 'Europe',
		minGpa: 3.2,
		cost: 5400,
		majors: 'ECON, FINC, BADM',
		seats: 4,
		demand: 11,
		fit: 80
	},
	{
		id: 'yonsei',
		name: 'Yonsei University',
		short: 'Yonsei Univ.',
		country: 'South Korea',
		region: 'East Asia',
		minGpa: 3.0,
		cost: 4900,
		majors: 'All AUC Majors',
		seats: 6,
		demand: 9,
		fit: 85
	},
	{
		id: 'berkeley',
		name: 'UC Berkeley',
		short: 'UC Berkeley',
		country: 'USA',
		region: 'North America',
		minGpa: 3.65,
		cost: 9400,
		majors: 'CSCE, ECON, CENG',
		seats: 1,
		demand: 12,
		fit: 74
	}
];

export const MOCK_SAVED_RUNS: SavedMatchRun[] = [
	{
		id: '#RUN-004',
		title: 'Equity + Preference Priority',
		meta: 'Today, 00:42 · Nadia Hassan',
		weightsLabel: 'λ=0.65 · GPA=0.45 · LLM=0.35 · Eq=0.20',
		weights: { pref: 0.65, gpa: 0.45, llm: 0.35, equity: 0.2, allowUnranked: true, mode: 'lex' },
		placed: 138,
		hkMax: 138,
		firstChoicePct: 68.1,
		status: 'Draft'
	},
	{
		id: '#RUN-003',
		title: 'Pure Academic Merit Baseline',
		meta: 'Yesterday · Dr. Karim Mansour',
		weightsLabel: 'λ=0.45 · GPA=0.80 · LLM=0.20 · Eq=0.00',
		weights: {
			pref: 0.45,
			gpa: 0.8,
			llm: 0.2,
			equity: 0.0,
			allowUnranked: true,
			mode: 'hungarian'
		},
		placed: 134,
		hkMax: 138,
		firstChoicePct: 63.0,
		status: 'Archived'
	},
	{
		id: '#RUN-002',
		title: 'Strict Top-3 Only (ε = 0)',
		meta: '2 days ago · Nadia Hassan',
		weightsLabel: 'λ=0.80 · GPA=0.40 · LLM=0.35 · Eq=0.15',
		weights: { pref: 0.8, gpa: 0.4, llm: 0.35, equity: 0.15, allowUnranked: false, mode: 'lex' },
		placed: 121,
		hkMax: 121,
		firstChoicePct: 71.9,
		status: 'Archived'
	}
];

export const MOCK_MATCHED_ROWS: MatchedAllocationRow[] = [
	{
		studentName: 'Salma Abdelrahman',
		studentId: 'AUC-900219',
		merit: 0.96,
		assignedUni: 'Sciences Po Paris',
		slotLabel: 'Slot #1',
		rankLabel: '#1 Choice',
		weight: 1.094
	},
	{
		studentName: 'Youssef El-Masry',
		studentId: 'AUC-900214',
		merit: 0.91,
		assignedUni: 'University of Amsterdam',
		slotLabel: 'Slot #2',
		rankLabel: '#1 Choice',
		weight: 1.041
	},
	{
		studentName: 'Laila Mansour',
		studentId: 'AUC-900245',
		merit: 0.94,
		assignedUni: 'ETH Zurich',
		slotLabel: 'Slot #1',
		rankLabel: '#1 Choice',
		weight: 1.062
	},
	{
		studentName: 'Omar Tarek',
		studentId: 'AUC-900224',
		merit: 0.84,
		assignedUni: 'TU Munich',
		slotLabel: 'Slot #3',
		rankLabel: '#2 Choice',
		weight: 0.784
	},
	{
		studentName: 'Mariam Nabil',
		studentId: 'AUC-900231',
		merit: 0.87,
		assignedUni: 'Bocconi University',
		slotLabel: 'Slot #1',
		rankLabel: '#1 Choice',
		weight: 0.985
	}
];

export const MOCK_UNPLACED_ROWS: UnplacedStudentRow[] = [
	{
		studentName: 'Nourhan Sherif',
		gpa: 3.44,
		prefsLabel: 'ETH Zurich · TU Munich · UC Berkeley',
		bottleneck: 'Capacity Contention',
		reason: 'All 3 choices filled by higher-weighted applicants; Yonsei open if fallback enabled.'
	},
	{
		studentName: 'Seif El-Din Adel',
		gpa: 3.05,
		prefsLabel: 'UC Berkeley · Univ. of Amsterdam · Sciences Po',
		bottleneck: 'No Eligible Seats',
		reason: 'GPA below partner thresholds on 2 picks; remaining budget-compatible partner full.'
	}
];
