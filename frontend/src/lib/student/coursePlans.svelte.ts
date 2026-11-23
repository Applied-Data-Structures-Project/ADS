// Client store for a student's Draft List of Courses (persisted via C++ GET/PUT/DELETE /api/course-plans).

import { callServer, errorText } from './api.ts';

export interface PlannedCourse {
	hostCourseCode: string;
	hostCourseTitle: string;
	aucEquivalent: string;
	effectiveTerm: string;
	source: 'banner' | 'manual';
}

export interface CoursePlan {
	universitySlug: string;
	universityName: string;
	bannerInstCode: string;
	bannerInstName: string;
	studyTerm: string;
	courses: PlannedCourse[];
	updatedAt: string;
}

interface ServerPlansResponse {
	plans: CoursePlan[];
	plan?: CoursePlan;
}

class CoursePlanStore {
	plans = $state<CoursePlan[]>([]);
	loaded = $state(false);
	saving = $state(false);

	bySlug(slug: string): CoursePlan | null {
		return this.plans.find((p) => p.universitySlug === slug) ?? null;
	}

	async loadAll(): Promise<boolean> {
		try {
			const res = await callServer<ServerPlansResponse>('GET', '/api/course-plans');
			if (Array.isArray(res.plans)) {
				this.plans = res.plans;
			}
			this.loaded = true;
			return true;
		} catch {
			this.loaded = true;
			return false;
		}
	}

	async savePlan(plan: Omit<CoursePlan, 'updatedAt'>): Promise<{ ok: boolean; error?: string }> {
		this.saving = true;
		try {
			const res = await callServer<ServerPlansResponse>(
				'PUT',
				`/api/course-plans/${encodeURIComponent(plan.universitySlug)}`,
				{
					universityName: plan.universityName,
					bannerInstCode: plan.bannerInstCode,
					bannerInstName: plan.bannerInstName,
					studyTerm: plan.studyTerm,
					courses: plan.courses
				}
			);
			if (Array.isArray(res.plans)) {
				this.plans = res.plans;
			}
			return { ok: true };
		} catch (e) {
			return { ok: false, error: errorText(e) };
		} finally {
			this.saving = false;
		}
	}

	async deletePlan(slug: string): Promise<{ ok: boolean; error?: string }> {
		try {
			const res = await callServer<ServerPlansResponse>(
				'DELETE',
				`/api/course-plans/${encodeURIComponent(slug)}`
			);
			if (Array.isArray(res.plans)) {
				this.plans = res.plans;
			}
			return { ok: true };
		} catch (e) {
			return { ok: false, error: errorText(e) };
		}
	}
}

export const coursePlans = new CoursePlanStore();
