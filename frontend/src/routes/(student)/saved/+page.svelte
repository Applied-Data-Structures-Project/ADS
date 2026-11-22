<script lang="ts">
	// Saved universities, ordered in C++ by nearest deadline (Min-Heap),
	// best fit (mergeSort), or lowest cost (mergeSort). Recommendations are
	// computed in C++ by Max-Heap (GET /api/recommendations).
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';
	import PageHeader from '#lib/components/student/PageHeader.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import CompareTray from '#lib/components/student/CompareTray.svelte';
	import {
		eligibilityBadgeClass,
		type UniversityEligibilityStatus
	} from '#lib/explore/eligibility.ts';
	import {
		daysLeft,
		formatDeadline,
		formatUsd,
		SAMPLE_METRICS_NOTE
	} from '#lib/explore/metrics.ts';
	import {
		discovery,
		type SavedSortBy,
		type ServerRecommendation
	} from '#lib/student/discovery.svelte.ts';
	import { store } from '#lib/student/store.svelte.ts';

	type UniversityCard = {
		slug: string;
		name: string;
		location: string;
		region: string;
		fitScore: number;
		estimatedSemesterCostUsd: number;
		withinBudget: boolean;
		deadline: string;
		status: UniversityEligibilityStatus;
		statusLabel: string;
		reason: string;
		matchSummary: string;
	};

	let sortBy = $state<SavedSortBy>(discovery.savedSort);

	onMount(() => {
		void discovery.loadSortedSaved(sortBy);
		void discovery.loadRecommendations(3);
	});

	const loading = $derived(discovery.savedLoading && !discovery.savedFromServer);
	const budget = $derived(store.application.budget);

	function toCard(item: ServerRecommendation): UniversityCard {
		const status: UniversityEligibilityStatus = item.eligible
			? item.provisional
				? 'provisional'
				: 'eligible'
			: 'ineligible';
		return {
			slug: item.slug,
			name: item.name || item.slug,
			location: [item.city, item.country].filter(Boolean).join(', ') || 'Location not available',
			region: item.region || 'Region TBC',
			fitScore: item.fitScore,
			estimatedSemesterCostUsd: item.estimatedSemesterCostUsd,
			withinBudget: item.withinBudget,
			deadline: item.deadline,
			status,
			statusLabel:
				status === 'eligible'
					? 'Eligible'
					: status === 'provisional'
						? 'Provisionally eligible'
						: 'Not eligible',
			reason: item.eligibilityReason || item.matchSummary,
			matchSummary: item.matchSummary
		};
	}

	/** Ordered directly by C++ Min-Heap (deadline) or C++ mergeSort (fit / cost). */
	const ordered = $derived(discovery.sortedSaved.map(toCard));

	/** Top three unsaved recommendations computed by C++ Max-Heap (GET /api/recommendations). */
	const recommendations = $derived(discovery.recommendations.map(toCard));

	const algorithmLabel = $derived(
		discovery.savedAlgorithm === 'cpp-min-heap'
			? 'C++ Min-Heap'
			: discovery.savedAlgorithm === 'cpp-merge-sort'
				? 'C++ Merge Sort'
				: 'C++'
	);

	function changeSort(next: SavedSortBy) {
		sortBy = next;
		void discovery.loadSortedSaved(next);
	}

	function prefRank(slug: string): number {
		return store.application.pref.indexOf(slug);
	}

	function useInApplication() {
		if (ordered.length < 3) {
			store.showToast('Save at least three universities first.');
			return;
		}
		store.useSavedPreferences(ordered.slice(0, 3).map((item) => item.slug));
		store.showToast('Saved schools copied into your application preferences.');
		goto('/apply?step=3');
	}

	function toggleSave(slug: string) {
		const saved = discovery.toggleSaved(slug);
		store.showToast(saved ? 'Saved to your list.' : 'Removed from your list.');
	}

	function toggleCompare(slug: string) {
		if (discovery.isCompared(slug)) {
			discovery.toggleCompare(slug);
			return;
		}
		if (!discovery.toggleCompare(slug)) {
			store.showToast('You can compare up to three universities at a time.');
		}
	}
</script>

<svelte:head><title>Saved Universities · AUC Exchange</title></svelte:head>

<section class="space-y-6 pb-16">
	<PageHeader
		crumbs={['Discover', 'Saved Universities']}
		title="Saved Universities"
		subtitle="Your shortlist. Your three application preferences are chosen from this list."
	>
		{#snippet actions()}
			<button
				type="button"
				onclick={useInApplication}
				disabled={ordered.length < 3}
				class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm inline-flex items-center gap-2 cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
			>
				Use in my application <Icon name="arrow-right" class="w-4 h-4" />
			</button>
		{/snippet}
	</PageHeader>

	{#if loading}
		<div class="grid sm:grid-cols-2 xl:grid-cols-3 gap-4">
			{#each [1, 2, 3] as i (i)}
				<div class="h-72 rounded-2xl border border-[#c6c6cd]/40 bg-white animate-pulse"></div>
			{/each}
		</div>
	{:else if !discovery.savedFromServer}
		<div role="alert" class="rounded-xl border border-amber-200 bg-amber-50 p-5 text-amber-900 text-sm">
			Could not reach the C++ backend to sort your saved universities. Start <code class="font-mono">ads_server</code> and refresh this page.
		</div>
	{:else if ordered.length === 0}
		<div class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-10 text-center space-y-3">
			<div class="flex justify-center text-[#515f74]"><Icon name="saved" class="w-9 h-9" /></div>
			<h2 class="text-lg font-bold text-[#0b1c30]">Nothing saved yet</h2>
			<p class="text-sm text-[#515f74] max-w-md mx-auto">
				Save a university from Explore to build your shortlist. Saved schools can be compared and
				copied into your application.
			</p>
			<a
				href="/explore"
				class="inline-flex items-center gap-2 rounded-xl bg-[#0b1c30] text-white px-5 py-2.5 text-sm font-semibold hover:bg-[#233853]"
			>
				Explore universities <Icon name="arrow-right" class="w-4 h-4" />
			</a>
		</div>
	{:else}
		<div class="flex flex-wrap items-end justify-between gap-3">
			<div>
				<div class="flex items-center gap-2 flex-wrap">
					<h2 class="text-xl font-bold text-[#0b1c30]">{ordered.length} saved</h2>
					<span class="rounded-lg border border-[#c6c6cd]/50 bg-white px-2.5 py-1 text-[11px] font-medium text-[#515f74]">
						{algorithmLabel}
					</span>
				</div>
				<p class="text-sm text-[#515f74] mt-1">Choose an order, then compare or apply.</p>
			</div>
			<div class="flex items-center gap-1 rounded-xl border border-[#c6c6cd]/50 bg-white p-1">
				{#each [
					{ id: 'deadline', label: 'Nearest deadline' },
					{ id: 'fit', label: 'Best fit' },
					{ id: 'cost', label: 'Lowest cost' }
				] as option (option.id)}
					<button
						type="button"
						onclick={() => changeSort(option.id as SavedSortBy)}
						aria-pressed={sortBy === option.id}
						class="rounded-lg px-3 py-2 text-xs font-semibold transition-colors cursor-pointer {sortBy ===
						option.id
							? 'bg-[#0b1c30] text-white'
							: 'text-[#515f74] hover:bg-[#eff4ff]'}"
					>
						{option.label}
					</button>
				{/each}
			</div>
		</div>

		<div class="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-4">
			{#each ordered as item (item.slug)}
				{@const days = daysLeft(item.deadline)}
				{@const rank = prefRank(item.slug)}
				{@const withinBudget = budget > 0 && item.withinBudget}
				<article
					class="flex flex-col rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm hover:border-[#a4b3d0] transition-colors"
				>
					<div class="flex items-start justify-between gap-3">
						<div class="min-w-0">
							<div class="flex items-center gap-2 flex-wrap">
								{#if rank >= 0}
									<span
										class="rounded-full bg-[#131b2e] text-white px-2 py-0.5 text-[11px] font-bold"
									>
										Preference #{rank + 1}
									</span>
								{/if}
								<h3 class="text-[16px] font-bold leading-snug text-[#0b1c30]">
									{item.name}
								</h3>
							</div>
							<p class="mt-1.5 text-sm text-[#515f74]">
								{item.location}
							</p>
						</div>
						<span
							class="shrink-0 rounded-xl bg-[#eff4ff] px-2.5 py-1.5 text-sm font-bold text-[#0b1c30] font-mono-num"
							title="Fit score computed in C++"
						>
							{item.fitScore}%
						</span>
					</div>

					<div class="mt-3 flex flex-wrap gap-1.5">
						<span
							title={item.reason}
							class="rounded-lg px-2.5 py-1 text-[11px] font-semibold {eligibilityBadgeClass(
								item.status
							)}"
						>
							{item.statusLabel}
						</span>
						<span class="rounded-lg bg-[#f2f3f7] px-2.5 py-1 text-[11px] text-[#515f74]">
							{item.region}
						</span>
					</div>
					{#if item.status === 'ineligible'}
						<p class="mt-1.5 text-[11px] font-medium text-[#ba1a1a]">{item.reason}</p>
					{/if}

					<dl class="mt-4 grid grid-cols-2 gap-2 text-xs">
						<div class="rounded-xl bg-[#f8faff] border border-[#dce4f0] px-3 py-2.5">
							<dt class="text-[#515f74]">Semester cost</dt>
							<dd class="mt-1 font-bold text-[#0b1c30] font-mono-num">
								{formatUsd(item.estimatedSemesterCostUsd)}
							</dd>
							<dd
								class="mt-0.5 text-[11px] {budget <= 0
									? 'text-[#515f74]'
									: withinBudget
										? 'text-[#15803d]'
										: 'text-[#b45309]'}"
							>
								{budget <= 0 ? 'No budget set' : withinBudget ? 'Within budget' : 'Over budget'}
							</dd>
						</div>
						<div class="rounded-xl bg-[#f8faff] border border-[#dce4f0] px-3 py-2.5">
							<dt class="text-[#515f74]">Deadline</dt>
							<dd
								class="mt-1 font-bold font-mono-num {days < 0
									? 'text-[#ba1a1a]'
									: days <= 30
										? 'text-[#b45309]'
										: 'text-[#0b1c30]'}"
							>
								{days < 0 ? 'Closed' : `${days} days left`}
							</dd>
							<dd class="mt-0.5 text-[11px] text-[#515f74]">
								{formatDeadline(item.deadline)}
							</dd>
						</div>
					</dl>

					<div class="mt-auto pt-4 flex gap-2">
						<a
							href={`/explore/university/${item.slug}`}
							class="flex-1 inline-flex justify-center items-center gap-1.5 rounded-xl bg-[#0b1c30] text-white hover:bg-[#233853] py-2.5 text-xs font-semibold"
						>
							View <Icon name="arrow-right" class="w-3.5 h-3.5" />
						</a>
						<button
							type="button"
							onclick={() => toggleCompare(item.slug)}
							aria-pressed={discovery.isCompared(item.slug)}
							class="rounded-xl border px-3 py-2.5 text-xs font-semibold inline-flex items-center gap-1.5 cursor-pointer {discovery.isCompared(
								item.slug
							)
								? 'border-[#131b2e] bg-[#dce9ff]/80 text-[#0b1c30]'
								: 'border-[#c6c6cd]/50 bg-white text-[#515f74] hover:bg-[#eff4ff]'}"
						>
							<Icon name="compare" class="w-3.5 h-3.5" /> Compare
						</button>
						<button
							type="button"
							onclick={() => toggleSave(item.slug)}
							aria-label={`Remove ${item.name} from saved`}
							class="rounded-xl border border-[#c6c6cd]/50 bg-white px-3 py-2.5 text-[#515f74] hover:bg-[#fef2f2] hover:text-[#ba1a1a] cursor-pointer"
						>
							<Icon name="x" class="w-3.5 h-3.5" />
						</button>
					</div>
				</article>
			{/each}
		</div>

		{#if ordered.length < 3}
			<div class="rounded-xl bg-[#fffbeb] px-4 py-3 text-sm text-[#b45309] font-medium">
				You have {ordered.length} saved. Save at least 3 universities before copying them into your
				application.
			</div>
		{/if}
	{/if}

	{#if !loading && recommendations.length > 0}
		<section class="space-y-3 pt-2">
			<div class="flex flex-wrap items-end justify-between gap-2">
				<div>
					<h2 class="text-xl font-bold text-[#0b1c30]">Recommended for you</h2>
					<p class="text-sm text-[#515f74] mt-1">Not in your saved list yet.</p>
				</div>
				<span class="rounded-lg border border-[#c6c6cd]/50 bg-white px-3 py-2 text-[11px] text-[#515f74]">
					Top 3 by fit · C++ Max-Heap
				</span>
			</div>
			<div class="grid grid-cols-1 md:grid-cols-3 gap-4">
				{#each recommendations as item (item.slug)}
					<article class="flex flex-col rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm">
						<div class="flex items-start justify-between gap-2">
							<h3 class="text-sm font-bold leading-snug text-[#0b1c30]">
								{item.name}
							</h3>
							<span class="shrink-0 text-sm font-bold font-mono-num text-[#0b1c30]">
								{item.fitScore}%
							</span>
						</div>
						<p class="mt-1 text-xs text-[#515f74]">
							{item.location}
						</p>
						<div class="mt-2 flex flex-wrap gap-1.5">
							<span
								title={item.reason}
								class="inline-block rounded-lg px-2 py-0.5 text-[11px] font-semibold {eligibilityBadgeClass(
									item.status
								)}"
							>
								{item.statusLabel}
							</span>
							{#if item.withinBudget}
								<span class="inline-block rounded-lg bg-[#eff4ff] px-2 py-0.5 text-[11px] font-semibold text-[#314c76]">
									Within budget
								</span>
							{/if}
						</div>
						<p class="mt-2 text-xs text-[#515f74]">{item.matchSummary}</p>
						<p class="mt-2 text-xs text-[#515f74]">
							Estimated semester cost
							<span class="font-semibold text-[#0b1c30] font-mono-num">
								{formatUsd(item.estimatedSemesterCostUsd)}
							</span>
						</p>
						<div class="mt-auto pt-4 flex gap-2">
							<button
								type="button"
								onclick={() => {
									discovery.toggleSaved(item.slug);
									store.showToast('Saved to your list.');
								}}
								class="flex-1 rounded-xl bg-[#131b2e] text-white hover:bg-[#1e293b] py-2.5 text-xs font-semibold cursor-pointer inline-flex items-center justify-center gap-1.5"
							>
								<Icon name="saved" class="w-3.5 h-3.5" /> Save
							</button>
							<a
								href={`/explore/university/${item.slug}`}
								class="rounded-xl border border-[#c6c6cd]/50 px-3 py-2.5 text-xs font-semibold text-[#515f74] hover:bg-[#eff4ff]"
							>
								View
							</a>
						</div>
					</article>
				{/each}
			</div>
		</section>
	{/if}

	<p class="text-xs text-[#515f74]">{SAMPLE_METRICS_NOTE}</p>
	<p class="text-[11px] text-[#657087]">
		Saved order &amp; recommendations run in C++: nearest deadline uses Heap (min-heap), best fit and lowest cost use mergeSort, and recommendations use Heap (max-heap).
	</p>
</section>

<CompareTray />
