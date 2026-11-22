// Demo comparison metrics for the frontend prototype.
//
// The real partner dataset has no seat counts, fit scores, tuition prices or
// machine-readable deadlines. To make Saved/Compare work end to end, this file
// derives one stable set of sample values per university from its slug. Nothing
// here uses Math.random, so the same school always shows the same numbers.
//
// Costs are in USD to match the application's "Estimated budget per semester"
// field. Every screen that shows these values also shows SAMPLE_METRICS_NOTE.

import type { UniversityRecord } from './universities.ts';

/** The subset of the eligibility profile the fit score needs. */
export interface FitProfile {
	level: 'UG' | 'GR' | '';
	gpa: number | null;
	englishComplete: boolean;
	rhet120?: boolean;
}

export interface DemoMetrics {
	/** 35–98. Higher means a better match for this student. */
	fit: number;
	/** Demo answer to "do you meet this host's level, GPA and English rules?". */
	qualification: boolean;
	tuitionUsd: number;
	livingMonthlyUsd: number;
	/** Estimated one-semester cost: half tuition + 5 months living + host fees. */
	semesterCostUsd: number;
	deadline: string;
	seats: number;
	gpaRequired: number;
	ielts: number;
	papers: number;
	english: string;
}

export const SAMPLE_METRICS_NOTE =
	'Costs, seats, deadlines and fit are sample values for this frontend demo. IPSO confirms real figures.';

/** Demo exchange rates: one unit of the currency in USD. */
const USD_RATE: Record<string, number> = {
	USD: 1,
	EUR: 1.08,
	GBP: 1.27,
	CHF: 1.14,
	JPY: 0.0066,
	NOK: 0.093,
	SEK: 0.095,
	CAD: 0.74,
	CNY: 0.14,
	AED: 0.27,
	HKD: 0.128,
	TND: 0.32,
	PLN: 0.25
};

const DAY_MS = 86_400_000;
const DEADLINE_START = Date.UTC(2026, 11, 1); // 1 Dec 2026
const DEADLINE_DAYS = 90; // last possible day is 28 Feb 2027

/** FNV-1a: a short, stable hash so a slug always seeds the same values. */
export function hashSeed(input: string): number {
	let hash = 0x811c9dc5;
	for (let i = 0; i < input.length; i++) {
		hash ^= input.charCodeAt(i);
		hash = Math.imul(hash, 0x01000193) >>> 0;
	}
	return hash >>> 0;
}

/** Small deterministic pseudo-random generator (mulberry32). */
function mulberry32(seed: number): () => number {
	let value = seed >>> 0;
	return () => {
		value = (value + 0x6d2b79f5) >>> 0;
		let t = value;
		t = Math.imul(t ^ (t >>> 15), t | 1);
		t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
		return ((t ^ (t >>> 14)) >>> 0) / 4_294_967_296;
	};
}

function between(rand: () => number, min: number, max: number): number {
	return min + rand() * (max - min);
}

function pick<T>(rand: () => number, options: readonly T[]): T {
	return options[Math.min(options.length - 1, Math.floor(rand() * options.length))];
}

export function usdRate(currency: string | null | undefined): number {
	return USD_RATE[(currency ?? '').toUpperCase()] ?? 1;
}

function demoDeadline(rand: () => number): string {
	const offset = Math.floor(rand() * DEADLINE_DAYS);
	return new Date(DEADLINE_START + offset * DAY_MS).toISOString().slice(0, 10);
}

function seededPapers(record: UniversityRecord, rand: () => number): number {
	const docs = record.required_application_docs.value;
	if (docs && docs.length > 0) return docs.length;
	return 3 + Math.floor(rand() * 3);
}

/**
 * One stable set of sample metrics for a university.
 * `profile` and `budget` only affect `fit` and `qualification`.
 */
export function demoMetrics(
	record: UniversityRecord,
	profile: FitProfile | null = null,
	budget = 0
): DemoMetrics {
	const rand = mulberry32(hashSeed(record.slug));

	const waived = record.tuition_model.value === 'auc_exchange_waiver';
	const tuitionUsd = waived ? 0 : Math.round(between(rand, 6000, 28000) / 100) * 100;

	const living = record.est_monthly_living_local.value;
	const livingMid = living ? (living.min + living.max) / 2 : null;
	const livingMonthlyUsd =
		livingMid && Number.isFinite(livingMid) && livingMid > 0
			? Math.min(3500, Math.max(300, Math.round(livingMid * usdRate(living?.currency))))
			: Math.round(between(rand, 550, 1400));

	const hostFeesUsd = Math.round(between(rand, 250, 900));
	const semesterCostUsd =
		Math.round((tuitionUsd / 2 + livingMonthlyUsd * 5 + hostFeesUsd) / 10) * 10;

	const deadline = demoDeadline(rand);
	const seats = 1 + Math.floor(rand() * 8);
	const gpaRequired = record.min_gpa_host.value ?? pick(rand, [2.5, 2.8, 3.0]);
	const ielts = pick(rand, [6, 6.5, 7, 7.5]);
	const papers = seededPapers(record, rand);

	const levels = record.agreement_student_levels.value ?? [];
	const levelOk = profile?.level
		? levels.length === 0 || levels.includes(profile.level)
		: false;

	// English is met by the graduate flag or by finishing RHET 1020.
	const englishOk = Boolean(profile && (profile.englishComplete || profile.rhet120));
	const gpaOk = profile?.gpa != null && profile.gpa >= gpaRequired;

	let fit = 45;
	if (!profile?.level || levels.length === 0 || levels.includes(profile.level)) fit += 15;
	if (profile?.gpa != null) {
		fit += Math.min(20, Math.max(0, Math.round((profile.gpa - gpaRequired) * 33)));
	}
	if (englishOk) fit += 10;
	if (record.agreement_status.value === 'active') fit += 5;
	fit += Math.floor(rand() * 9); // 0–8 seeded variation
	if (budget > 0 && semesterCostUsd <= budget) fit += 10;
	fit = Math.max(35, Math.min(98, fit));

	return {
		fit,
		qualification: levelOk && gpaOk && englishOk,
		tuitionUsd,
		livingMonthlyUsd,
		semesterCostUsd,
		deadline,
		seats,
		gpaRequired,
		ielts,
		papers,
		english: record.english_proficiency_and_rhet1020.value ?? 'Not specified'
	};
}

export function formatUsd(amount: number): string {
	return `$${Math.round(amount).toLocaleString('en-US')}`;
}

export function formatDeadline(iso: string): string {
	return new Date(`${iso}T00:00:00Z`).toLocaleDateString('en-GB', {
		day: 'numeric',
		month: 'short',
		year: 'numeric',
		timeZone: 'UTC'
	});
}

/** Whole days from now until the demo deadline. Negative when it has passed. */
export function daysLeft(iso: string, now = Date.now()): number {
	const end = new Date(`${iso}T23:59:59Z`).getTime();
	return Math.ceil((end - now) / DAY_MS);
}
