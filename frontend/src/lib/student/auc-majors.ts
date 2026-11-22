// Official AUC majors list used by /signup, /profile, and per-university eligibility.
//
// Edit `auc-majors.json` in this folder to update or replace the catalog.
// Each entry has:
//   - name:    label shown in the dropdown and saved to /api/me
//   - school:  AUC school heading for <optgroup> grouping
//   - levels:  ['UG'], ['GR'], or ['UG', 'GR']
//   - domains: discipline tags matched against partner universities' eligible_majors

import rawMajors from './auc-majors.json';
import type { Level } from './types.ts';

export interface AucMajor {
	name: string;
	school: string;
	levels: Level[];
	domains: string[];
}

export interface MajorGroup {
	school: string;
	majors: AucMajor[];
}

export const AUC_MAJORS: readonly AucMajor[] = rawMajors as AucMajor[];

const BY_NAME = new Map<string, AucMajor>(
	AUC_MAJORS.map((item) => [item.name.trim().toLowerCase(), item])
);

/** Find an AUC major entry by its saved name (case-insensitive). */
export function findAucMajor(name: string | null | undefined): AucMajor | null {
	const key = (name ?? '').trim().toLowerCase();
	if (!key) return null;
	return BY_NAME.get(key) ?? null;
}

/** Majors offered for the chosen level ('UG' or 'GR'), or all majors when level is blank. */
export function majorsForLevel(level: Level | ''): AucMajor[] {
	if (level !== 'UG' && level !== 'GR') return [...AUC_MAJORS];
	return AUC_MAJORS.filter((m) => m.levels.includes(level));
}

/** Majors for the chosen level grouped by AUC school for <optgroup> rendering. */
export function groupedMajorsForLevel(level: Level | ''): MajorGroup[] {
	const groups = new Map<string, AucMajor[]>();
	for (const major of majorsForLevel(level)) {
		const list = groups.get(major.school);
		if (list) list.push(major);
		else groups.set(major.school, [major]);
	}
	return Array.from(groups.entries()).map(([school, majors]) => ({ school, majors }));
}

/** True if `name` is an official AUC major (and offered at `level`, when provided). */
export function isValidAucMajor(name: string | null | undefined, level?: Level | ''): boolean {
	const found = findAucMajor(name);
	if (!found) return false;
	if (level === 'UG' || level === 'GR') return found.levels.includes(level);
	return true;
}
