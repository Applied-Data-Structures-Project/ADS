import { auth } from '#lib/auth/store.svelte.ts';
import type {
	AdminApplicationDetail,
	AdminApplicationListResponse,
	AdminOverviewStats,
	AdminPartnerDetail,
	AdminPartnerListResponse,
	AdminPartnerSummary,
	AdminRunDiff,
	AdminSavedRunDetail,
	AdminSavedRunSummary,
	AdminSimulationResult,
	AuditStatus,
	WeightConfig
} from './types.ts';

export class AdminApiError extends Error {
	constructor(
		public status: number,
		message: string
	) {
		super(message);
	}
}

async function adminRequest<T>(method: string, path: string, body?: unknown): Promise<T> {
	const headers: Record<string, string> = {};
	if (auth.email) headers['X-User-Email'] = auth.email;
	if (body !== undefined) headers['Content-Type'] = 'application/json';

	let res: Response;
	try {
		res = await fetch(path, {
			method,
			headers,
			body: body === undefined ? undefined : JSON.stringify(body)
		});
	} catch {
		throw new AdminApiError(0, 'Cannot reach the C++ backend. Is ./build/ads_server running?');
	}

	const text = await res.text();
	let data: unknown = null;
	try {
		data = text ? JSON.parse(text) : null;
	} catch {
		data = null;
	}
	if (!res.ok) {
		const message = (data as { error?: string } | null)?.error;
		throw new AdminApiError(res.status, message ?? `Server error (${res.status})`);
	}
	return data as T;
}

export const adminApi = {
	getOverview(cycle: string): Promise<AdminOverviewStats> {
		return adminRequest('GET', `/api/admin/overview?cycle=${encodeURIComponent(cycle)}`);
	},

	getApplications(params?: {
		cohort?: string;
		search?: string;
		standing?: string;
		level?: string;
		gpaMin?: number;
		gpaMax?: number;
		uni?: string;
		prior?: string;
		major?: string;
		status?: string;
	}): Promise<AdminApplicationListResponse> {
		const q = new URLSearchParams();
		if (params) {
			for (const [k, v] of Object.entries(params)) {
				if (v !== undefined && v !== '') q.set(k, String(v));
			}
		}
		const qs = q.toString();
		return adminRequest('GET', `/api/admin/applications${qs ? `?${qs}` : ''}`);
	},

	getApplicationDetail(email: string): Promise<AdminApplicationDetail> {
		return adminRequest('GET', `/api/admin/applications/${encodeURIComponent(email)}`);
	},

	updateReview(
		email: string,
		payload: {
			status?: AuditStatus;
			goodStanding?: boolean | null;
			statementRating?: number | null;
			notes?: string;
		}
	): Promise<AdminApplicationDetail> {
		return adminRequest('PUT', `/api/admin/applications/${encodeURIComponent(email)}/review`, payload);
	},

	getUniversities(params?: {
		search?: string;
		region?: string;
		status?: string;
		term?: string;
		level?: string;
		quota?: string;
	}): Promise<AdminPartnerListResponse> {
		const q = new URLSearchParams();
		if (params) {
			for (const [k, v] of Object.entries(params)) {
				if (v !== undefined && v !== '') q.set(k, String(v));
			}
		}
		const qs = q.toString();
		return adminRequest('GET', `/api/admin/universities${qs ? `?${qs}` : ''}`);
	},

	getUniversityDetail(slug: string): Promise<AdminPartnerDetail> {
		return adminRequest('GET', `/api/admin/universities/${encodeURIComponent(slug)}`);
	},

	setUniversitySeats(slug: string, seats: number): Promise<AdminPartnerSummary> {
		return adminRequest('PUT', `/api/admin/universities/${encodeURIComponent(slug)}/seats`, {
			seats
		});
	},

	simulate(payload: {
		cycle: string;
		weights: WeightConfig;
		shortlistLimit?: number;
		compareRunId?: number;
	}): Promise<AdminSimulationResult> {
		return adminRequest('POST', '/api/admin/matching/simulate', {
			cycle: payload.cycle,
			mode: payload.weights.mode,
			weights: payload.weights,
			shortlistLimit: payload.shortlistLimit ?? 10,
			compareRunId: payload.compareRunId ?? 0
		});
	},

	getRuns(): Promise<{ runs: AdminSavedRunSummary[] }> {
		return adminRequest('GET', '/api/admin/runs');
	},

	getRunDetail(runId: number): Promise<AdminSavedRunDetail> {
		return adminRequest('GET', `/api/admin/runs/${runId}`);
	},

	saveRun(payload: {
		title: string;
		cycle: string;
		weights: WeightConfig;
		createdBy?: string;
	}): Promise<AdminSavedRunDetail> {
		return adminRequest('POST', '/api/admin/runs', {
			title: payload.title,
			cycle: payload.cycle,
			mode: payload.weights.mode,
			weights: payload.weights,
			createdBy: payload.createdBy ?? auth.displayName
		});
	},

	publishRun(runId: number): Promise<AdminSavedRunSummary> {
		return adminRequest('POST', `/api/admin/runs/${runId}/publish`, {});
	},

	deleteRun(runId: number): Promise<{ deleted: number }> {
		return adminRequest('DELETE', `/api/admin/runs/${runId}`);
	},

	diffRuns(runA: number, runB: number): Promise<AdminRunDiff> {
		return adminRequest('GET', `/api/admin/runs/diff?a=${runA}&b=${runB}`);
	},

	seedDemoCohort(): Promise<{ seeded: number }> {
		return adminRequest('POST', '/api/admin/demo/seed', {});
	},

	clearDemoCohort(): Promise<{ removed: number }> {
		return adminRequest('DELETE', '/api/admin/demo/seed');
	}
};
