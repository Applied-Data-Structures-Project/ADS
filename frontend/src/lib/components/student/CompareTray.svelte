<script lang="ts">
	// Floating compare tray, shared by Explore, Saved and Compare.
	import Icon from './Icon.svelte';
	import { discovery } from '#lib/student/discovery.svelte.ts';

	const count = $derived(discovery.compareSlugs.length);
	const ready = $derived(count >= 2);

	function initials(name: string | null | undefined): string {
		return (name ?? '?')
			.replace(/^Dr\.\s*/, '')
			.split(/\s+/)
			.filter(Boolean)
			.slice(0, 2)
			.map((word) => word[0] ?? '')
			.join('')
			.toUpperCase();
	}

	const summary = $derived(
		discovery.compare
			.map((record) => record.short_name.value ?? record.official_name.value ?? '')
			.join(' · ')
	);
</script>

{#if count > 0}
	<div class="fixed bottom-20 md:bottom-5 left-3 right-3 md:left-auto md:right-6 md:w-[430px] z-40">
		<div class="rounded-2xl bg-[#0b1c30] text-white shadow-xl p-3.5 flex items-center gap-3">
			<div class="flex -space-x-2 shrink-0">
				{#each discovery.compare as record (record.slug)}
					<span
						class="w-9 h-9 rounded-full bg-white/15 border-2 border-[#0b1c30] flex items-center justify-center text-[11px] font-bold"
						title={record.official_name.value ?? record.slug}
					>
						{initials(record.short_name.value ?? record.official_name.value)}
					</span>
				{/each}
			</div>
			<div class="min-w-0 flex-1">
				<p class="text-xs font-semibold">{count} of 3 selected</p>
				<p class="text-[11px] text-[#cad9f4] truncate">{summary}</p>
			</div>
			{#if ready}
				<a
					href="/compare"
					class="shrink-0 inline-flex items-center gap-1.5 rounded-xl bg-white text-[#0b1c30] px-3 py-2 text-xs font-semibold hover:bg-[#eff4ff]"
				>
					Compare <Icon name="arrow-right" class="w-3.5 h-3.5" />
				</a>
			{:else}
				<span class="shrink-0 rounded-xl bg-white/10 px-3 py-2 text-[11px] font-semibold text-[#cad9f4]">
					Pick one more
				</span>
			{/if}
			<button
				type="button"
				onclick={() => discovery.clearCompare()}
				aria-label="Clear compare selection"
				class="shrink-0 text-[#cad9f4] hover:text-white cursor-pointer"
			>
				<Icon name="x" class="w-4 h-4" />
			</button>
		</div>
	</div>
{/if}
