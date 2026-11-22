// Saved schools, the compare tray and notifications for Group 2's pages.
//
// Saved universities and notifications live on the C++ server (DiscoveryService's
// HashMaps and FIFO Queue, persisted in SQLite). localStorage holds the 3-slot
// compare tray and acts as an offline cache if the C++ server is not running.

import { catalog } from '../explore/catalog.svelte.ts';
import type { UniversityRecord } from '../explore/universities.ts';
import { callServer, ServerError } from './api.ts';

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

interface ServerSavedResponse {
	saved: string[];
}

interface ServerNotificationsResponse {
	notifications: StudentNotification[];
	unreadCount: number;
}

/** Real slugs, one per region, used only if the C++ backend is offline on a first visit. */
const OFFLINE_SEED_SAVED = [
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

function seedOfflineNotifications(): StudentNotification[] {
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
	serverConnected = $state(false);

	private accountId = '';

	/** The saved schools, in the order they were saved. */
	saved = $derived(catalog.bySlugs(this.savedSlugs));

	/** The schools in the compare tray, in selection order. */
	compare = $derived(catalog.bySlugs(this.compareSlugs));

	unreadCount = $derived(this.notifications.filter((item) => !item.read).length);

	/** Load cached local state (including the compare tray) once per account. */
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
			this.savedSlugs = [];
			this.compareSlugs = [];
			this.notifications = [];
		}
		this.initialized = true;
	}

	/** Load saved universities and FIFO notifications from the C++ server. */
	async loadFromServer(accountId: string): Promise<boolean> {
		this.init(accountId);
		try {
			const [savedRes, notifRes] = await Promise.all([
				callServer<ServerSavedResponse>('GET', '/api/saved'),
				callServer<ServerNotificationsResponse>('GET', '/api/notifications')
			]);
			this.savedSlugs = Array.isArray(savedRes.saved) ? savedRes.saved : [];
			this.notifications = Array.isArray(notifRes.notifications) ? notifRes.notifications : [];
			this.serverConnected = true;
			this.persist();
			return true;
		} catch {
			// Offline fallback: if nothing was cached yet, populate demo defaults so UI stays usable.
			this.serverConnected = false;
			if (readStored(accountId) === null && this.savedSlugs.length === 0) {
				this.savedSlugs = [...OFFLINE_SEED_SAVED];
				this.notifications = seedOfflineNotifications();
				this.persist();
			}
			return false;
		}
	}

	/** Re-fetch notifications from GET /api/notifications after backend events (submit, letters, profile). */
	async refreshNotifications(): Promise<void> {
		try {
			const res = await callServer<ServerNotificationsResponse>('GET', '/api/notifications');
			if (Array.isArray(res.notifications)) {
				this.notifications = res.notifications;
				this.serverConnected = true;
				this.persist();
			}
		} catch {
			// Offline: keep current list.
		}
	}

	isSaved(slug: string): boolean {
		return this.savedSlugs.includes(slug);
	}

	/** Returns true when the school is saved after the toggle. Syncs with POST/DELETE /api/saved/<slug>. */
	toggleSaved(slug: string): boolean {
		if (!slug) return false;
		const before = [...this.savedSlugs];
		const wasSaved = before.includes(slug);
		this.savedSlugs = wasSaved ? before.filter((item) => item !== slug) : [...before, slug];
		this.persist();

		const method = wasSaved ? 'DELETE' : 'POST';
		void callServer<ServerSavedResponse>(method, `/api/saved/${encodeURIComponent(slug)}`)
			.then((res) => {
				if (Array.isArray(res.saved)) {
					this.savedSlugs = res.saved;
					this.serverConnected = true;
					this.persist();
				}
			})
			.catch((e) => {
				// Roll back only on an explicit server refusal (not when offline with status 0).
				if (e instanceof ServerError && e.status > 0) {
					this.savedSlugs = before;
					this.persist();
				}
			});

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
		const trimmed = message.trim();
		if (!trimmed) return;
		const id = Math.max(0, ...this.notifications.map((item) => item.id)) + 1;
		this.notifications = [
			...this.notifications,
			{ id, message: trimmed, createdAt: new Date().toISOString(), read: false }
		];
		this.persist();

		void callServer<ServerNotificationsResponse>('POST', '/api/notifications', { message: trimmed })
			.then((res) => {
				if (Array.isArray(res.notifications)) {
					this.notifications = res.notifications;
					this.serverConnected = true;
					this.persist();
				}
			})
			.catch(() => {
				// Keep optimistic local notification when offline.
			});
	}

	markAllRead(): void {
		const before = this.notifications;
		this.notifications = this.notifications.map((item) => ({ ...item, read: true }));
		this.persist();

		void callServer<ServerNotificationsResponse>('POST', '/api/notifications/read-all')
			.then((res) => {
				if (Array.isArray(res.notifications)) {
					this.notifications = res.notifications;
					this.serverConnected = true;
					this.persist();
				}
			})
			.catch((e) => {
				if (e instanceof ServerError && e.status > 0) {
					this.notifications = before;
					this.persist();
				}
			});
	}

	/** Marks the first unread notification, the one at the front of the queue. */
	markOldestRead(): void {
		const index = this.notifications.findIndex((item) => !item.read);
		if (index < 0) return;
		const before = this.notifications;
		this.notifications = this.notifications.map((item, i) =>
			i === index ? { ...item, read: true } : item
		);
		this.persist();

		void callServer<ServerNotificationsResponse>('POST', '/api/notifications/read-oldest')
			.then((res) => {
				if (Array.isArray(res.notifications)) {
					this.notifications = res.notifications;
					this.serverConnected = true;
					this.persist();
				}
			})
			.catch((e) => {
				if (e instanceof ServerError && e.status > 0) {
					this.notifications = before;
					this.persist();
				}
			});
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
