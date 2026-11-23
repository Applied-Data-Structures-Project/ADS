export const THEME_STORAGE_KEY = 'auc-theme-dark';
export const LEGACY_THEME_KEYS = ['auc-student-dark-mode', 'auc-admin-dark-mode'] as const;

/**
 * Resolves dark mode preference from storage, falling back once to any legacy
 * per-portal key (`auc-student-dark-mode` or `auc-admin-dark-mode`).
 */
export function resolveStoredDarkMode(
	storage: Pick<Storage, 'getItem'> | null | undefined
): boolean {
	if (!storage) return false;
	const saved = storage.getItem(THEME_STORAGE_KEY);
	if (saved !== null) return saved === '1';
	return LEGACY_THEME_KEYS.some((key) => storage.getItem(key) === '1');
}

/**
 * Persists the unified theme key and removes stale legacy per-portal keys.
 */
export function persistDarkMode(
	storage: Pick<Storage, 'setItem' | 'removeItem'> | null | undefined,
	dark: boolean
): void {
	if (!storage) return;
	storage.setItem(THEME_STORAGE_KEY, dark ? '1' : '0');
	for (const key of LEGACY_THEME_KEYS) {
		storage.removeItem(key);
	}
}
