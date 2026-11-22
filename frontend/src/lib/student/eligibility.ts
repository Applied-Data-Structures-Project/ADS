// Eligibility pre-check for the study-abroad application.
//
// The student enters their academic record once. These functions turn it into
// the pass/fail list the profile page and the application's first step show.
// The rules come from IPSO's undergraduate and graduate eligibility sheets.
//
// Nothing here talks to the server: the profile is self-reported for now and is
// kept in localStorage per signed-in account until the backend has a route.

import type { EligibilityCheck, Level } from './types.ts';
import { isValidAucMajor } from './auc-majors.ts';

export interface EligibilityProfile {
	level: Level | '';
	major: string;
	gpa: number | null;
	credits: number | null;
	majorDeclared: boolean;
	rhet110: boolean;
	rhet120: boolean;
	finalSemester: boolean;
	academicProbation: boolean;
	disciplinaryProbation: boolean;
	englishComplete: boolean;
	firstSemesterNoGpa: boolean;
}

export type EligibilityErrors = Partial<Record<keyof EligibilityProfile, string>>;

export interface EligibilitySummary {
	checks: EligibilityCheck[];
	eligible: boolean;
	provisional: boolean;
}

export const MIN_GPA_UG = 2.8;
export const MIN_GPA_GR = 3.2;
export const MIN_CREDITS_UG = 24;

/** Structural validation: can this profile be saved? It does not judge eligibility. */
export function validateEligibilityProfile(profile: EligibilityProfile): EligibilityErrors {
	const errors: EligibilityErrors = {};

	if (profile.level !== 'UG' && profile.level !== 'GR') {
		errors.level = 'Choose undergraduate or graduate.';
	}
	if (!isValidAucMajor(profile.major, profile.level)) {
		errors.major = 'Select your AUC major from the list.';
	}
	if (
		profile.credits === null ||
		!Number.isInteger(profile.credits) ||
		profile.credits < 0 ||
		profile.credits > 300
	) {
		errors.credits = 'Enter completed credits as a whole number (0–300).';
	}

	const needsGpa = !(profile.level === 'GR' && profile.firstSemesterNoGpa);
	if (
		needsGpa &&
		(profile.gpa === null || !Number.isFinite(profile.gpa) || profile.gpa < 0 || profile.gpa > 4)
	) {
		errors.gpa = 'Enter a GPA between 0.00 and 4.00.';
	}

	return errors;
}

export function isEligibilityComplete(profile: EligibilityProfile | null | undefined): boolean {
	return !!profile && Object.keys(validateEligibilityProfile(profile)).length === 0;
}

/** Turn a profile into the rules IPSO checks, each with a reason. */
export function evaluateEligibility(profile: EligibilityProfile): EligibilitySummary {
	const isUndergraduate = profile.level !== 'GR';
	const minGpa = isUndergraduate ? MIN_GPA_UG : MIN_GPA_GR;
	const provisionalGpa =
		!isUndergraduate && profile.firstSemesterNoGpa && profile.gpa === null;
	const gpaOk =
		provisionalGpa || (profile.gpa !== null && Number.isFinite(profile.gpa) && profile.gpa >= minGpa);

	const checks: EligibilityCheck[] = [
		{
			label: `GPA of ${minGpa.toFixed(1)} or higher`,
			ok: gpaOk,
			detail: provisionalGpa
				? 'First semester: considered provisionally until your grades are final.'
				: profile.gpa === null
					? 'Enter your GPA to check this rule.'
					: `Your GPA is ${profile.gpa.toFixed(2)}.`,
			provisional: provisionalGpa
		},
		{
			label: 'Good academic standing',
			ok: !profile.academicProbation,
			detail: profile.academicProbation
				? 'Students on academic probation are not eligible.'
				: 'You are not on academic probation.'
		},
		{
			label: 'No disciplinary probation',
			ok: !profile.disciplinaryProbation,
			detail: profile.disciplinaryProbation
				? 'Students on disciplinary probation are not eligible.'
				: 'You are not on disciplinary probation.'
		}
	];

	if (isUndergraduate) {
		checks.push(
			{
				label: `At least ${MIN_CREDITS_UG} completed credits`,
				ok: profile.credits !== null && profile.credits >= MIN_CREDITS_UG,
				detail:
					profile.credits === null
						? 'Enter your completed credits.'
						: `You have ${profile.credits} completed credits.`
			},
			{
				label: 'Officially declared major',
				ok: profile.majorDeclared,
				detail: profile.majorDeclared
					? `Your major is ${profile.major.trim() || 'declared'}.`
					: 'Only students with an officially declared major can apply.'
			},
			{
				label: 'RHET 110/1010 + CORE 110/1010',
				ok: profile.rhet110,
				detail: profile.rhet110
					? 'The freshman writing and core requirement is complete.'
					: 'Complete RHET 110/1010 and CORE 110/1010 first.'
			},
			{
				label: 'RHET 120/1020',
				ok: profile.rhet120,
				detail: profile.rhet120
					? 'The research writing requirement is complete.'
					: 'Complete RHET 120/1020 first.'
			},
			{
				label: 'Not your final semester at AUC',
				ok: !profile.finalSemester,
				detail: profile.finalSemester
					? 'Students in their last semester at AUC are not eligible.'
					: 'This is not your final semester.'
			}
		);
	} else {
		checks.push({
			label: 'Required English courses complete',
			ok: profile.englishComplete,
			detail: profile.englishComplete
				? 'All required English courses are complete.'
				: 'Complete all required English courses before applying.'
		});
	}

	const eligible = checks.every((check) => check.ok);
	return { checks, eligible, provisional: eligible && provisionalGpa };
}

// ---- Local persistence, keyed by the signed-in account ----------------------

export function eligibilityKey(accountId: string): string {
	return `auc-eligibility:${accountId.trim().toLowerCase()}`;
}

function normalize(raw: unknown): EligibilityProfile {
	const data = (raw && typeof raw === 'object' ? raw : {}) as Record<string, unknown>;
	const numberOrNull = (value: unknown): number | null =>
		typeof value === 'number' && Number.isFinite(value) ? value : null;

	return {
		level: data.level === 'UG' || data.level === 'GR' ? data.level : '',
		major: typeof data.major === 'string' ? data.major : '',
		gpa: numberOrNull(data.gpa),
		credits: numberOrNull(data.credits),
		majorDeclared: data.majorDeclared === true,
		rhet110: data.rhet110 === true,
		rhet120: data.rhet120 === true,
		finalSemester: data.finalSemester === true,
		academicProbation: data.academicProbation === true,
		disciplinaryProbation: data.disciplinaryProbation === true,
		englishComplete: data.englishComplete === true,
		firstSemesterNoGpa: data.firstSemesterNoGpa === true
	};
}

/** Read a saved profile, or null when there is no complete one for this account. */
export function readEligibility(accountId: string): EligibilityProfile | null {
	if (typeof localStorage === 'undefined' || accountId.trim() === '') return null;
	try {
		const raw = localStorage.getItem(eligibilityKey(accountId));
		if (!raw) return null;
		const profile = normalize(JSON.parse(raw));
		return isEligibilityComplete(profile) ? profile : null;
	} catch {
		return null;
	}
}

export function writeEligibility(accountId: string, profile: EligibilityProfile): void {
	if (typeof localStorage === 'undefined' || accountId.trim() === '') return;
	try {
		localStorage.setItem(eligibilityKey(accountId), JSON.stringify(profile));
	} catch {
		// Storage blocked: the profile lasts for this visit only.
	}
}

export function removeEligibility(accountId: string): void {
	if (typeof localStorage === 'undefined' || accountId.trim() === '') return;
	try {
		localStorage.removeItem(eligibilityKey(accountId));
	} catch {
		// Storage blocked: nothing to remove.
	}
}
