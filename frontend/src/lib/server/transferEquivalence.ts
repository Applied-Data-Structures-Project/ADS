// Helpers for querying and normalizing AUC Banner's public Transfer Equivalency API.

export const BANNER_TOOL_URL =
	'https://generalssb-prod.ec.aucegypt.edu/BannerExtensibility/customPage/page/TransferEquivalence';

export const BANNER_INSTITUTIONS_URL =
	'https://generalssb-prod.ec.aucegypt.edu/BannerExtensibility/internalPb/virtualDomains.TransferEquivalence_institutions_info';

export const BANNER_CATALOGUE_URL =
	'https://generalssb-prod.ec.aucegypt.edu/BannerExtensibility/internalPb/virtualDomains.TransferEquivalence_inst_catalogue';

export type EquivalencySearchBy = 'auc' | 'host' | 'all';

export interface BannerInstitution {
	code: string;
	name: string;
}

export interface TransferEquivalencyItem {
	id: string;
	instCode: string;
	effectiveTermCode: string;
	effectiveTermLabel: string;
	hostCourseCode: string;
	hostCourseTitle: string;
	rawCourseGroup: string;
	aucEquivalent: string;
	aucDepartments: string[];
	hasEquivalent: boolean;
}

/** Decode common HTML entities and strip Banner inline formatting tags (e.g. <b><u>OR</u></b>). */
export function cleanBannerHtml(raw: string): string {
	if (!raw) return '';
	return raw
		.replace(/<br\s*\/?>/gi, ' / ')
		.replace(/<\/?(?:b|u|i|strong|em|span|p|div)[^>]*>/gi, ' ')
		.replace(/<[^>]+>/g, ' ')
		.replace(/&nbsp;/gi, ' ')
		.replace(/&amp;/gi, '&')
		.replace(/&lt;/gi, '<')
		.replace(/&gt;/gi, '>')
		.replace(/&quot;/gi, '"')
		.replace(/&#39;|&apos;/gi, "'")
		.replace(/\s+/g, ' ')
		.trim();
}

/**
 * Split Banner COURSE_GROUP like "(FA 341 History of the Cinema)" or
 * "(AI MGMT AI-Pow Knowl Mgmt Fut Org)" into code + title.
 */
export function parseHostCourseGroup(raw: string): { code: string; title: string; cleanRaw: string } {
	const clean = cleanBannerHtml(raw);
	if (!clean) return { code: '', title: '', cleanRaw: '' };

	// Single course wrapped in outer parentheses: "(DEPT 123 Course Title)"
	const unwrapped =
		clean.startsWith('(') && clean.endsWith(')') && !clean.slice(1, -1).includes('(')
			? clean.slice(1, -1).trim()
			: clean;

	// Pattern 1: Alphanumeric dept + numeric/alphanumeric course number (e.g. "CSCE 1101 Intro..." or "FA 341 History...")
	const numMatch = unwrapped.match(/^([A-Z]{2,8}[\s-]?[0-9]{2,5}[A-Z0-9.-]*)\s+(.+)$/);
	if (numMatch) {
		return { code: numMatch[1].trim(), title: numMatch[2].trim(), cleanRaw: clean };
	}

	// Pattern 2: Banner short uppercase subject + uppercase subcode (e.g. "ADV PLAN Adv Planning...")
	const upperPair = unwrapped.match(/^([A-Z]{2,6}\s+[A-Z0-9]{2,6})\s+([A-Z][a-z].*)$/);
	if (upperPair) {
		return { code: upperPair[1].trim(), title: upperPair[2].trim(), cleanRaw: clean };
	}

	// Pattern 3: Leading all-caps token followed by title (e.g. "ECON301 Macroeconomics")
	const singleCode = unwrapped.match(/^([A-Z]{2,6}[0-9][A-Z0-9.-]*)\s+(.+)$/);
	if (singleCode) {
		return { code: singleCode[1].trim(), title: singleCode[2].trim(), cleanRaw: clean };
	}

	return { code: '', title: unwrapped, cleanRaw: clean };
}

/** Format Banner term code like "202620" -> "Spring 2026", "202510" -> "Fall 2025". */
export function formatBannerTerm(code: string): string {
	const trimmed = (code || '').trim();
	if (!/^\d{6}$/.test(trimmed)) return trimmed || 'Unspecified';
	const year = trimmed.slice(0, 4);
	const suffix = trimmed.slice(4, 6);
	if (suffix === '10') return `Fall ${year}`;
	if (suffix === '20') return `Spring ${year}`;
	if (suffix === '30') return `Summer ${year}`;
	if (suffix === '40') return `Winter ${year}`;
	return trimmed;
}

/** Extract unique 3-5 letter AUC subject prefixes like CSCE, ECON, MACT from an equivalent string. */
export function extractAucDepartments(aucEquivalent: string): string[] {
	const seen = new Set<string>();
	const matches = aucEquivalent.matchAll(/\b([A-Z]{3,5})\s+(?:[0-9]{3,4}[A-Z]?|[0-9]-{2,4})(?=\s|,|;|$)/g);
	for (const m of matches) {
		seen.add(m[1]);
	}
	return [...seen];
}

export function normalizeInstitutionRow(raw: unknown): BannerInstitution | null {
	if (!raw || typeof raw !== 'object') return null;
	const row = raw as Record<string, unknown>;
	const code = typeof row.SORBTAG_SBGI_CODE === 'string' ? row.SORBTAG_SBGI_CODE.trim() : '';
	const name = typeof row.STVSBGI_DESC === 'string' ? row.STVSBGI_DESC.trim().replace(/\s+/g, ' ') : '';
	if (!code || !name) return null;
	return { code, name };
}

export function normalizeCourseRow(raw: unknown, index: number): TransferEquivalencyItem | null {
	if (!raw || typeof raw !== 'object') return null;
	const row = raw as Record<string, unknown>;
	const instCode = typeof row.SHBTATC_SBGI_CODE === 'string' ? row.SHBTATC_SBGI_CODE.trim() : '';
	const effectiveTermCode =
		typeof row.SHBTATC_TERM_CODE_EFF_TRNS === 'string' ? row.SHBTATC_TERM_CODE_EFF_TRNS.trim() : '';
	const rawGroup = typeof row.COURSE_GROUP === 'string' ? row.COURSE_GROUP : '';
	const rawAuc = typeof row.AUC_EQUIVALENT_COURSE === 'string' ? row.AUC_EQUIVALENT_COURSE : '';

	const parsed = parseHostCourseGroup(rawGroup);
	const aucEquivalent = cleanBannerHtml(rawAuc);
	if (!parsed.cleanRaw && !aucEquivalent) return null;

	const hasEquivalent =
		aucEquivalent.length > 0 && !/^no equivalent course\b/i.test(aucEquivalent);

	return {
		id: `${instCode}-${effectiveTermCode}-${index}-${parsed.cleanRaw.slice(0, 32)}`,
		instCode,
		effectiveTermCode,
		effectiveTermLabel: formatBannerTerm(effectiveTermCode),
		hostCourseCode: parsed.code,
		hostCourseTitle: parsed.title || parsed.cleanRaw,
		rawCourseGroup: parsed.cleanRaw,
		aucEquivalent: aucEquivalent || 'No equivalent course',
		aucDepartments: hasEquivalent ? extractAucDepartments(aucEquivalent) : [],
		hasEquivalent
	};
}

/** Generate fallback search keywords when a full official university name has punctuation/parentheses. */
export function buildInstitutionSearchVariants(name: string): string[] {
	const trimmed = name.trim();
	if (!trimmed) return [];
	const variants: string[] = [trimmed];

	// Strip parentheticals like "Sciences Po (Paris)" -> "Sciences Po"
	const noParens = trimmed.replace(/\([^)]*\)/g, ' ').replace(/\s+/g, ' ').trim();
	if (noParens && !variants.includes(noParens)) variants.push(noParens);

	// Text before comma or hyphen (e.g. "University of California, Berkeley" -> "University of California")
	const beforeSep = noParens.split(/[,–—-]/)[0]?.trim() ?? '';
	if (beforeSep.length >= 4 && !variants.includes(beforeSep)) variants.push(beforeSep);

	// Distinctive non-generic tokens (e.g. "Bocconi University" -> "Bocconi")
	const generic = new Set([
		'university',
		'universite',
		'universidad',
		'universita',
		'universitat',
		'college',
		'institute',
		'school',
		'international',
		'national',
		'state',
		'of',
		'the',
		'in',
		'at',
		'de',
		'di',
		'la',
		'le',
		'and',
		'for'
	]);
	const distinctive = noParens
		.split(/[^A-Za-z0-9]+/)
		.filter((w) => w.length >= 4 && !generic.has(w.toLowerCase()));
	if (distinctive.length > 0) {
		const primary = distinctive[0];
		if (!variants.includes(primary)) variants.push(primary);
	}

	return variants.slice(0, 3);
}

function compactCourseCode(text: string): string {
	return text.toLowerCase().replace(/[\s-]+/g, '');
}

/**
 * Filter normalized Banner equivalencies by AUC course (code/name), host course (code/name),
 * or both. Supports multi-word queries and space-insensitive course codes (e.g. "CSCE4701").
 */
export function filterEquivalencies(
	courses: TransferEquivalencyItem[],
	query: string,
	searchBy: EquivalencySearchBy = 'all'
): TransferEquivalencyItem[] {
	const trimmed = query.trim().toLowerCase();
	if (!trimmed) return courses;

	const tokens = trimmed.split(/\s+/).filter(Boolean);
	const compactNeedle = compactCourseCode(trimmed);

	return courses.filter((item) => {
		const aucHaystack = item.aucEquivalent.toLowerCase();
		const hostHaystack =
			`${item.hostCourseCode} ${item.hostCourseTitle} ${item.rawCourseGroup}`.toLowerCase();

		const target =
			searchBy === 'auc'
				? aucHaystack
				: searchBy === 'host'
					? hostHaystack
					: `${aucHaystack} ${hostHaystack}`;

		if (tokens.every((tok) => target.includes(tok))) return true;
		if (compactNeedle.length >= 4 && compactCourseCode(target).includes(compactNeedle)) {
			return true;
		}
		return false;
	});
}
