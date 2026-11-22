// One shared copy of the 155-school partner dataset.
//
// Explore, the detail page, Saved, Compare and the application preferences all
// read from here, so the 2.4 MB JSON is fetched once per visit instead of once
// per page. Call `catalog.load()` from the student layout on mount.

import { SvelteMap } from 'svelte/reactivity';
import { loadUniversities, type UniversityRecord } from './universities.ts';

class UniversityCatalog {
	records = $state<UniversityRecord[]>([]);
	status = $state<'idle' | 'loading' | 'ready' | 'error'>('idle');
	error = $state('');

	private index = new SvelteMap<string, UniversityRecord>();
	private pending: Promise<void> | null = null;

	/** Fetch once. Safe to call from every page: later calls reuse the result. */
	load(): Promise<void> {
		if (this.status === 'ready') return Promise.resolve();
		if (this.pending) return this.pending;

		this.status = 'loading';
		this.error = '';
		this.pending = loadUniversities()
			.then((records) => {
				this.records = records;
				this.index.clear();
				for (const record of records) this.index.set(record.slug, record);
				this.status = 'ready';
			})
			.catch((error: unknown) => {
				this.error = error instanceof Error ? error.message : 'Unable to load universities';
				this.status = 'error';
			})
			.finally(() => {
				this.pending = null;
			});

		return this.pending;
	}

	bySlug(slug: string): UniversityRecord | undefined {
		if (!slug) return undefined;
		return this.index.get(slug);
	}

	/** Keeps the requested order and silently drops unknown slugs. */
	bySlugs(slugs: readonly string[]): UniversityRecord[] {
		const found: UniversityRecord[] = [];
		for (const slug of slugs) {
			const record = this.index.get(slug);
			if (record) found.push(record);
		}
		return found;
	}
}

export const catalog = new UniversityCatalog();
