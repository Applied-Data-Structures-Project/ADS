// First-login profile completion.
//
// Microsoft tells us the person's name and email, but not their academic
// details, so students fill those in once after their first sign-in. Until the
// C++ backend has PUT /api/me, the profile is kept in localStorage keyed by the
// Microsoft account id, so two accounts on one browser do not collide.

import type { Level } from '#lib/student/types.ts';
import { isValidAucMajor } from '#lib/student/auc-majors.ts';

export interface StudentProfileInput {
	name?: string;
	level: Level | '';
	major: string;
	aucId: string;
	phone: string;
}

export type ProfileErrors = Partial<Record<keyof StudentProfileInput, string>>;

export function validateProfile(profile: StudentProfileInput): ProfileErrors {
	const errors: ProfileErrors = {};
	if (profile.level !== 'UG' && profile.level !== 'GR') errors.level = 'Choose your level.';
	if (!isValidAucMajor(profile.major, profile.level)) {
		errors.major = 'Select your AUC major from the list.';
	}
	if (!/^\d{9}$/.test(profile.aucId.trim())) errors.aucId = 'Your AUC ID is 9 digits.';
	if (profile.phone.trim().length < 8) errors.phone = 'Enter a phone number with a country code.';
	return errors;
}

export function isProfileComplete(profile: StudentProfileInput | null | undefined): boolean {
	return !!profile && Object.keys(validateProfile(profile)).length === 0;
}

export function profileKey(homeAccountId: string): string {
	return `auc-profile:${homeAccountId}`;
}

/** Read the saved profile for one Microsoft account, if any. */
export function readProfile(homeAccountId: string): StudentProfileInput | null {
	if (typeof localStorage === 'undefined') return null;
	try {
		const raw = localStorage.getItem(profileKey(homeAccountId));
		if (!raw) return null;
		const parsed = JSON.parse(raw) as Partial<StudentProfileInput>;
		const profile: StudentProfileInput = {
			name: typeof parsed.name === 'string' && parsed.name.trim() ? parsed.name.trim() : undefined,
			level: parsed.level === 'UG' || parsed.level === 'GR' ? parsed.level : '',
			major: typeof parsed.major === 'string' ? parsed.major : '',
			aucId: typeof parsed.aucId === 'string' ? parsed.aucId : '',
			phone: typeof parsed.phone === 'string' ? parsed.phone : ''
		};
		return isProfileComplete(profile) ? profile : null;
	} catch {
		return null;
	}
}

export function writeProfile(homeAccountId: string, profile: StudentProfileInput): void {
	if (typeof localStorage === 'undefined') return;
	try {
		localStorage.setItem(profileKey(homeAccountId), JSON.stringify(profile));
	} catch {
		// Storage blocked: the profile lasts for this visit only.
	}
}

export function removeProfile(homeAccountId: string): void {
	if (typeof localStorage === 'undefined') return;
	try {
		localStorage.removeItem(profileKey(homeAccountId));
	} catch {
		// Storage blocked: nothing to remove.
	}
}
