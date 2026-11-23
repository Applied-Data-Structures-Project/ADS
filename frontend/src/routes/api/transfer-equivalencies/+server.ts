// Live proxy to AUC Banner's public Transfer Equivalency Tool (Option B: live + short memory cache).
//
//   GET /api/transfer-equivalencies?mode=institutions&q=<query>&max=25
//   GET /api/transfer-equivalencies?mode=courses&instCode=<code>&q=<query>&searchBy=auc|host|all&offset=0&max=100

import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import { isValidAucEmail } from '#lib/server/coachLogic.ts';
import {
	BANNER_CATALOGUE_URL,
	BANNER_INSTITUTIONS_URL,
	BANNER_TOOL_URL,
	buildInstitutionSearchVariants,
	filterEquivalencies,
	normalizeCourseRow,
	normalizeInstitutionRow,
	type BannerInstitution,
	type EquivalencySearchBy,
	type TransferEquivalencyItem
} from '#lib/server/transferEquivalence.ts';

const CACHE_TTL_MS = 10 * 60 * 1000;
const REQUEST_TIMEOUT_MS = 15_000;
const BANNER_BATCH_SIZE = 500;
const MAX_CATALOG_ROWS = 2500;

interface CacheEntry<T> {
	expiresAt: number;
	value: T;
}

const cache = new Map<string, CacheEntry<unknown>>();

function getCached<T>(key: string): T | null {
	const entry = cache.get(key);
	if (!entry) return null;
	if (Date.now() > entry.expiresAt) {
		cache.delete(key);
		return null;
	}
	return entry.value as T;
}

function setCached<T>(key: string, value: T): void {
	if (cache.size > 250) {
		const firstKey = cache.keys().next().value;
		if (firstKey) cache.delete(firstKey);
	}
	cache.set(key, { expiresAt: Date.now() + CACHE_TTL_MS, value });
}

function jsonError(status: number, error: string) {
	return json({ error, bannerUrl: BANNER_TOOL_URL }, { status });
}

async function fetchBannerJson(url: string): Promise<unknown[]> {
	const res = await fetch(url, {
		method: 'GET',
		headers: {
			Accept: 'application/json',
			'User-Agent': 'AUC-Exchange-Portal/1.0'
		},
		signal: AbortSignal.timeout(REQUEST_TIMEOUT_MS)
	});
	if (!res.ok) {
		throw new Error(`Banner returned HTTP ${res.status}`);
	}
	const data = await res.json();
	if (!Array.isArray(data)) {
		throw new Error('Unexpected response format from AUC Banner');
	}
	return data;
}

async function queryInstitutions(query: string, max: number): Promise<BannerInstitution[]> {
	const trimmed = query.trim();
	const cacheKey = `inst:${trimmed.toLowerCase()}:${max}`;
	const cached = getCached<BannerInstitution[]>(cacheKey);
	if (cached) return cached;

	const params = new URLSearchParams({
		offset: '0',
		max: String(max)
	});
	if (trimmed) params.set('search_inst', trimmed);

	const rawRows = await fetchBannerJson(`${BANNER_INSTITUTIONS_URL}?${params.toString()}`);
	const parsed = rawRows
		.map((r) => normalizeInstitutionRow(r))
		.filter((r): r is BannerInstitution => r !== null);

	setCached(cacheKey, parsed);
	return parsed;
}

/**
 * Load and cache the full equivalency catalog for a Banner institution so students
 * can search across BOTH AUC course codes/titles and host course codes/titles instantly.
 */
async function loadInstitutionCatalog(
	instCode: string
): Promise<{ courses: TransferEquivalencyItem[]; cached: boolean }> {
	const cacheKey = `catalog-all:${instCode}`;
	const cached = getCached<TransferEquivalencyItem[]>(cacheKey);
	if (cached) return { courses: cached, cached: true };

	const all: TransferEquivalencyItem[] = [];
	for (let offset = 0; offset < MAX_CATALOG_ROWS; offset += BANNER_BATCH_SIZE) {
		const params = new URLSearchParams({
			inst_code: instCode,
			offset: String(offset),
			max: String(BANNER_BATCH_SIZE)
		});
		const rawRows = await fetchBannerJson(`${BANNER_CATALOGUE_URL}?${params.toString()}`);
		for (let idx = 0; idx < rawRows.length; idx++) {
			const normalized = normalizeCourseRow(rawRows[idx], offset + idx);
			if (normalized) all.push(normalized);
		}
		if (rawRows.length < BANNER_BATCH_SIZE) break;
	}

	setCached(cacheKey, all);
	return { courses: all, cached: false };
}

export const GET: RequestHandler = async ({ request, url }) => {
	if (!isValidAucEmail(request.headers.get('x-user-email'))) {
		return jsonError(401, 'Sign in with your AUC account.');
	}

	const mode = (url.searchParams.get('mode') || 'institutions').trim();

	if (mode === 'institutions') {
		const q = (url.searchParams.get('q') || '').trim();
		const autoFallback = url.searchParams.get('fallback') === '1';
		const maxRaw = Number(url.searchParams.get('max') || '30');
		const max = Number.isFinite(maxRaw) ? Math.min(Math.max(Math.round(maxRaw), 1), 100) : 30;

		try {
			const variants = autoFallback && q ? buildInstitutionSearchVariants(q) : [q];
			let institutions: BannerInstitution[] = [];
			let matchedQuery = q;

			for (const variant of variants) {
				institutions = await queryInstitutions(variant, max);
				matchedQuery = variant;
				if (institutions.length > 0) break;
			}

			return json({
				source: 'auc-banner-live',
				bannerUrl: BANNER_TOOL_URL,
				query: matchedQuery,
				count: institutions.length,
				institutions
			});
		} catch (err) {
			console.error('AUC Banner institutions lookup failed:', err);
			return jsonError(
				502,
				'Could not reach the AUC Banner Transfer Equivalency service right now. You can retry or open the official Banner tool directly.'
			);
		}
	}

	if (mode === 'courses') {
		const instCode = (url.searchParams.get('instCode') || '').trim();
		if (!instCode || instCode.length > 32) {
			return jsonError(400, 'Select a Banner institution code first.');
		}
		const q = (url.searchParams.get('q') || '').trim();
		const rawSearchBy = (url.searchParams.get('searchBy') || 'all').trim().toLowerCase();
		const searchBy: EquivalencySearchBy =
			rawSearchBy === 'auc' || rawSearchBy === 'host' ? rawSearchBy : 'all';

		const offsetRaw = Number(url.searchParams.get('offset') || '0');
		const maxRaw = Number(url.searchParams.get('max') || '100');
		const offset = Number.isFinite(offsetRaw) ? Math.max(0, Math.round(offsetRaw)) : 0;
		const max = Number.isFinite(maxRaw) ? Math.min(Math.max(Math.round(maxRaw), 1), 250) : 100;

		try {
			const { courses: fullCatalog, cached } = await loadInstitutionCatalog(instCode);

			const deptSet = new Set<string>();
			for (const item of fullCatalog) {
				for (const d of item.aucDepartments) deptSet.add(d);
			}
			const departments = [...deptSet].sort();

			const matched = filterEquivalencies(fullCatalog, q, searchBy);
			const pageSlice = matched.slice(offset, offset + max);

			return json({
				source: 'auc-banner-live',
				cached,
				bannerUrl: BANNER_TOOL_URL,
				instCode,
				searchBy,
				totalCatalogCount: fullCatalog.length,
				totalMatchedCount: matched.length,
				count: pageSlice.length,
				hasMore: offset + pageSlice.length < matched.length,
				departments,
				courses: pageSlice
			});
		} catch (err) {
			console.error('AUC Banner courses lookup failed:', err);
			return jsonError(
				502,
				'Could not load course equivalencies from AUC Banner right now. Please retry or open the official Banner tool directly.'
			);
		}
	}

	return jsonError(400, 'Unknown mode. Expected "institutions" or "courses".');
};
