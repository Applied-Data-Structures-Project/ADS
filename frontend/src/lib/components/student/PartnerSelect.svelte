<script lang="ts">
	// Searchable partner university dropdown modeled after MajorSelect.svelte.
	// Opens consistently downward with region group headings and quick search by
	// university name, country, city, or region.
	import Icon from './Icon.svelte';
	import type { UniversityRecord } from '#lib/explore/universities.ts';

	interface Props {
		id?: string;
		universities: UniversityRecord[];
		value: string;
		plannedCounts?: Map<string, number>;
		onchange?: (slug: string) => void;
	}

	let {
		id = 'partner-select',
		universities,
		value = '',
		plannedCounts = new Map<string, number>(),
		onchange
	}: Props = $props();

	let open = $state(false);
	let query = $state('');
	let rootEl: HTMLDivElement | null = null;
	let searchEl = $state<HTMLInputElement | null>(null);

	const selectedRecord = $derived(universities.find((u) => u.slug === value) ?? null);

	const grouped = $derived.by(() => {
		const needle = query.trim().toLowerCase();
		const matching = needle
			? universities.filter((u) => {
					const name = (u.official_name.value ?? u.slug).toLowerCase();
					const shortName = (u.short_name.value ?? '').toLowerCase();
					const country = (u.country.value ?? '').toLowerCase();
					const city = (u.city.value ?? '').toLowerCase();
					const region = (u.region.value ?? '').toLowerCase();
					return (
						name.includes(needle) ||
						shortName.includes(needle) ||
						country.includes(needle) ||
						city.includes(needle) ||
						region.includes(needle)
					);
				})
			: universities;

		const map = new Map<string, UniversityRecord[]>();
		for (const u of matching) {
			const region = u.region.value?.trim() || 'Other Regions';
			const bucket = map.get(region);
			if (bucket) bucket.push(u);
			else map.set(region, [u]);
		}

		return [...map.entries()]
			.sort(([a], [b]) => a.localeCompare(b))
			.map(([region, items]) => ({
				region,
				items: items.sort((a, b) =>
					(a.official_name.value ?? a.slug).localeCompare(b.official_name.value ?? b.slug)
				)
			}));
	});

	function toggle() {
		open = !open;
		if (open) {
			query = '';
			queueMicrotask(() => searchEl?.focus());
		}
	}

	function select(slug: string) {
		open = false;
		query = '';
		onchange?.(slug);
	}

	function handleWindowClick(event: MouseEvent) {
		if (!open || !rootEl) return;
		if (!rootEl.contains(event.target as Node)) {
			open = false;
		}
	}

	function handleKeydown(event: KeyboardEvent) {
		if (event.key === 'Escape' && open) {
			open = false;
		}
	}
</script>

<svelte:window onclick={handleWindowClick} onkeydown={handleKeydown} />

<div bind:this={rootEl} class="relative min-w-0">
	<button
		{id}
		type="button"
		onclick={toggle}
		aria-haspopup="listbox"
		aria-expanded={open}
		class="w-full h-11 px-3.5 bg-white text-sm rounded-xl border {open
			? 'border-black'
			: 'border-[#c6c6cd]/60'} focus:outline-none focus:border-black shadow-sm flex items-center justify-between gap-2 text-left cursor-pointer"
	>
		<span class="truncate {selectedRecord ? 'text-[#0b1c30] font-medium' : 'text-[#515f74]'}">
			{#if selectedRecord}
				{selectedRecord.official_name.value ?? selectedRecord.slug}
				{#if selectedRecord.country.value}
					<span class="text-[#515f74] font-normal"> · {selectedRecord.country.value}</span>
				{/if}
			{:else}
				Select a partner university…
			{/if}
		</span>
		<svg
			class="w-4 h-4 text-[#515f74] shrink-0 transition-transform duration-150 {open ? 'rotate-180' : ''}"
			viewBox="0 0 24 24"
			fill="none"
			stroke="currentColor"
			stroke-width="2"
			stroke-linecap="round"
			stroke-linejoin="round"
			aria-hidden="true"
		>
			<polyline points="6 9 12 15 18 9" />
		</svg>
	</button>

	{#if open}
		<div
			role="listbox"
			aria-label="Partner Universities"
			class="absolute left-0 right-0 top-full mt-1.5 z-50 rounded-xl border border-[#c6c6cd]/70 bg-white shadow-lg overflow-hidden"
		>
			<div class="p-2 border-b border-[#c6c6cd]/40 bg-[#f8f9ff]">
				<div class="relative">
					<span class="absolute left-2.5 top-1/2 -translate-y-1/2 text-[#515f74] pointer-events-none">
						<Icon name="search" class="w-3.5 h-3.5" />
					</span>
					<input
						bind:this={searchEl}
						bind:value={query}
						type="text"
						placeholder="Search by university name, country, or city…"
						class="w-full h-9 pl-8 pr-3 rounded-lg border border-[#c6c6cd]/60 bg-white text-xs text-[#0b1c30] placeholder:text-[#515f74] focus:outline-none focus:border-black focus:ring-0"
					/>
				</div>
			</div>

			<div class="max-h-72 overflow-y-auto py-1">
				{#if grouped.length === 0}
					<p class="px-3.5 py-4 text-xs text-[#515f74] text-center">
						No matching partner university found.
					</p>
				{:else}
					{#each grouped as group (group.region)}
						<div class="px-3 pt-2.5 pb-1 text-[10px] font-bold uppercase tracking-wider text-[#515f74]">
							{group.region} ({group.items.length})
						</div>
						{#each group.items as item (item.slug)}
							{@const selected = item.slug === value}
							{@const plannedCount = plannedCounts.get(item.slug) ?? 0}
							<button
								type="button"
								role="option"
								aria-selected={selected}
								onclick={() => select(item.slug)}
								class="w-full px-3.5 py-2 text-left text-sm flex items-center justify-between gap-2 transition-colors cursor-pointer {selected
									? 'bg-[#dce9ff]/80 text-[#0b1c30] font-semibold'
									: 'text-[#0b1c30] hover:bg-[#eff4ff]'}"
							>
								<span class="min-w-0 flex-1">
									<span class="block truncate">{item.official_name.value ?? item.slug}</span>
									<span class="block truncate text-[11px] font-normal text-[#515f74]">
										{[item.city.value, item.country.value].filter(Boolean).join(', ')}
									</span>
								</span>
								<span class="flex items-center gap-1.5 shrink-0">
									{#if plannedCount > 0}
										<span class="rounded-full bg-[#dcfce7] text-[#15803d] px-1.5 py-0.5 text-[10px] font-bold font-mono-num">
											{plannedCount} planned
										</span>
									{/if}
									{#if selected}
										<Icon name="check" class="w-3.5 h-3.5 text-[#131b2e]" strokeWidth={2.5} />
									{/if}
								</span>
							</button>
						{/each}
					{/each}
				{/if}
			</div>
		</div>
	{/if}
</div>
