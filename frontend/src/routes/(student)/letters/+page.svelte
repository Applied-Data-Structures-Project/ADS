<script lang="ts">
	// G3 (Malak): ask AUC faculty for the required recommendation letter.
	// Each request joins that doctor's queue (HashMap: doctor email → Queue of requests), answered oldest first.
	import PageHeader from '#lib/components/student/PageHeader.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import { store } from '#lib/student/store.svelte.ts';
	import { RULES } from '#lib/student/mocks.ts';
	import type { LetterStatus } from '#lib/student/types.ts';

	let doctorName = $state('');
	let doctorEmail = $state('');
	let error = $state<string | null>(null);

	const active = $derived(store.myLetters.filter((l) => l.status !== 'Declined').length);
	const canAsk = $derived(active < RULES.maxLetters && !store.locked);

	function send(e: SubmitEvent) {
		e.preventDefault();
		error = store.requestLetter(doctorName, doctorEmail);
		if (!error) {
			doctorName = '';
			doctorEmail = '';
		}
	}

	function pill(status: LetterStatus) {
		if (status === 'Received') return 'bg-[#f0fdf4] text-[#15803d]';
		if (status === 'Declined') return 'bg-[#fef2f2] text-[#ba1a1a]';
		return 'bg-[#fffbeb] text-[#b45309]';
	}

	const field =
		'w-full h-11 px-3.5 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm';
	const labelCls = 'text-xs font-semibold text-[#515f74] uppercase tracking-wider';
</script>

<svelte:head><title>Recommendation Letter · AUC Exchange</title></svelte:head>

<section class="space-y-5 md:space-y-6">
	<PageHeader
		crumbs={['My Application', 'Recommendation Letter']}
		title="Recommendation Letter"
		subtitle="Required: one letter from an AUC faculty member who can speak to how you'll do abroad. They don't need to have taught you."
	/>

	<div
		class="rounded-xl px-4 sm:px-5 py-3.5 flex flex-wrap items-center gap-2.5 text-sm border border-[#c6c6cd]/40 {store.hasLetter
			? 'bg-[#f0fdf4]'
			: 'bg-[#fffbeb]'}"
	>
		<span class={store.hasLetter ? 'text-[#15803d]' : 'text-[#b45309]'}>
			<Icon name={store.hasLetter ? 'check' : 'alert'} class="w-5 h-5" />
		</span>
		<span class="font-semibold text-[#0b1c30]">
			{store.hasLetter ? 'Letter received. This requirement is complete.' : 'You need at least 1 received letter to submit.'}
		</span>
		<span class="text-[#515f74]">You can ask up to {RULES.maxLetters} faculty members, so a decline doesn't block you.</span>
	</div>

	<div class="grid grid-cols-1 lg:grid-cols-3 gap-4">
		<!-- Requests -->
		<div class="lg:col-span-2 bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
			<div class="px-4 sm:px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex items-center justify-between gap-3">
				<span class="text-base sm:text-lg font-semibold text-[#0b1c30]">My requests</span>
				<span class="px-3 py-0.5 bg-white border border-[#c6c6cd]/40 rounded-full text-xs font-mono-num font-semibold text-[#0b1c30]">
					{active} of {RULES.maxLetters} active
				</span>
			</div>
			{#if store.myLetters.length === 0}
				<div class="p-8 text-center text-sm text-[#515f74]">No requests yet. Ask a faculty member using the form.</div>
			{:else}
				<ul class="divide-y divide-[#c6c6cd]/20">
					{#each store.myLetters as l (l.id)}
						<li class="px-4 sm:px-5 py-4 flex flex-col sm:flex-row sm:items-center gap-3">
							<div class="flex-1 min-w-0">
								<div class="font-semibold text-[15px] text-[#0b1c30]">{l.doctorName}</div>
								<div class="text-xs sm:text-[13px] text-[#515f74] font-mono-num truncate">{l.doctorEmail}</div>
								<div class="text-xs text-[#515f74] mt-0.5">
									Requested {l.requestedAt}{l.answeredAt ? ` · answered ${l.answeredAt}` : ''}
								</div>
							</div>
							<span class="px-2.5 py-1 rounded-full text-xs font-semibold self-start sm:self-auto {pill(l.status)}">{l.status}</span>
						</li>
					{/each}
				</ul>
			{/if}
		</div>

		<!-- New request -->
		<form onsubmit={send} class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 p-4 sm:p-5 space-y-4 self-start">
			<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Ask a faculty member</div>
			<fieldset class="space-y-4" disabled={!canAsk}>
				<div class="flex flex-col gap-1.5">
					<label for="l-name" class={labelCls}>Name</label>
					<input id="l-name" class={field} bind:value={doctorName} placeholder="Dr. Hala Fawzy" />
				</div>
				<div class="flex flex-col gap-1.5">
					<label for="l-email" class={labelCls}>AUC email</label>
					<input id="l-email" type="email" class={field} bind:value={doctorEmail} placeholder="name@aucegypt.edu" />
				</div>
				{#if error}
					<p class="px-3.5 py-2.5 rounded-xl bg-[#fef2f2] text-[#ba1a1a] text-sm font-medium" role="alert">{error}</p>
				{/if}
				<button
					type="submit"
					class="w-full h-11 bg-[#131b2e] hover:bg-[#1e293b] text-white text-sm font-semibold rounded-xl shadow-sm cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed inline-flex items-center justify-center gap-2"
				>
					<Icon name="letters" class="w-4 h-4" /> Send request
				</button>
			</fieldset>
			<p class="text-xs text-[#515f74]">
				{#if store.locked}
					Your application is submitted, so requests are closed.
				{:else if !canAsk}
					You've reached {RULES.maxLetters} active requests.
				{:else}
					They'll use IPSO's Recommendation Letter Template. They can also email it to studyabroad@aucegypt.edu with your name and ID in the subject.
				{/if}
			</p>
		</form>
	</div>
</section>
