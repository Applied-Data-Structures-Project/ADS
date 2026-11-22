<script lang="ts">
	// "Me": the student's academic summary, budget, notifications and letters.
	// Notifications arrive ordered oldest-first by C++ mergeSort + FIFO Queue.
	import PageHeader from '#lib/components/student/PageHeader.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import { auth } from '#lib/auth/store.svelte.ts';
	import { discovery } from '#lib/student/discovery.svelte.ts';
	import { store } from '#lib/student/store.svelte.ts';

	const accountId = $derived(auth.account?.homeAccountId ?? auth.email);
	const profile = $derived(store.eligibilityProfile);

	const notifications = $derived(discovery.notifications);

	const englishStatus = $derived(
		profile && (profile.englishComplete || profile.rhet120) ? 'Complete' : 'Not complete'
	);

	function when(iso: string): string {
		return new Date(iso).toLocaleString('en-GB', {
			day: 'numeric',
			month: 'short',
			hour: 'numeric',
			minute: '2-digit'
		});
	}

	function saveBudget() {
		store.saveBudget(accountId, store.application.budget);
	}
</script>

<svelte:head><title>Me · AUC Exchange</title></svelte:head>

<section class="space-y-5 md:space-y-6 pb-16">
	<PageHeader
		crumbs={['Account', 'Me']}
		title="Me"
		subtitle="Profile, notifications and recommendation letters."
	/>

	<div class="grid grid-cols-1 lg:grid-cols-[minmax(0,1fr)_minmax(280px,360px)] gap-4 items-start">
		<div class="space-y-4">
			<!-- PROFILE -->
			<section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm space-y-4">
				<div class="flex items-start justify-between gap-3">
					<div>
						<h2 class="text-lg font-bold text-[#0b1c30]">Academic profile</h2>
						<p class="text-sm text-[#515f74] mt-1">
							Used by the eligibility check and the sample fit score.
						</p>
					</div>
					<a
						href="/profile"
						class="shrink-0 h-9 px-3 rounded-lg border border-[#c6c6cd]/60 text-xs font-semibold text-[#0b1c30] hover:bg-[#eff4ff] inline-flex items-center gap-1.5"
					>
						<Icon name="profile" class="w-3.5 h-3.5" /> Edit
					</a>
				</div>

				{#if profile}
					<dl class="grid grid-cols-2 sm:grid-cols-4 gap-3 text-sm">
						<div class="rounded-xl bg-[#f8faff] border border-[#dce4f0] px-3.5 py-3">
							<dt class="text-xs text-[#515f74]">Level</dt>
							<dd class="mt-1 font-bold text-[#0b1c30]">{profile.level || '—'}</dd>
						</div>
						<div class="rounded-xl bg-[#f8faff] border border-[#dce4f0] px-3.5 py-3">
							<dt class="text-xs text-[#515f74]">GPA</dt>
							<dd class="mt-1 font-bold text-[#0b1c30] font-mono-num">
								{profile.gpa === null ? '—' : profile.gpa.toFixed(2)}
							</dd>
						</div>
						<div class="rounded-xl bg-[#f8faff] border border-[#dce4f0] px-3.5 py-3 col-span-2">
							<dt class="text-xs text-[#515f74]">Major</dt>
							<dd class="mt-1 font-bold text-[#0b1c30] truncate">{profile.major || '—'}</dd>
						</div>
						<div class="rounded-xl bg-[#f8faff] border border-[#dce4f0] px-3.5 py-3 col-span-2">
							<dt class="text-xs text-[#515f74]">English requirement</dt>
							<dd
								class="mt-1 font-bold {englishStatus === 'Complete'
									? 'text-[#15803d]'
									: 'text-[#b45309]'}"
							>
								{englishStatus}
							</dd>
						</div>
					</dl>
				{:else}
					<div class="rounded-xl bg-[#fffbeb] px-4 py-3 text-sm text-[#b45309]">
						No eligibility profile saved yet.
						<a href="/profile" class="font-semibold underline">Fill it in</a> to use the
						eligibility check and fit scores.
					</div>
				{/if}
			</section>

			<!-- NOTIFICATIONS -->
			<section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm space-y-4">
				<div class="flex flex-wrap items-start justify-between gap-3">
					<div>
						<div class="flex items-center gap-2">
							<h2 class="text-lg font-bold text-[#0b1c30]">Notifications</h2>
							{#if discovery.unreadCount > 0}
								<span
									class="rounded-full bg-[#131b2e] text-white px-2 py-0.5 text-[11px] font-bold font-mono-num"
								>
									{discovery.unreadCount} new
								</span>
							{/if}
						</div>
						<p class="text-sm text-[#515f74] mt-1">Oldest first, like a queue.</p>
					</div>
					<div class="flex flex-wrap gap-2">
						<button
							type="button"
							onclick={() => discovery.markOldestRead()}
							disabled={discovery.unreadCount === 0}
							class="h-9 px-3 rounded-lg border border-[#c6c6cd]/60 text-xs font-semibold text-[#0b1c30] hover:bg-[#eff4ff] cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
						>
							Mark oldest read
						</button>
						<button
							type="button"
							onclick={() => discovery.markAllRead()}
							disabled={discovery.unreadCount === 0}
							class="h-9 px-3 rounded-lg bg-[#131b2e] text-white text-xs font-semibold hover:bg-[#1e293b] cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
						>
							Mark all read
						</button>
					</div>
				</div>

				{#if notifications.length === 0}
					<div class="rounded-xl bg-[#eff4ff] px-4 py-3 text-sm text-[#515f74]">
						No notifications yet.
					</div>
				{:else}
					<ul class="space-y-2">
						{#each notifications as item (item.id)}
							<li
								class="rounded-xl border px-4 py-3 flex items-start gap-3 {item.read
									? 'border-[#c6c6cd]/40 bg-white'
									: 'border-[#b8cbe9] bg-[#f3f7ff]'}"
							>
								<span
									class="mt-1.5 w-2 h-2 rounded-full shrink-0 {item.read
										? 'bg-[#c6c6cd]'
										: 'bg-[#131b2e]'}"
									aria-hidden="true"
								></span>
								<div class="min-w-0 flex-1">
									<p
										class="text-sm {item.read
											? 'text-[#515f74]'
											: 'text-[#0b1c30] font-semibold'}"
									>
										{item.message}
									</p>
									<p class="mt-1 text-[11px] text-[#657087] font-mono-num">
										{when(item.createdAt)}
									</p>
								</div>
								{#if !item.read}
									<span class="text-[10px] font-bold uppercase tracking-wider text-[#314c76]">
										Unread
									</span>
								{/if}
							</li>
						{/each}
					</ul>
				{/if}
			</section>
		</div>

		<aside class="space-y-4">
			<!-- BUDGET -->
			<section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm space-y-3">
				<h2 class="text-base font-bold text-[#0b1c30]">Semester budget</h2>
				<p class="text-sm text-[#515f74]">
					Used by Compare and by the saved-list cost check. Amounts are USD.
				</p>
				<label for="me-budget" class="block text-xs font-semibold uppercase tracking-wider text-[#515f74]">
					Budget per semester
				</label>
				<div class="flex gap-2">
					<input
						id="me-budget"
						type="number"
						min="0"
						step="100"
						bind:value={store.application.budget}
						class="w-full h-11 px-3.5 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm font-mono-num"
					/>
					<button
						type="button"
						onclick={saveBudget}
						class="shrink-0 h-11 px-4 rounded-xl bg-[#131b2e] text-white text-xs font-semibold hover:bg-[#1e293b] cursor-pointer"
					>
						Save
					</button>
				</div>
			</section>

			<!-- LETTERS -->
			<section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm space-y-3">
				<div class="flex items-start justify-between gap-2">
					<h2 class="text-base font-bold text-[#0b1c30]">Recommendation letters</h2>
					<span
						class="rounded-full px-2.5 py-1 text-[11px] font-semibold {store.hasLetter
							? 'bg-[#edf8f1] text-[#247047]'
							: 'bg-[#fff6e3] text-[#986b1a]'}"
					>
						{store.hasLetter ? 'Received' : store.myLetters.length ? 'Pending' : 'Not requested'}
					</span>
				</div>
				{#if store.myLetters.length > 0}
					<ul class="space-y-1.5 text-sm">
						{#each store.myLetters as letter (letter.id)}
							<li class="flex items-center justify-between gap-2">
								<span class="text-[#0b1c30] truncate">{letter.doctorName}</span>
								<span class="text-xs text-[#515f74] shrink-0">{letter.status}</span>
							</li>
						{/each}
					</ul>
				{:else}
					<p class="text-sm text-[#515f74]">You have not asked a faculty member yet.</p>
				{/if}
				<a
					href="/letters"
					class="w-full inline-flex items-center justify-center gap-2 rounded-xl bg-[#eff4ff] px-4 py-2.5 text-xs font-semibold text-[#0b1c30] hover:bg-[#dce9ff]"
				>
					<Icon name="letters" class="w-4 h-4" /> Manage requests
				</a>
			</section>

			<!-- COMPARE -->
			<section class="rounded-2xl border border-[#c6c6cd]/50 bg-[#eff4ff] p-5 space-y-3">
				<h2 class="text-base font-bold text-[#0b1c30]">Compare shortlist</h2>
				{#if discovery.compare.length > 0}
					<p class="text-sm text-[#515f74]">
						{discovery.compare.length} of 3 selected:
						{discovery.compare
							.map((record) => record.short_name.value ?? record.official_name.value ?? '')
							.join(', ')}
					</p>
					<a
						href="/compare"
						class="w-full inline-flex items-center justify-center gap-2 rounded-xl bg-[#0b1c30] text-white px-4 py-3 text-sm font-semibold hover:bg-[#233853]"
					>
						<Icon name="compare" class="w-4 h-4" /> Open comparison
					</a>
				{:else}
					<p class="text-sm text-[#515f74]">
						Add schools from Explore or your saved list to compare them side by side.
					</p>
					<a
						href="/saved"
						class="w-full inline-flex items-center justify-center gap-2 rounded-xl bg-white border border-[#c6c6cd]/50 px-4 py-3 text-sm font-semibold text-[#0b1c30] hover:bg-[#f8faff]"
					>
						<Icon name="saved" class="w-4 h-4" /> Go to saved list
					</a>
				{/if}
			</section>
		</aside>
	</div>
</section>
