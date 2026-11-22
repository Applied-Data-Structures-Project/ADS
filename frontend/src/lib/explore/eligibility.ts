// Real per-university eligibility checker.
//
// Combines the student's IPSO eligibility profile (from GET /api/me) with the
// host university's verified requirements (from GET /api/universities):
//   1. General AUC / IPSO program eligibility (UG/GR rules)
//   2. Accepted student levels (UG / GR)
//   3. Verified host minimum GPA (when present in the dataset)
//   4. Agreement major scope (eligible_majors and excluded_majors)

import { evaluateEligibility, type EligibilityProfile } from '../student/eligibility.ts';
import { findAucMajor } from '../student/auc-majors.ts';
import type { UniversityRecord } from './universities.ts';

export type UniversityEligibilityStatus = 'eligible' | 'provisional' | 'ineligible' | 'unknown';

export interface UniversityEligibility {
	status: UniversityEligibilityStatus;
	label: string;
	reason: string;
	reasons: string[];
	hostGpaUnverified: boolean;
}

/** Common AUC major codes & discipline keywords mapped to broader agreement buckets. */
const MAJOR_GROUPS: { bucket: string; patterns: RegExp[] }[] = [
	{
		bucket: 'business',
		patterns: [
			/\bbus\b/i,
			/\bbusiness\b/i,
			/\bmanagement\b/i,
			/\bfinance\b/i,
			/\baccounting\b/i,
			/\bmarketing\b/i,
			/\bentrepreneur/i,
			/\bmba\b/i,
			/\bmgmt\b/i,
			/\bfinc\b/i,
			/\bacct\b/i,
			/\bmktg\b/i
		]
	},
	{
		bucket: 'economics',
		patterns: [/\becon\b/i, /\beconomics?\b/i]
	},
	{
		bucket: 'political science',
		patterns: [/\bpols\b/i, /\bpoli\b/i, /\bpolitical\b/i, /\bpublic policy\b/i, /\binternational relations\b/i, /\bglobal affairs\b/i]
	},
	{
		bucket: 'journalism',
		patterns: [/\bjrnl\b/i, /\bjrmc\b/i, /\bjournalism\b/i, /\bmedia\b/i, /\bcommunication/i, /\bimc\b/i, /\bfilm\b/i]
	},
	{
		bucket: 'humanities',
		patterns: [
			/\bhumanities\b/i,
			/\bhistory\b/i,
			/\bhist\b/i,
			/\bphilosophy\b/i,
			/\bphil\b/i,
			/\benglish\b/i,
			/\bengl\b/i,
			/\bliterature\b/i,
			/\barabic\b/i,
			/\btheatre\b/i,
			/\btheater\b/i,
			/\bmusic\b/i,
			/\bart\b/i,
			/\banthropology\b/i,
			/\bsociology\b/i,
			/\bpsychology\b/i,
			/\bmiddle east\b/i
		]
	},
	{
		bucket: 'law',
		patterns: [/\blaw\b/i, /\blegal\b/i, /\bll\.?m\b/i]
	},
	{
		bucket: 'engineering',
		patterns: [
			/\bengineering\b/i,
			/\bcomputer\b/i,
			/\bcsce\b/i,
			/\bcs\b/i,
			/\bdata science\b/i,
			/\bmechanical\b/i,
			/\belectrical\b/i,
			/\bconstruction\b/i,
			/\bpetroleum\b/i,
			/\barchitecture\b/i,
			/\barch\b/i
		]
	}
];

function cleanText(raw: string): string {
	return raw
		.replace(/\([^)]*\)/g, ' ')
		.replace(/[^a-z0-9\s/-]/gi, ' ')
		.replace(/\s+/g, ' ')
		.trim()
		.toLowerCase();
}

function bucketsFor(text: string): Set<string> {
	const found = new Set<string>();
	for (const group of MAJOR_GROUPS) {
		if (group.patterns.some((pattern) => pattern.test(text))) {
			found.add(group.bucket);
		}
	}
	return found;
}

/** True when a student's major matches one of a host university's listed eligible majors. */
function majorMatchesEligible(studentMajor: string, eligibleList: string[]): boolean {
	const studentClean = cleanText(studentMajor);
	if (!studentClean) return false;

	// Prefer structured AUC major domains from auc-majors.json when available.
	const catalogMajor = findAucMajor(studentMajor);
	const studentTerms = catalogMajor
		? [studentClean, ...catalogMajor.domains.map((d) => cleanText(d))]
		: [studentClean];
	const studentBuckets = bucketsFor(
		catalogMajor ? `${studentMajor} ${catalogMajor.domains.join(' ')}` : studentMajor
	);

	for (const rawEntry of eligibleList) {
		if (rawEntry.trim().toUpperCase() === 'ALL') return true;
		const parts = rawEntry.split(/[/,]/).map((p) => cleanText(p)).filter(Boolean);
		for (const part of parts) {
			if (
				part.length >= 3 &&
				studentTerms.some((term) => term.includes(part) || part.includes(term))
			) {
				return true;
			}
			const entryBuckets = bucketsFor(part);
			for (const b of entryBuckets) {
				if (studentBuckets.has(b)) return true;
			}
		}
	}
	return false;
}

const STOP_WORDS = new Set(['and', 'the', 'for', 'with', 'from', 'into', 'school', 'program', 'programs', 'studies']);

/** Returns the matching excluded entry if the student's major/level is excluded, else null. */
function findExclusion(
	studentMajor: string,
	studentLevel: 'UG' | 'GR' | '',
	excludedList: string[]
): string | null {
	const catalogMajor = findAucMajor(studentMajor);
	const combined = catalogMajor
		? `${cleanText(studentMajor)} ${catalogMajor.domains.map((d) => cleanText(d)).join(' ')}`
		: cleanText(studentMajor);
	const studentTokens = Array.from(
		new Set(combined.split(/\s+/).filter((t) => t.length >= 3 && !STOP_WORDS.has(t)))
	);

	for (const rawEntry of excludedList) {
		const lower = rawEntry.toLowerCase();

		// Handle level-based exclusions like "Graduate-level programs".
		if (/\bgraduate\b/.test(lower) && !/\bexcept\b/.test(lower)) {
			if (studentLevel === 'GR') return rawEntry;
			continue;
		}

		// Ignore generic course-level notes (e.g. "Core/Compulsory degree courses").
		if (/\bcourses?\b/.test(lower) && !/\b(medicine|dentistry|pharmacy|nursing|law|business|engineering)\b/.test(lower)) {
			continue;
		}

		if (studentTokens.length === 0) continue;
		const entryClean = cleanText(rawEntry);
		// Check specific discipline match (strict enough to avoid false positives).
		for (const token of studentTokens) {
			const wordRe = new RegExp(`\\b${token.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\b`, 'i');
			if (wordRe.test(entryClean)) {
				return rawEntry;
			}
		}
	}
	return null;
}

/**
 * Evaluate whether the student qualifies for a specific partner university,
 * using their saved AUC eligibility profile and the university's real record.
 */
export function evaluateUniversityEligibility(
	university: UniversityRecord,
	profile: EligibilityProfile | null | undefined
): UniversityEligibility {
	const hostGpa = university.min_gpa_host.value;
	const hostGpaUnverified = hostGpa === null || hostGpa === undefined;

	if (!profile || (profile.level !== 'UG' && profile.level !== 'GR')) {
		return {
			status: 'unknown',
			label: 'Complete profile',
			reason: 'Fill in your eligibility profile to check this university.',
			reasons: [],
			hostGpaUnverified
		};
	}

	const reasons: string[] = [];

	// 1. General AUC / IPSO program eligibility
	const ipso = evaluateEligibility(profile);
	if (!ipso.eligible) {
		const firstFail = ipso.checks.find((c) => !c.ok);
		reasons.push(
			firstFail
				? `AUC requirement: ${firstFail.label}.`
				: 'Does not meet AUC study-abroad eligibility rules.'
		);
	}

	// 2. Host student level (Ph.D. programs also match host agreements listing "PhD")
	const levels = university.agreement_student_levels.value ?? [];
	const isPhd = /\bph\.?d\.?\b/i.test(profile.major);
	const levelAccepted =
		levels.length === 0 ||
		levels.includes(profile.level) ||
		(isPhd && levels.includes('PhD'));
	if (!levelAccepted) {
		reasons.push(`Accepts ${levels.join(' / ')} only (you are ${profile.level}).`);
	}

	// 3. Verified host minimum GPA
	if (hostGpa !== null && hostGpa !== undefined) {
		const provisionalGr = profile.level === 'GR' && profile.firstSemesterNoGpa && profile.gpa === null;
		if (!provisionalGr) {
			if (profile.gpa === null) {
				reasons.push(`Host requires a ${hostGpa.toFixed(2)} GPA.`);
			} else if (profile.gpa < hostGpa) {
				reasons.push(`Host requires ${hostGpa.toFixed(2)} GPA (yours is ${profile.gpa.toFixed(2)}).`);
			}
		}
	}

	// 4. Restricted eligible majors
	const eligibleMajors = (university.eligible_majors.value ?? []).filter((m) => m.trim() !== '');
	const openToAll =
		eligibleMajors.length === 0 ||
		eligibleMajors.some((m) => m.trim().toUpperCase() === 'ALL');
	if (!openToAll && !majorMatchesEligible(profile.major, eligibleMajors)) {
		reasons.push(`Restricted to ${eligibleMajors.join(', ')}.`);
	}

	// 5. Excluded majors
	const excludedMajors = university.excluded_majors.value ?? [];
	if (excludedMajors.length > 0) {
		const excluded = findExclusion(profile.major, profile.level, excludedMajors);
		if (excluded) {
			reasons.push(`Excluded under host agreement (${excluded}).`);
		}
	}

	if (reasons.length > 0) {
		return {
			status: 'ineligible',
			label: 'Not eligible',
			reason: reasons[0],
			reasons,
			hostGpaUnverified
		};
	}

	if (ipso.provisional) {
		return {
			status: 'provisional',
			label: 'Provisionally eligible',
			reason: 'Provisional until your first-semester graduate GPA is final.',
			reasons: [],
			hostGpaUnverified
		};
	}

	return {
		status: 'eligible',
		label: 'Eligible',
		reason: hostGpaUnverified
			? 'Meets AUC and listed host rules (confirm host GPA with IPSO).'
			: 'Meets AUC and host university requirements.',
		reasons: [],
		hostGpaUnverified
	};
}

/** Tailwind classes matching the student portal theme for each status badge. */
export function eligibilityBadgeClass(status: UniversityEligibilityStatus): string {
	if (status === 'eligible') return 'bg-[#edf8f1] text-[#247047]';
	if (status === 'provisional') return 'bg-[#fff6e3] text-[#986b1a]';
	if (status === 'ineligible') return 'bg-[#fef2f2] text-[#ba1a1a]';
	return 'bg-[#f2f3f7] text-[#515f74]';
}
