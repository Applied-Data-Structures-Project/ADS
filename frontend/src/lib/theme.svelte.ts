import {
	LEGACY_THEME_KEYS,
	THEME_STORAGE_KEY,
	persistDarkMode,
	resolveStoredDarkMode
} from './theme.ts';

class ThemeStore {
	dark = $state(false);
	private initialized = false;

	private apply(next: boolean) {
		this.dark = next;
		if (typeof document !== 'undefined') {
			document.documentElement.classList.toggle('admin-dark', next);
		}
	}

	/** Synchronizes theme state across all portals, tabs, and legacy storage keys. */
	init() {
		if (typeof window === 'undefined') return;
		if (this.initialized) {
			try {
				this.apply(resolveStoredDarkMode(localStorage));
			} catch {
				// Ignore storage access restrictions.
			}
			return;
		}
		this.initialized = true;

		let stored = false;
		let hadCanonical = false;
		try {
			hadCanonical = localStorage.getItem(THEME_STORAGE_KEY) !== null;
			stored = resolveStoredDarkMode(localStorage);
			if (!hadCanonical && stored) {
				persistDarkMode(localStorage, stored);
			}
		} catch {
			stored = false;
		}
		this.apply(stored);

		window.addEventListener('storage', (event) => {
			if (
				event.key === THEME_STORAGE_KEY ||
				(event.key && (LEGACY_THEME_KEYS as readonly string[]).includes(event.key))
			) {
				try {
					this.apply(resolveStoredDarkMode(localStorage));
				} catch {
					// Ignore storage access restrictions.
				}
			}
		});
	}

	set(next: boolean) {
		this.apply(next);
		try {
			persistDarkMode(localStorage, next);
		} catch {
			// Storage blocked: the choice lasts for this visit only.
		}
	}

	toggle(): boolean {
		this.set(!this.dark);
		return this.dark;
	}
}

export const theme = new ThemeStore();
