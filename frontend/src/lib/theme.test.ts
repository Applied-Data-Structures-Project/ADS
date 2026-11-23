import { describe, expect, it } from 'vitest';
import {
	LEGACY_THEME_KEYS,
	THEME_STORAGE_KEY,
	persistDarkMode,
	resolveStoredDarkMode
} from './theme.ts';

function createMemoryStorage(initial: Record<string, string> = {}) {
	const map = new Map<string, string>(Object.entries(initial));
	return {
		getItem(key: string): string | null {
			return map.has(key) ? map.get(key)! : null;
		},
		setItem(key: string, value: string): void {
			map.set(key, value);
		},
		removeItem(key: string): void {
			map.delete(key);
		},
		snapshot(): Record<string, string> {
			return Object.fromEntries(map.entries());
		}
	};
}

describe('unified theme storage', () => {
	it('defaults to light mode when storage is missing or empty', () => {
		expect(resolveStoredDarkMode(null)).toBe(false);
		expect(resolveStoredDarkMode(undefined)).toBe(false);
		expect(resolveStoredDarkMode(createMemoryStorage())).toBe(false);
	});

	it('respects canonical auc-theme-dark over legacy per-portal keys', () => {
		const darkStore = createMemoryStorage({ [THEME_STORAGE_KEY]: '1' });
		expect(resolveStoredDarkMode(darkStore)).toBe(true);

		const lightOverride = createMemoryStorage({
			[THEME_STORAGE_KEY]: '0',
			'auc-student-dark-mode': '1',
			'auc-admin-dark-mode': '1'
		});
		expect(resolveStoredDarkMode(lightOverride)).toBe(false);
	});

	it('migrates seamlessly when either legacy portal key is enabled', () => {
		expect(
			resolveStoredDarkMode(createMemoryStorage({ 'auc-student-dark-mode': '1' }))
		).toBe(true);
		expect(
			resolveStoredDarkMode(createMemoryStorage({ 'auc-admin-dark-mode': '1' }))
		).toBe(true);
		expect(
			resolveStoredDarkMode(createMemoryStorage({ 'auc-admin-dark-mode': '0' }))
		).toBe(false);
	});

	it('persists canonical key and removes legacy split keys', () => {
		const storage = createMemoryStorage({
			'auc-student-dark-mode': '1',
			'auc-admin-dark-mode': '0'
		});

		persistDarkMode(storage, true);
		expect(storage.getItem(THEME_STORAGE_KEY)).toBe('1');
		for (const legacy of LEGACY_THEME_KEYS) {
			expect(storage.getItem(legacy)).toBeNull();
		}

		persistDarkMode(storage, false);
		expect(storage.snapshot()).toEqual({ [THEME_STORAGE_KEY]: '0' });
	});
});
