import type {
	AdminPartnerSummary,
	AdminPlacementRow,
	AdminShortlistCandidateRow,
	AdminShortlistRow,
	AdminUniversityFillRow,
	AdminUnplacedRow
} from './types.ts';

export type PlacementSortKey = 'score_desc' | 'gpa_desc' | 'rank_asc' | 'uni_asc' | 'name_asc';

export interface PlacementFilterState {
	search: string;
	prefRank: 'all' | '1' | '2' | '3' | '0';
	partnerSlug: string;
	major: string;
	minGpa: number;
	sort: PlacementSortKey;
}

export interface GroupedPartnerPlacements {
	slug: string;
	name: string;
	country: string;
	region: string;
	totalSeats: number;
	seatsAssumed: boolean;
	totalPlacedAtPartner: number;
	openSeats: number;
	cutoffScore: number | null;
	demandCount: number;
	demandRatio: number;
	placements: AdminPlacementRow[];
}

export interface UnplacedFilterState {
	search: string;
	bottleneck: string;
	major: string;
	sort: 'gpa_desc' | 'name_asc';
}

export type FillStatusFilter = 'all' | 'full' | 'open' | 'high' | 'empty';
export type FillSortKey = 'demand_desc' | 'fill_desc' | 'open_desc' | 'cutoff_desc' | 'name_asc';

export interface FillFilterState {
	search: string;
	region: string;
	status: FillStatusFilter;
	sort: FillSortKey;
}

export type ShortlistQuotaFilter = 'all' | 'in_quota' | 'waitlist';

export interface ShortlistFilterState {
	search: string;
	partnerSlug: string;
	major: string;
	quota: ShortlistQuotaFilter;
	minGpa: number;
	hideEmpty: boolean;
}

function norm(s: string | undefined | null): string {
	return (s ?? '').trim().toLowerCase();
}

export function formatShortSeatLabel(seatLabel: string | undefined | null): string {
	const trimmed = (seatLabel ?? '').trim();
	if (!trimmed) return '#1';
	const hashIdx = trimmed.lastIndexOf('#');
	if (hashIdx >= 0 && hashIdx < trimmed.length - 1) {
		return `#${trimmed.slice(hashIdx + 1)}`;
	}
	if (trimmed.length <= 5) return trimmed;
	return trimmed.slice(0, 5);
}

export function filterAndSortPlacements(
	placements: AdminPlacementRow[],
	f: PlacementFilterState
): AdminPlacementRow[] {
	const q = norm(f.search);
	const rows = placements.filter((p) => {
		if (f.prefRank !== 'all' && String(p.prefRank) !== f.prefRank) return false;
		if (f.partnerSlug !== 'all' && f.partnerSlug && p.universitySlug !== f.partnerSlug) {
			return false;
		}
		if (f.major !== 'all' && f.major && p.studentMajor !== f.major) return false;
		if (f.minGpa > 0 && p.studentGpa < f.minGpa) return false;
		if (q) {
			const hay = `${p.studentName} ${p.studentEmail} ${p.studentAucId} ${p.studentMajor} ${p.universityName} ${p.universitySlug} ${p.seatLabel} ${p.rankLabel}`.toLowerCase();
			if (!hay.includes(q)) return false;
		}
		return true;
	});

	const sorted = [...rows];
	if (f.sort === 'score_desc') {
		sorted.sort((a, b) => b.weight - a.weight || a.studentName.localeCompare(b.studentName));
	} else if (f.sort === 'gpa_desc') {
		sorted.sort((a, b) => b.studentGpa - a.studentGpa || b.weight - a.weight);
	} else if (f.sort === 'rank_asc') {
		sorted.sort((a, b) => {
			const ra = a.prefRank === 0 ? 99 : a.prefRank;
			const rb = b.prefRank === 0 ? 99 : b.prefRank;
			return ra - rb || b.weight - a.weight;
		});
	} else if (f.sort === 'uni_asc') {
		sorted.sort((a, b) => a.universityName.localeCompare(b.universityName) || b.weight - a.weight);
	} else {
		sorted.sort((a, b) => a.studentName.localeCompare(b.studentName));
	}
	return sorted;
}

export function groupPlacementsByPartner(
	filteredPlacements: AdminPlacementRow[],
	allPlacements: AdminPlacementRow[],
	universityFills: AdminUniversityFillRow[],
	partners: AdminPartnerSummary[],
	sort: PlacementSortKey = 'score_desc'
): GroupedPartnerPlacements[] {
	const fillBySlug = new Map<string, AdminUniversityFillRow>();
	for (const u of universityFills) fillBySlug.set(u.slug, u);

	const partnerBySlug = new Map<string, AdminPartnerSummary>();
	for (const p of partners) partnerBySlug.set(p.slug, p);

	// Track true total filled count & cutoff per partner across the entire run
	const allBySlug = new Map<string, AdminPlacementRow[]>();
	for (const row of allPlacements) {
		const list = allBySlug.get(row.universitySlug);
		if (list) list.push(row);
		else allBySlug.set(row.universitySlug, [row]);
	}

	const groupedMap = new Map<string, AdminPlacementRow[]>();
	for (const row of filteredPlacements) {
		const list = groupedMap.get(row.universitySlug);
		if (list) list.push(row);
		else groupedMap.set(row.universitySlug, [row]);
	}

	const groups: GroupedPartnerPlacements[] = [];
	for (const [slug, placements] of groupedMap.entries()) {
		const fill = fillBySlug.get(slug);
		const partner = partnerBySlug.get(slug);
		const allAtPartner = allBySlug.get(slug) ?? placements;

		const totalPlacedAtPartner = allAtPartner.length;
		const totalSeats = fill?.totalSeats ?? partner?.seats ?? totalPlacedAtPartner;
		const openSeats = fill?.leftSeats ?? Math.max(0, totalSeats - totalPlacedAtPartner);

		let cutoffScore: number | null = fill?.cutoffScore ?? null;
		if (cutoffScore === null && allAtPartner.length > 0) {
			cutoffScore = Math.min(...allAtPartner.map((p) => p.weight));
		}

		groups.push({
			slug,
			name: fill?.name ?? partner?.officialName ?? placements[0].universityName,
			country: fill?.country ?? partner?.country ?? '',
			region: fill?.region ?? partner?.region ?? '',
			totalSeats,
			seatsAssumed: fill?.seatsAssumed ?? partner?.seatsAssumed ?? true,
			totalPlacedAtPartner,
			openSeats,
			cutoffScore,
			demandCount: fill?.demandCount ?? partner?.totalDemand ?? totalPlacedAtPartner,
			demandRatio:
				fill?.demandRatio ??
				(totalSeats > 0
					? (partner?.totalDemand ?? totalPlacedAtPartner) / totalSeats
					: (partner?.totalDemand ?? totalPlacedAtPartner)),
			placements
		});
	}

	if (sort === 'uni_asc') {
		groups.sort((a, b) => a.name.localeCompare(b.name));
	} else {
		groups.sort((a, b) => {
			const topA = Math.max(...a.placements.map((p) => p.weight));
			const topB = Math.max(...b.placements.map((p) => p.weight));
			return topB - topA || a.name.localeCompare(b.name);
		});
	}

	return groups;
}

export function filterAndSortUnplaced(
	unplaced: AdminUnplacedRow[],
	f: UnplacedFilterState
): AdminUnplacedRow[] {
	const q = norm(f.search);
	const rows = unplaced.filter((u) => {
		if (f.bottleneck !== 'all' && f.bottleneck && u.bottleneck !== f.bottleneck) return false;
		if (f.major !== 'all' && f.major && u.studentMajor !== f.major) return false;
		if (q) {
			const hay = `${u.studentName} ${u.studentEmail} ${u.studentAucId} ${u.studentMajor} ${u.prefsLabel} ${u.bottleneck} ${u.reason}`.toLowerCase();
			if (!hay.includes(q)) return false;
		}
		return true;
	});

	const sorted = [...rows];
	if (f.sort === 'gpa_desc') {
		sorted.sort((a, b) => b.gpa - a.gpa || a.studentName.localeCompare(b.studentName));
	} else {
		sorted.sort((a, b) => a.studentName.localeCompare(b.studentName));
	}
	return sorted;
}

export function filterAndSortUniversityFills(
	fills: AdminUniversityFillRow[],
	f: FillFilterState
): AdminUniversityFillRow[] {
	const q = norm(f.search);
	const rows = fills.filter((u) => {
		if (f.region !== 'all' && f.region && u.region !== f.region) return false;
		if (f.status === 'full' && (u.totalSeats <= 0 || u.filledSeats < u.totalSeats)) return false;
		if (f.status === 'open' && u.leftSeats <= 0) return false;
		if (f.status === 'high' && u.demandRatio < 3) return false;
		if (f.status === 'empty' && u.filledSeats > 0) return false;
		if (q) {
			const hay = `${u.name} ${u.slug} ${u.country} ${u.region}`.toLowerCase();
			if (!hay.includes(q)) return false;
		}
		return true;
	});

	const sorted = [...rows];
	if (f.sort === 'demand_desc') {
		sorted.sort((a, b) => b.demandRatio - a.demandRatio || b.demandCount - a.demandCount);
	} else if (f.sort === 'fill_desc') {
		sorted.sort((a, b) => {
			const pctA = a.totalSeats > 0 ? a.filledSeats / a.totalSeats : 0;
			const pctB = b.totalSeats > 0 ? b.filledSeats / b.totalSeats : 0;
			return pctB - pctA || b.filledSeats - a.filledSeats;
		});
	} else if (f.sort === 'open_desc') {
		sorted.sort((a, b) => b.leftSeats - a.leftSeats || b.demandCount - a.demandCount);
	} else if (f.sort === 'cutoff_desc') {
		sorted.sort((a, b) => (b.cutoffScore ?? -1) - (a.cutoffScore ?? -1));
	} else {
		sorted.sort((a, b) => a.name.localeCompare(b.name));
	}
	return sorted;
}

function candidateMatchesQuery(c: AdminShortlistCandidateRow, q: string): boolean {
	const hay = `${c.studentName} ${c.studentEmail} ${c.studentAucId} ${c.studentMajor}`.toLowerCase();
	return hay.includes(q);
}

export function filterShortlists(
	shortlists: AdminShortlistRow[],
	f: ShortlistFilterState
): AdminShortlistRow[] {
	const q = norm(f.search);
	const out: AdminShortlistRow[] = [];

	for (const sl of shortlists) {
		if (f.partnerSlug !== 'all' && f.partnerSlug && sl.slug !== f.partnerSlug) continue;

		const partnerMatchesQuery =
			q.length > 0 && `${sl.name} ${sl.country} ${sl.slug}`.toLowerCase().includes(q);

		const candidates = sl.candidates.filter((c) => {
			if (f.quota === 'in_quota' && c.rank > sl.totalSeats) return false;
			if (f.quota === 'waitlist' && c.rank <= sl.totalSeats) return false;
			if (f.major !== 'all' && f.major && c.studentMajor !== f.major) return false;
			if (f.minGpa > 0 && c.studentGpa < f.minGpa) return false;
			if (q && !partnerMatchesQuery && !candidateMatchesQuery(c, q)) return false;
			return true;
		});

		if (f.hideEmpty && candidates.length === 0) continue;

		out.push({
			...sl,
			candidates
		});
	}

	return out;
}
