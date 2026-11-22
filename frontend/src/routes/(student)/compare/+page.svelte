<script lang="ts">
	// Side-by-side comparison of up to three saved schools. The best value in
	// each row is highlighted; missing values never win.
	import PageHeader from '#lib/components/student/PageHeader.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import CompareTray from '#lib/components/student/CompareTray.svelte';
	import { catalog } from '#lib/explore/catalog.svelte.ts';
	import { bestColumns, COMPARE_ROWS } from '#lib/explore/compare.ts';
	import { demoMetrics, formatUsd, SAMPLE_METRICS_NOTE } from '#lib/explore/metrics.ts';
	import { discovery } from '#lib/student/discovery.svelte.ts';
	import { store } from '#lib/student/store.svelte.ts';

	const loading = $derived(catalog.status === 'idle' || catalog.status === 'loading');
	const profile = $derived(store.eligibilityProfile);
	const budget = $derived(store.application.budget);

	const columns = $derived(
		discovery.compare.map((record) => ({
			record,
			metrics: demoMetrics(record, profile, budget)
		}))
	);

	const advisorLink = $derived(
		`mailto:studyabroad@aucegypt.edu?subject=${encodeURIComponent(
			`Compare universities: ${columns
				.map((column) => column.record.official_name.value ?? column.record.slug)
				.join(', ')}`
		)}`
	);

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
</script>

<svelte:head><title>Compare Universities · AUC Exchange</title></svelte:head>

<section class="space-y-5 pb-16">
	<PageHeader
		crumbs={['Discover', 'Saved Universities', 'Compare']}
		title="Compare Universities"
		subtitle="Up to three schools side by side. The best value in each row is highlighted in green."
	>
		{#snippet actions()}
			<a
				href="/saved"
				class="h-10 px-4 bg-white border border-[#c6c6cd]/40 rounded-xl text-xs sm:text-sm font-semibold shadow-sm inline-flex items-center gap-2"
			>
				<Icon name="arrow-left" class="w-4 h-4" /> Saved list
			</a>
		{/snippet}
	</PageHeader>

	{#if loading}
		<div class="h-80 rounded-2xl border border-[#c6c6cd]/40 bg-white animate-pulse"></div>
	{:else if columns.length < 2}
		<div class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-10 text-center space-y-3">
			<div class="flex justify-center text-[#515f74]"><Icon name="compare" class="w-9 h-9" /></div>
			<h2 class="text-lg font-bold text-[#0b1c30]">Pick at least two universities</h2>
			<p class="text-sm text-[#515f74] max-w-md mx-auto">
				Add schools to the compare tray from Explore or your saved list, then come back here.
			</p>
			<div class="flex flex-wrap justify-center gap-2 pt-1">
				<a
					href="/saved"
					class="inline-flex items-center gap-2 rounded-xl bg-[#0b1c30] text-white px-5 py-2.5 text-sm font-semibold hover:bg-[#233853]"
				>
					Go to saved <Icon name="arrow-right" class="w-4 h-4" />
				</a>
				<a
					href="/explore"
					class="inline-flex items-center gap-2 rounded-xl border border-[#c6c6cd]/60 bg-white px-5 py-2.5 text-sm font-semibold hover:bg-[#eff4ff]"
				>
					Explore universities
				</a>
			</div>
		</div>
	{:else}
		<div class="flex flex-wrap items-center justify-between gap-3">
			<p class="text-sm text-[#515f74]">
				Comparing {columns.length} schools.
				{budget > 0
					? `Your budget is ${formatUsd(budget)} per semester.`
					: 'Set a budget on Me to see how each school fits it.'}
				{#if budget <= 0}
					<a href="/me" class="font-semibold text-[#003ea8] hover:underline">Set it on Me</a>
				{/if}
			</p>
			<button
				type="button"
				onclick={() => discovery.clearCompare()}
				class="text-xs font-semibold text-[#515f74] hover:text-[#0b1c30] cursor-pointer"
			>
				Clear selection
			</button>
		</div>

		<div class="overflow-x-auto rounded-2xl border border-[#c6c6cd]/50 bg-white shadow-sm">
			<table class="w-full min-w-[780px] border-collapse text-sm">
				<thead>
					<tr>
						<th
							scope="col"
							class="sticky left-0 z-10 bg-white border-b border-[#c6c6cd]/40 px-4 py-4 text-left w-44"
						>
							<span class="text-[11px] font-bold uppercase tracking-wider text-[#515f74]">
								Comparison
							</span>
						</th>
						{#each columns as column (column.record.slug)}
							<th scope="col" class="border-b border-[#c6c6cd]/40 px-4 py-4 text-left align-top">
								<div class="flex items-start gap-2.5">
									<span
										class="w-9 h-9 rounded-lg bg-[#eff4ff] text-[#0b1c30] flex items-center justify-center text-xs font-bold shrink-0"
									>
										{initials(
											column.record.short_name.value ?? column.record.official_name.value
										)}
									</span>
									<div class="min-w-0">
										<div class="text-sm font-bold leading-snug text-[#0b1c30]">
											{column.record.official_name.value ?? column.record.slug}
										</div>
										<div class="mt-1 text-xs text-[#515f74]">
											{[column.record.city.value, column.record.country.value]
												.filter(Boolean)
												.join(', ') || 'Location not available'}
										</div>
										<div class="mt-1.5 text-xs font-semibold text-[#314c76]">
											Fit {column.metrics.fit}%
										</div>
									</div>
									<button
										type="button"
										onclick={() => discovery.toggleCompare(column.record.slug)}
										aria-label={`Remove ${column.record.official_name.value ?? column.record.slug} from compare`}
										class="ml-auto text-[#515f74] hover:text-[#ba1a1a] cursor-pointer"
									>
										<Icon name="x" class="w-4 h-4" />
									</button>
								</div>
							</th>
						{/each}
					</tr>
				</thead>
				<tbody>
					{#each COMPARE_ROWS as row (row.id)}
						{@const values = columns.map((column) =>
							row.value(column.record, column.metrics, budget)
						)}
						{@const best = bestColumns(values, row.direction)}
						<tr class="border-b border-[#c6c6cd]/25 last:border-0">
							<th
								scope="row"
								class="sticky left-0 z-10 bg-white px-4 py-3 text-left text-xs font-semibold text-[#515f74]"
							>
								{row.label}
							</th>
							{#each columns as column, i (column.record.slug)}
								<td class="px-4 py-3 align-top {best[i] ? 'bg-[#f0fdf4]' : ''}">
									<span class="font-medium text-[#0b1c30]">
										{row.format(column.record, column.metrics, budget)}
									</span>
									{#if best[i]}
										<span
											class="ml-2 inline-block rounded-full bg-[#dcfce7] px-2 py-0.5 text-[10px] font-bold text-[#15803d]"
										>
											Best
										</span>
									{/if}
								</td>
							{/each}
						</tr>
					{/each}
				</tbody>
			</table>
		</div>

		<div class="flex flex-wrap items-center justify-between gap-3">
			<a
				href={advisorLink}
				class="h-10 px-4 bg-[#131b2e] hover:bg-[#1e293b] text-white rounded-xl text-xs sm:text-sm font-semibold inline-flex items-center gap-2"
			>
				Ask an advisor which to choose <Icon name="arrow-right" class="w-4 h-4" />
			</a>
			<span class="text-xs text-[#515f74]">
				Sample values are seeded from a hash of each university's slug.
			</span>
		</div>
	{/if}

	<p class="text-xs text-[#515f74]">{SAMPLE_METRICS_NOTE}</p>
</section>

<CompareTray />
