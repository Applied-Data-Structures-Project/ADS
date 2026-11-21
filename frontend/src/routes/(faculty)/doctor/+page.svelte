<script lang="ts">
	// G3 (Malak): a faculty member answers recommendation requests, oldest first.
	// The server keeps a HashMap from doctor email to a Queue of request ids: front() is shown here,
	// and answering or declining pop()s it.
	import PageHeader from '#lib/components/student/PageHeader.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import { store } from '#lib/student/store.svelte.ts';

	let text = $state('');
	let error = $state<string | null>(null);
	let busy = $state(false); // true while waiting for the server, so a double click sends once

	const front = $derived(store.doctorQueue[0]);
	const waiting = $derived(store.doctorQueue.slice(1));

	async function send() {
		if (!front || busy) return;
		busy = true;
		error = await store.answerLetter(front.id, text);
		busy = false;
		if (!error) text = '';
	}

	async function decline() {
		if (!front || busy) return;
		busy = true;
		await store.declineLetter(front.id);
		busy = false;
		text = '';
		error = null;
	}
</script>

<svelte:head><title>Letter Requests · AUC Exchange</title></svelte:head>

<section class="space-y-5 md:space-y-6">
	<PageHeader
		crumbs={['Faculty', 'Recommendation Letter Requests']}
		title="Recommendation Letter Requests"
		subtitle="Students who asked you for a study abroad recommendation. Requests are answered in the order they arrived."
	/>

	<div class="grid grid-cols-2 lg:grid-cols-3 gap-3 sm:gap-4">
		<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40">
			<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">Waiting</div>
			<div class="mt-2.5 text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tnum">{store.doctorQueue.length}</div>
		</div>
		<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40">
			<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">Answered</div>
			<div class="mt-2.5 text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tnum">{store.doctorHistory.length}</div>
		</div>
		<div class="col-span-2 lg:col-span-1 bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40">
			<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">Oldest request</div>
			<div class="mt-2.5 text-lg sm:text-xl font-bold text-[#0b1c30] font-mono-num">{front?.requestedAt ?? '—'}</div>
		</div>
	</div>

	<div class="grid grid-cols-1 lg:grid-cols-3 gap-4">
		<!-- Front of the queue -->
		<div class="lg:col-span-2 bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
			<div class="px-4 sm:px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex items-center justify-between gap-3">
				<span class="text-base sm:text-lg font-semibold text-[#0b1c30]">Next request</span>
				{#if front}<span class="px-2.5 py-0.5 rounded-full bg-[#e5eeff] text-[#0b1c30] text-xs font-semibold">Front of queue</span>{/if}
			</div>
			{#if !front}
				<div class="p-10 text-center space-y-2">
					<div class="mx-auto w-12 h-12 rounded-xl bg-[#f0fdf4] text-[#15803d] flex items-center justify-center">
						<Icon name="check" class="w-6 h-6" strokeWidth={2.4} />
					</div>
					<p class="text-sm text-[#515f74]">No requests waiting. You're all caught up.</p>
				</div>
			{:else}
				<div class="p-4 sm:p-5 space-y-4">
					<div class="grid grid-cols-2 sm:grid-cols-4 gap-3.5 p-4 bg-[#eff4ff] rounded-xl text-sm">
						<div class="col-span-2 min-w-0">
							<span class="text-[#515f74] block text-xs">Student</span>
							<span class="font-semibold text-[#0b1c30]">{front.studentName}</span>
							<span class="block text-xs font-mono-num text-[#515f74] truncate">{front.studentEmail}</span>
						</div>
						<div>
							<span class="text-[#515f74] block text-xs">Major</span>
							<span class="font-semibold text-[#0b1c30]">{front.studentMajor}</span>
						</div>
						<div>
							<span class="text-[#515f74] block text-xs">GPA</span>
							<span class="font-mono-num font-bold text-[#0b1c30]">{front.studentGpa.toFixed(2)}</span>
						</div>
					</div>
					<div class="space-y-1.5">
						<label for="letter-text" class="text-xs font-semibold text-[#515f74] uppercase tracking-wider">Letter</label>
						<textarea
							id="letter-text"
							rows="9"
							bind:value={text}
							placeholder={`I am pleased to recommend ${front.studentName} for AUC's Fall 2027 study abroad program…`}
							class="w-full px-3.5 py-3 bg-white text-[#0b1c30] text-sm leading-relaxed rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm placeholder:text-[#515f74]"
						></textarea>
						<div class="text-xs text-[#515f74]">Follow IPSO's Recommendation Letter Template: how you know the student and why they will thrive abroad.</div>
					</div>
					{#if error}
						<p class="px-3.5 py-2.5 rounded-xl bg-[#fef2f2] text-[#ba1a1a] text-sm font-medium" role="alert">{error}</p>
					{/if}
				</div>
				<div class="px-4 sm:px-5 py-4 border-t border-[#c6c6cd]/30 bg-[#eff4ff] flex justify-end gap-2.5">
					<button type="button" onclick={decline} disabled={busy} class="disabled:opacity-50 h-10 px-4 bg-white border border-[#c6c6cd]/40 rounded-xl text-xs sm:text-sm font-semibold shadow-sm cursor-pointer">
						Decline
					</button>
					<button type="button" onclick={send} disabled={busy} class="disabled:opacity-50 h-10 px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white rounded-xl text-xs sm:text-sm font-semibold shadow-sm cursor-pointer inline-flex items-center gap-2">
						<Icon name="letters" class="w-4 h-4" /> Send letter
					</button>
				</div>
			{/if}
		</div>

		<!-- Rest of the queue + history -->
		<div class="space-y-4">
			<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 p-4 sm:p-5 space-y-2.5">
				<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Then ({waiting.length})</div>
				{#each waiting as l, i (l.id)}
					<div class="flex items-center gap-3 px-3.5 py-2.5 bg-white border border-[#c6c6cd]/40 rounded-lg shadow-sm">
						<span class="font-mono-num text-xs font-bold text-[#515f74]">{i + 2}</span>
						<div class="min-w-0 flex-1">
							<div class="text-sm font-medium text-[#0b1c30] truncate">{l.studentName}</div>
							<div class="text-xs text-[#515f74]">{l.requestedAt}</div>
						</div>
					</div>
				{:else}
					<div class="text-sm text-[#515f74]">Nobody else is waiting.</div>
				{/each}
			</div>
			<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 p-4 sm:p-5 space-y-2.5">
				<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Answered</div>
				{#each store.doctorHistory as l (l.id)}
					<div class="flex items-center justify-between gap-2 text-sm">
						<span class="text-[#0b1c30] truncate">{l.studentName}</span>
						<span
							class="px-2 py-0.5 rounded-full text-[11px] font-semibold shrink-0 {l.status === 'Received'
								? 'bg-[#f0fdf4] text-[#15803d]'
								: 'bg-[#fef2f2] text-[#ba1a1a]'}"
						>
							{l.status === 'Received' ? 'Sent' : 'Declined'}
						</span>
					</div>
				{:else}
					<div class="text-sm text-[#515f74]">None yet.</div>
				{/each}
			</div>
		</div>
	</div>
</section>
