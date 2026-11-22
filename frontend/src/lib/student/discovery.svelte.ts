// Saved schools, the compare tray and notifications for Group 2's pages.
//
// Frontend only for now: state lives in localStorage under the signed-in
// account, and every university is identified by its Explore slug. When the
// backend saved/notifications routes exist, replace the read/write helpers in
// this file and keep the public methods the same.

import { catalog } from '../explore/catalog.svelte.ts';
import type { UniversityRecord } from '../explore/universities.ts';

export interface StudentNotification {
	id: number;
	message: string;
	createdAt: string;
	read: boolean;
}

interface StoredDiscovery {
	saved: string[];
	compare: string[];
	notifications: StudentNotification[];
}

/** Real slugs, one per region, so a fresh demo account has something to show. */
const SEED_SAVED = [
	'bocconi-university',
	'sciences-po-institute-d-etudes-politiques-de-paris',
	'aarhus-university',
	'akita-international-university',
	'auburn-university'
];

const MAX_COMPARE = 3;

function storageKey(accountId: string): string {
	return `auc-discovery-v1:${accountId}`;
}

function isNotification(value: unknown): value is StudentNotification {
	const item = value as Partial<StudentNotification> | null;
	return (
		!!item &&
		typeof item.id === 'number' &&
		typeof item.message === 'string' &&
		typeof item.createdAt === 'string' &&
		typeof item.read === 'boolean'
	);
}

function readStored(accountId: string): StoredDiscovery | null {
	if (typeof localStorage === 'undefined') return null;
	try {
		const raw = localStorage.getItem(storageKey(accountId));
		if (!raw) return null;
		const parsed = JSON.parse(raw) as Partial<StoredDiscovery> | null;
		if (!parsed || !Array.isArray(parsed.saved)) return null;

		const saved = parsed.saved.filter((slug): slug is string => typeof slug === 'string');
		const compare = Array.isArray(parsed.compare)
			? parsed.compare
					.filter((slug): slug is string => typeof slug === 'string')
					.slice(0, MAX_COMPARE)
			: [];
		const notifications = Array.isArray(parsed.notifications)
			? parsed.notifications.filter(isNotification)
			: [];

		return { saved, compare, notifications };
	} catch {
		return null;
	}
}

function seedNotifications(): StudentNotification[] {
	const now = Date.now();
	const hour = 3_600_000;
	return [
		{
			id: 1,
			message: 'Welcome to the AUC exchange portal. Start by exploring partner universities.',
			createdAt: new Date(now - 48 * hour).toISOString(),
			read: true
		},
		{
			id: 2,
			message: 'Your application is 43% complete. Finish the questionnaire to raise it.',
			createdAt: new Date(now - 26 * hour).toISOString(),
			read: false
		},
		{
			id: 3,
			message: 'Review your saved universities before the application deadline.',
			createdAt: new Date(now - 4 * hour).toISOString(),
			read: false
		}
	];
}

class DiscoveryStore {
	savedSlugs = $state<string[]>([]);
	compareSlugs = $state<string[]>([]);
	notifications = $state<StudentNotification[]>([]);
	initialized = $state(false);

	private accountId = '';

	/** The saved schools, in the order they were saved. */
	saved = $derived(catalog.bySlugs(this.savedSlugs));

	/** The schools in the compare tray, in selection order. */
	compare = $derived(catalog.bySlugs(this.compareSlugs));

	unreadCount = $derived(this.notifications.filter((item) => !item.read).length);

	/** Load this account's list once. Seeds a demo list the first time. */
	init(accountId: string): void {
		if (!accountId) return;
		if (this.initialized && this.accountId === accountId) return;

		this.accountId = accountId;
		const stored = readStored(accountId);
		if (stored) {
			this.savedSlugs = stored.saved;
			this.compareSlugs = stored.compare;
			this.notifications = stored.notifications;
		} else {
			this.savedSlugs = [...SEED_SAVED];
			this.compareSlugs = [];
			this.notifications = seedNotifications();
			this.persist();
		}
		this.initialized = true;
	}

	isSaved(slug: string): boolean {
		return this.savedSlugs.includes(slug);
	}

	/** Returns true when the school is saved after the toggle. */
	toggleSaved(slug: string): boolean {
		if (!slug) return false;
		const wasSaved = this.savedSlugs.includes(slug);
		this.savedSlugs = wasSaved
			? this.savedSlugs.filter((item) => item !== slug)
			: [...this.savedSlugs, slug];
		this.persist();
		return !wasSaved;
	}

	isCompared(slug: string): boolean {
		return this.compareSlugs.includes(slug);
	}

	/**
	 * Returns true when the school is in the tray after the toggle.
	 * Returns false when it was removed, or when the tray is already full.
	 */
	toggleCompare(slug: string): boolean {
		if (!slug) return false;
		if (this.compareSlugs.includes(slug)) {
			this.compareSlugs = this.compareSlugs.filter((item) => item !== slug);
			this.persist();
			return false;
		}
		if (this.compareSlugs.length >= MAX_COMPARE) return false;
		this.compareSlugs = [...this.compareSlugs, slug];
		this.persist();
		return true;
	}

	clearCompare(): void {
		this.compareSlugs = [];
		this.persist();
	}

	notify(message: string): void {
		const id = Math.max(0, ...this.notifications.map((item) => item.id)) + 1;
		this.notifications = [
			...this.notifications,
			{ id, message, createdAt: new Date().toISOString(), read: false }
		];
		this.persist();
	}

	markAllRead(): void {
		this.notifications = this.notifications.map((item) => ({ ...item, read: true }));
		this.persist();
	}

	/** Marks the first unread notification, the one at the front of the queue. */
	markOldestRead(): void {
		const index = this.notifications.findIndex((item) => !item.read);
		if (index < 0) return;
		this.notifications = this.notifications.map((item, i) =>
			i === index ? { ...item, read: true } : item
		);
		this.persist();
	}

	private persist(): void {
		if (!this.accountId || typeof localStorage === 'undefined') return;
		try {
			localStorage.setItem(
				storageKey(this.accountId),
				JSON.stringify({
					saved: this.savedSlugs,
					compare: this.compareSlugs,
					notifications: this.notifications
				})
			);
		} catch {
			// Storage blocked or full: the lists still work for this visit.
		}
	}
}

export const discovery = new DiscoveryStore();

export type { UniversityRecord };
