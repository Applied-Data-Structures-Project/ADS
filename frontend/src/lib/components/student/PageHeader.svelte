<script lang="ts">
	// Breadcrumb + big title + optional actions on the right, as on every admin page.
	import type { Snippet } from 'svelte';

	interface Props {
		crumbs: string[];
		title: string;
		subtitle?: string;
		actions?: Snippet;
	}

	let { crumbs, title, subtitle, actions }: Props = $props();
</script>

<div class="flex flex-col sm:flex-row sm:items-end justify-between gap-3.5">
	<div class="flex flex-col gap-1.5 min-w-0">
		<div class="flex items-center gap-1.5 text-xs font-medium text-[#515f74] flex-wrap">
			<span>AUC IPSO</span>
			{#each crumbs as crumb, i (crumb)}
				<span class="text-[#c6c6cd] {i === crumbs.length - 1 ? 'hidden sm:inline' : ''}">&rsaquo;</span>
				<span class={i === crumbs.length - 1 ? 'hidden sm:inline text-[#0b1c30] font-semibold' : ''}>{crumb}</span>
			{/each}
		</div>
		<h1 class="text-2xl sm:text-3xl md:text-[32px] font-bold tracking-tight text-[#0b1c30] leading-tight">
			{title}
		</h1>
		{#if subtitle}
			<p class="text-sm text-[#515f74] max-w-2xl">{subtitle}</p>
		{/if}
	</div>
	{#if actions}
		<div class="flex items-center gap-2.5 flex-wrap shrink-0">{@render actions()}</div>
	{/if}
</div>
