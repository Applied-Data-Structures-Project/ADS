<script lang="ts">
	// G3 (Malak): where the application is in IPSO's review.
	// The student sets Draft → Submitted; the admin side moves it to In Review, then Verified or Ineligible.
	import PageHeader from '#lib/components/student/PageHeader.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import { store } from '#lib/student/store.svelte.ts';
	import { catalog } from '#lib/explore/catalog.svelte.ts';
	import { DOCUMENTS } from '#lib/student/mocks.ts';
	import type { AppStatus } from '#lib/student/types.ts';

	const a = $derived(store.application);

	const STAGES: { status: AppStatus; title: string; detail: string }[] = [
		{ status: 'Draft', title: 'Draft', detail: 'You are filling in the form. You can still change anything.' },
		{ status: 'Submitted', title: 'Submitted', detail: 'Received by IPSO. No more edits can be made.' },
		{ status: 'In Review', title: 'Eligibility scan', detail: 'IPSO confirms your documents are complete and you meet the criteria.' },
		{ status: 'Verified', title: 'Evaluation', detail: 'The selection committee reviews your materials in detail.' }
	];

	const order: AppStatus[] = ['Draft', 'Submitted', 'In Review', 'Verified'];
	const current = $derived(a.status === 'Ineligible' ? 2 : order.indexOf(a.status));

	function fmt(iso: string) {
		if (!iso) return '';
		return new Date(iso).toLocaleString('en-GB', { day: 'numeric', month: 'short', year: 'numeric', hour: 'numeric', minute: '2-digit' });
	}
</script>

<svelte:head><title>Application Status · AUC Exchange</title></svelte:head>

<section class="space-y-5 md:space-y-6">
	<PageHeader crumbs={['My Application', 'Application Status']} title="Application Status">
		{#snippet actions()}
			{#if a.status === 'Draft'}
				<a
					href="/apply"
					class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm inline-flex items-center gap-2"
				>
					Continue application <Icon name="arrow-right" class="w-4 h-4" />
				</a>
			{/if}
		{/snippet}
	</PageHeader>

	<div class="grid grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
		<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
			<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">Status</div>
			<div class="mt-2.5">
				<span
					class="px-3 py-1 rounded-full text-sm font-semibold {a.status === 'Verified'
						? 'bg-[#f0fdf4] text-[#15803d]'
						: a.status === 'Ineligible'
							? 'bg-[#fef2f2] text-[#ba1a1a]'
							: a.status === 'Draft'
								? 'bg-[#eff4ff] text-[#515f74]'
								: 'bg-[#fffbeb] text-[#b45309]'}"
				>
					{a.status}
				</span>
			</div>
			<div class="mt-2.5 text-xs sm:text-sm text-[#515f74]">{a.submittedAt ? `Since ${fmt(a.submittedAt)}` : 'Not submitted yet'}</div>
		</div>
		<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
			<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">Completed</div>
			<div class="mt-2.5 text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tnum">{store.progress}%</div>
			<div class="mt-1.5 text-xs sm:text-sm text-[#515f74]">{store.stepsDone.filter(Boolean).length} of 7 steps</div>
		</div>
		<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
			<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">Documents</div>
			<div class="mt-2.5 text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tnum">
				{DOCUMENTS.filter((d) => store.docs[d.type]).length}<span class="text-base text-[#515f74] font-medium"> / {DOCUMENTS.length}</span>
			</div>
			<div class="mt-1.5 text-xs sm:text-sm text-[#515f74]">PDFs uploaded</div>
		</div>
		<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
			<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">Letter</div>
			<div class="mt-2.5 text-lg sm:text-xl font-bold {store.hasLetter ? 'text-[#15803d]' : 'text-[#b45309]'}">
				{store.hasLetter ? 'Received' : store.myLetters.length ? 'Pending' : 'Not requested'}
			</div>
			<div class="mt-1.5 text-xs sm:text-sm text-[#515f74]">{store.myLetters.length} of 2 requests</div>
		</div>
	</div>

	<div class="grid grid-cols-1 lg:grid-cols-3 gap-4">
		<!-- Timeline -->
		<div class="lg:col-span-2 bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
			<div class="px-4 sm:px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/30 text-base sm:text-lg font-semibold text-[#0b1c30]">
				Review process
			</div>
			<ol class="p-4 sm:p-5 space-y-0">
				{#each STAGES as s, i (s.status)}
					{@const done = i < current}
					{@const now = i === current}
					<li class="relative flex gap-4 pb-6 last:pb-0">
						{#if i < STAGES.length - 1}
							<span class="absolute left-[15px] top-8 bottom-0 w-0.5 {done ? 'bg-[#131b2e]' : 'bg-[#c6c6cd]'}"></span>
						{/if}
						<span
							class="relative z-10 w-8 h-8 rounded-full flex items-center justify-center shrink-0 text-xs font-bold font-mono-num {done
								? 'bg-[#131b2e] text-white'
								: now
									? 'bg-[#dce9ff] text-[#0b1c30] border-2 border-[#131b2e]'
									: 'bg-[#eff4ff] text-[#515f74]'}"
						>
							{#if done}<Icon name="check" class="w-4 h-4" strokeWidth={2.5} />{:else}{i + 1}{/if}
						</span>
						<div class="min-w-0 pt-1">
							<div class="flex items-center gap-2 flex-wrap">
								<span class="text-sm sm:text-[15px] font-semibold {now || done ? 'text-[#0b1c30]' : 'text-[#515f74]'}">{s.title}</span>
								{#if now}<span class="px-2 py-0.5 rounded-full bg-[#e5eeff] text-[#0b1c30] text-[11px] font-semibold">Current</span>{/if}
							</div>
							<div class="text-xs sm:text-sm text-[#515f74]">{s.detail}</div>
							{#if s.status === 'Submitted' && a.submittedAt}
								<div class="text-xs font-mono-num text-[#0b1c30] mt-0.5">{fmt(a.submittedAt)}</div>
							{/if}
						</div>
					</li>
				{/each}
			</ol>
			{#if a.status === 'Ineligible'}
				<div class="mx-4 sm:mx-5 mb-5 rounded-xl bg-[#fef2f2] px-4 py-3 text-sm text-[#ba1a1a] font-medium">
					IPSO marked this application as ineligible. Contact studyabroad@aucegypt.edu for details.
				</div>
			{:else}
				<div class="mx-4 sm:mx-5 mb-5 rounded-xl bg-[#eff4ff] px-4 py-3 text-xs sm:text-sm text-[#0b1c30]">
					You'll be notified when the review is complete. Matched students then see their nomination here.
				</div>
			{/if}
		</div>

		<!-- Preferences summary -->
		<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 p-4 sm:p-5 space-y-3 self-start">
			<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Your ranked preferences</div>
			{#each a.pref as slug, i (i)}
				{@const u = catalog.bySlug(slug)}
				<div class="flex items-center gap-3 px-3.5 py-2.5 bg-white border border-[#c6c6cd]/40 rounded-lg shadow-sm">
					<span class="font-mono-num font-bold text-sm">#{i + 1}</span>
					<span class="text-sm min-w-0 truncate {u ? 'text-[#0b1c30] font-medium' : 'text-[#515f74]'}">{u?.official_name.value ?? 'Not chosen'}</span>
				</div>
			{/each}
			<div class="text-xs text-[#515f74] pt-2 border-t border-[#c6c6cd]/20">
				{a.programType || 'Program type not chosen'} · Fall 2027
			</div>
		</div>
	</div>
</section>
