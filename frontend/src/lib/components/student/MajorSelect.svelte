<script lang="ts">
	// Shared AUC major picker used by both /signup and /profile.
	// Opens consistently downward with school group headings and quick search,
	// avoiding native OS <select> clipping and upward-flip quirks.
	import Icon from './Icon.svelte';
	import { groupedMajorsForLevel } from '#lib/student/auc-majors.ts';
	import type { Level } from '#lib/student/types.ts';

	interface Props {
		id?: string;
		level: Level | '';
		value: string;
		invalid?: boolean;
	}

	let { id = 'major', level, value = $bindable(''), invalid = false }: Props = $props();

	let open = $state(false);
	let query = $state('');
	let rootEl: HTMLDivElement | null = null;
	let searchEl = $state<HTMLInputElement | null>(null);

	const groups = $derived(groupedMajorsForLevel(level));

	const filteredGroups = $derived.by(() => {
		const needle = query.trim().toLowerCase();
		if (!needle) return groups;
		return groups
			.map((g) => ({
				school: g.school,
				majors: g.majors.filter(
					(m) =>
						m.name.toLowerCase().includes(needle) ||
						g.school.toLowerCase().includes(needle) ||
						m.domains.some((d) => d.toLowerCase().includes(needle))
				)
			}))
			.filter((g) => g.majors.length > 0);
	});

	function toggle() {
		open = !open;
		if (open) {
			query = '';
			queueMicrotask(() => searchEl?.focus());
		}
	}

	function select(name: string) {
		value = name;
		open = false;
		query = '';
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
		class="w-full h-11 px-3.5 bg-white text-sm rounded-xl border {invalid
			? 'border-[#ba1a1a]/70'
			: open
				? 'border-black'
				: 'border-[#c6c6cd]/60'} focus:outline-none focus:border-black shadow-sm flex items-center justify-between gap-2 text-left cursor-pointer"
	>
		<span class="truncate {value ? 'text-[#0b1c30] font-medium' : 'text-[#515f74]'}">
			{value || 'Select your AUC major…'}
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
			aria-label="AUC Majors"
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
						placeholder="Search AUC majors…"
						class="w-full h-9 pl-8 pr-3 rounded-lg border border-[#c6c6cd]/60 bg-white text-xs text-[#0b1c30] placeholder:text-[#515f74] focus:outline-none focus:border-black focus:ring-0"
					/>
				</div>
			</div>

			<div class="max-h-64 overflow-y-auto py-1">
				{#if filteredGroups.length === 0}
					<p class="px-3.5 py-4 text-xs text-[#515f74] text-center">No matching AUC major found.</p>
				{:else}
					{#each filteredGroups as group (group.school)}
						<div class="px-3 pt-2.5 pb-1 text-[10px] font-bold uppercase tracking-wider text-[#515f74]">
							{group.school}
						</div>
						{#each group.majors as item (item.name)}
							{@const selected = item.name === value}
							<button
								type="button"
								role="option"
								aria-selected={selected}
								onclick={() => select(item.name)}
								class="w-full px-3.5 py-2 text-left text-sm flex items-center justify-between gap-2 transition-colors cursor-pointer {selected
									? 'bg-[#dce9ff]/80 text-[#0b1c30] font-semibold'
									: 'text-[#0b1c30] hover:bg-[#eff4ff]'}"
							>
								<span class="truncate">{item.name}</span>
								{#if selected}
									<Icon name="check" class="w-3.5 h-3.5 text-[#131b2e] shrink-0" strokeWidth={2.5} />
								{/if}
							</button>
						{/each}
					{/each}
				{/if}
			</div>
		</div>
	{/if}
</div>
