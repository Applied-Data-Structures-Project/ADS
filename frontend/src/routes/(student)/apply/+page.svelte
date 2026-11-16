<script lang="ts">
	// G3 (Malak): the application, in the 7 steps of IPSO's real Fall 2027 form.
	// Every "Save & continue" saves the draft (PUT /api/application). Submit is final:
	// after it the server refuses every edit and upload.
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import PageHeader from '#lib/components/student/PageHeader.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import { store, wordCount, universityById, acceptsLevel } from '#lib/student/store.svelte.ts';
	import { DOCUMENTS, RULES } from '#lib/student/mocks.ts';
	import type { DocType } from '#lib/student/types.ts';

	const STEPS = [
		{ title: 'Eligibility', hint: 'GPA, declared major, credits' },
		{ title: 'Personal information', hint: 'As on your passport' },
		{ title: 'Program & preferences', hint: '3 ranked universities' },
		{ title: 'Parent / guardian', hint: 'Emergency contact' },
		{ title: 'Questionnaire', hint: '7 questions' },
		{ title: 'Documents & letter', hint: '4 PDFs + 1 letter' },
		{ title: 'Review & submit', hint: 'Declaration' }
	];

	const startStep = Number(page.url.searchParams.get('step'));
	let step = $state(startStep >= 1 && startStep <= 7 ? startStep : 1);
	let uploadErrors = $state<Partial<Record<DocType, string>>>({});

	const a = $derived(store.application);
	const myLevel = $derived(store.user?.level ?? 'UG');
	const disabled = $derived(store.locked);
	const goalsWords = $derived(wordCount(a.qGoals));
	const adaptWords = $derived(wordCount(a.qAdaptability));
	const deadlineText = new Date(RULES.deadline).toLocaleString('en-GB', {
		day: 'numeric',
		month: 'long',
		hour: 'numeric',
		minute: '2-digit',
		timeZone: 'Africa/Cairo'
	});

	const reviewCards = $derived([
		{ n: 2, title: 'Personal', lines: [`${a.firstName} ${a.surname}`, `${a.nationality} · ${a.dateOfBirth}`, a.phone] },
		{
			n: 3,
			title: 'Program & preferences',
			lines: [a.programType || '—', ...a.pref.map((id, i) => `#${i + 1} ${universityById(id)?.name ?? '—'}`)]
		},
		{ n: 4, title: 'Guardian', lines: [`${a.guardianName} (${a.guardianRelation})`, a.guardianPhone, a.guardianEmail] },
		{
			n: 5,
			title: 'Questionnaire',
			lines: [`Goals: ${goalsWords} words`, `Adaptability: ${adaptWords} words`, `Travel: ${a.travelExperience || '—'}`]
		},
		{
			n: 6,
			title: 'Documents',
			lines: [
				`${DOCUMENTS.filter((d) => store.docs[d.type]).length} of ${DOCUMENTS.length} uploaded`,
				`Letter: ${store.hasLetter ? 'received' : 'not received yet'}`
			]
		}
	]);

	type TextKey =
		| 'firstName'
		| 'surname'
		| 'dateOfBirth'
		| 'nationality'
		| 'secondNationality'
		| 'phone'
		| 'expectedGraduation'
		| 'guardianName'
		| 'guardianRelation'
		| 'guardianPhone'
		| 'guardianEmail'
		| 'guardianLanguage';

	const PERSONAL: { key: TextKey; label: string; placeholder?: string; optional?: boolean }[] = [
		{ key: 'firstName', label: 'First name (as per passport)' },
		{ key: 'surname', label: 'Surname (as per passport)' },
		{ key: 'dateOfBirth', label: 'Date of birth', placeholder: 'MM/DD/YYYY' },
		{ key: 'nationality', label: 'Nationality' },
		{ key: 'secondNationality', label: 'Second nationality', optional: true },
		{ key: 'phone', label: 'Phone (with country code)', placeholder: '+20 …' },
		{ key: 'expectedGraduation', label: 'Expected graduation', placeholder: 'MM/YYYY' }
	];

	const GUARDIAN: { key: TextKey; label: string; placeholder?: string; optional?: boolean }[] = [
		{ key: 'guardianName', label: 'Full name' },
		{ key: 'guardianRelation', label: 'Relation to student', placeholder: 'e.g. Mother' },
		{ key: 'guardianPhone', label: 'Phone (with country code)', placeholder: '+20 …' },
		{ key: 'guardianEmail', label: 'Email' },
		{ key: 'guardianLanguage', label: 'Preferred language for communication', optional: true }
	];

	type EssayKey = 'qPreparedness' | 'qGoals' | 'qAdaptability' | 'qTeamwork' | 'qReflection' | 'qFinance';

	const ESSAYS: { key: EssayKey; label: string; help: string; min?: number; max?: number }[] = [
		{
			key: 'qPreparedness',
			label: 'Academic preparedness',
			help: 'How are you, and how will you, academically prepare for this experience?'
		},
		{
			key: 'qGoals',
			label: 'How does the program align with your academic goals?',
			help: 'How will it contribute to your studies, research interests or career goals?',
			min: RULES.goalsWords.min,
			max: RULES.goalsWords.max
		},
		{
			key: 'qAdaptability',
			label: 'Adapting to new environments',
			help: 'How do you adapt to unfamiliar cultures? What will you do about language or cultural barriers?',
			min: RULES.adaptabilityWords.min,
			max: RULES.adaptabilityWords.max
		},
		{
			key: 'qTeamwork',
			label: 'Teamwork',
			help: 'Describe a time you worked well in a team. What was your role, and how would you handle conflict?'
		},
		{
			key: 'qReflection',
			label: 'Self-assessment',
			help: 'What do you hope to gain personally and professionally, and what would you like to improve?'
		},
		{
			key: 'qFinance',
			label: 'Financial plan',
			help: 'How do you plan to finance and budget for your semester abroad?'
		}
	];

	const TRAVEL = ['Yes, Study Abroad', 'Yes, Non-Study Abroad', 'No', 'Other'] as const;

	const PROGRAMS = [
		{ v: 'Exchange', d: 'Pay AUC tuition in one payment (no installments while on exchange).' },
		{ v: 'Direct Enrollment', d: 'Enroll and pay the host university directly.' }
	] as const;

	function go(n: number) {
		step = Math.min(7, Math.max(1, n));
		if (typeof window !== 'undefined') window.scrollTo({ top: 0, behavior: 'smooth' });
	}

	function saveAndContinue() {
		store.saveDraft(true);
		store.showToast('Draft saved.');
		go(step + 1);
	}

	function pickFile(type: DocType, e: Event) {
		const input = e.currentTarget as HTMLInputElement;
		const file = input.files?.[0];
		if (!file) return;
		const error = store.upload(type, file);
		uploadErrors = { ...uploadErrors, [type]: error ?? undefined };
		input.value = '';
	}

	function submit() {
		if (store.submit()) goto('/apply/status');
	}

	function counterClass(words: number, min?: number, max?: number) {
		if (min === undefined) return 'text-[#515f74]';
		const ok = words >= min && (max === undefined || words <= max);
		return ok ? 'text-[#15803d]' : 'text-[#ba1a1a]';
	}

	const field =
		'w-full h-11 px-3.5 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm disabled:bg-[#eff4ff] disabled:text-[#515f74]';
	const labelCls = 'text-xs font-semibold text-[#515f74] uppercase tracking-wider';
	const fieldBox = 'flex flex-col gap-1.5 min-w-0';
</script>

<svelte:head><title>Application · AUC Exchange</title></svelte:head>

<section class="space-y-5 md:space-y-6">
	<PageHeader
		crumbs={['My Application', 'Fall 2027 Study Abroad Application']}
		title="Fall 2027 Study Abroad Application"
		subtitle={`Exchange and direct enrollment. Closes ${deadlineText} (Cairo time). Your draft saves at every step.`}
	>
		{#snippet actions()}
			<span class="px-3.5 py-2 bg-[#eff4ff] border border-[#c6c6cd]/40 rounded-full text-xs sm:text-[13px] font-mono-num text-[#0b1c30] shadow-sm">
				{a.status} · {store.progress}% complete
			</span>
		{/snippet}
	</PageHeader>

	{#if disabled}
		<div class="rounded-xl px-4 sm:px-5 py-3.5 bg-[#f0fdf4] border border-[#c6c6cd]/40 flex flex-wrap items-center gap-2.5 text-sm">
			<span class="text-[#15803d]"><Icon name="lock" class="w-5 h-5" /></span>
			<span class="font-semibold text-[#0b1c30]">Submitted. Your application can no longer be edited.</span>
			<a href="/apply/status" class="font-semibold text-[#003ea8] hover:underline">Track its status →</a>
		</div>
	{/if}

	<!-- Mobile stepper -->
	<div class="lg:hidden flex items-center gap-1.5 overflow-x-auto no-scrollbar bg-[#d3e4fe]/40 p-1 rounded-xl">
		{#each STEPS as s, i (s.title)}
			<button
				type="button"
				onclick={() => go(i + 1)}
				class="h-9 px-3 text-xs rounded-lg whitespace-nowrap shrink-0 cursor-pointer inline-flex items-center gap-1.5 {step === i + 1
					? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
					: 'text-[#515f74] font-medium'}"
			>
				<span class="font-mono-num {store.stepsDone[i] ? 'text-[#15803d]' : ''}">{store.stepsDone[i] ? '✓' : i + 1}</span>
				{s.title}
			</button>
		{/each}
	</div>

	<div class="grid grid-cols-1 lg:grid-cols-[280px_minmax(0,1fr)] gap-4 lg:gap-5 items-start">
		<!-- Desktop stepper + what's missing -->
		<aside class="hidden lg:flex flex-col gap-4 sticky top-[96px]">
			<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 p-3">
				<div class="px-2 pt-1 pb-3">
					<div class="flex justify-between text-xs font-semibold text-[#515f74] uppercase tracking-wider">
						<span>Progress</span><span class="font-mono-num text-[#0b1c30]">{store.progress}%</span>
					</div>
					<div class="mt-2 h-2 bg-[#eff4ff] rounded-full overflow-hidden">
						<div class="h-full bg-[#131b2e] rounded-full transition-all" style="width: {store.progress}%"></div>
					</div>
				</div>
				<nav class="flex flex-col gap-1">
					{#each STEPS as s, i (s.title)}
						{@const active = step === i + 1}
						<button
							type="button"
							onclick={() => go(i + 1)}
							class="w-full flex items-center gap-3 px-2.5 py-2.5 rounded-xl text-left cursor-pointer transition-colors {active
								? 'bg-[#dce9ff]/80 border border-[#c6c6cd]/50 shadow-sm'
								: 'hover:bg-[#eff4ff]'}"
						>
							<span
								class="w-8 h-8 rounded-lg flex items-center justify-center shrink-0 text-xs font-bold font-mono-num {store.stepsDone[i]
									? 'bg-[#f0fdf4] text-[#15803d]'
									: active
										? 'bg-[#131b2e] text-white'
										: 'bg-[#eff4ff] text-[#0b1c30]'}"
							>
								{#if store.stepsDone[i]}<Icon name="check" class="w-4 h-4" strokeWidth={2.5} />{:else}{i + 1}{/if}
							</span>
							<span class="min-w-0">
								<span class="block text-sm font-semibold text-[#0b1c30] truncate">{s.title}</span>
								<span class="block text-xs text-[#515f74] truncate">{s.hint}</span>
							</span>
						</button>
					{/each}
				</nav>
			</div>

			<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 p-4 space-y-2.5">
				<div class="text-xs font-semibold uppercase tracking-wider {store.missing.length ? 'text-[#ba1a1a]' : 'text-[#15803d]'}">
					{store.missing.length ? `Still missing (${store.missing.length})` : 'Ready to submit'}
				</div>
				{#each store.missing.slice(0, 6) as m (m.label)}
					<button type="button" onclick={() => go(m.step)} class="w-full text-left text-xs text-[#0b1c30] hover:underline cursor-pointer flex gap-2">
						<span class="font-mono-num text-[#515f74] shrink-0">{m.step}</span>{m.label}
					</button>
				{/each}
				{#if store.missing.length > 6}
					<button type="button" onclick={() => go(7)} class="text-xs font-semibold text-[#003ea8] hover:underline cursor-pointer">
						+{store.missing.length - 6} more on the review step
					</button>
				{/if}
			</div>
		</aside>

		<!-- Step content -->
		<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden min-w-0">
			<div class="px-4 sm:px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex items-center justify-between gap-3">
				<div class="min-w-0">
					<div class="text-xs font-semibold text-[#515f74] uppercase tracking-wider">Step {step} of 7</div>
					<h2 class="text-base sm:text-lg font-semibold text-[#0b1c30] truncate">{STEPS[step - 1].title}</h2>
				</div>
				{#if store.stepsDone[step - 1]}
					<span class="px-2.5 py-1 rounded-full text-xs font-semibold bg-[#f0fdf4] text-[#15803d] shrink-0">Complete</span>
				{/if}
			</div>

			<fieldset class="p-4 sm:p-5 space-y-5" disabled={disabled && step !== 7}>
				<!-- STEP 1: ELIGIBILITY -->
				{#if step === 1}
					<p class="text-sm text-[#515f74]">IPSO only considers applications that meet all three rules. They come from your profile.</p>
					<div class="grid grid-cols-1 sm:grid-cols-3 gap-3">
						{#each store.eligibility as c (c.label)}
							<div class="p-4 rounded-xl border border-[#c6c6cd]/40 {c.ok ? 'bg-[#f0fdf4]' : 'bg-[#fef2f2]'}">
								<div class="flex items-center gap-2 {c.ok ? 'text-[#15803d]' : 'text-[#ba1a1a]'}">
									<Icon name={c.ok ? 'check' : 'x'} class="w-5 h-5" strokeWidth={2.4} />
									<span class="text-sm font-semibold">{c.ok ? 'Meets the rule' : 'Does not meet the rule'}</span>
								</div>
								<div class="mt-2 text-sm font-semibold text-[#0b1c30]">{c.label}</div>
								<div class="text-xs text-[#515f74]">{c.detail}</div>
							</div>
						{/each}
					</div>
					<div class="rounded-xl bg-[#eff4ff] px-4 py-3 text-xs sm:text-sm text-[#0b1c30] space-y-1">
						<div><span class="font-semibold">Good standing:</span> IPSO checks academic integrity and conduct before assigning and before nominating you.</div>
						<div><span class="font-semibold">Host GPA:</span> you must also meet your host university's own GPA requirement.</div>
					</div>
					{#if !store.isEligible}
						<a href="/profile" class="inline-flex h-10 px-4 bg-[#131b2e] hover:bg-[#1e293b] text-white text-sm font-semibold rounded-xl items-center gap-2">
							Update my profile <Icon name="arrow-right" class="w-4 h-4" />
						</a>
					{/if}

				<!-- STEP 2: PERSONAL -->
				{:else if step === 2}
					<div class="grid grid-cols-1 sm:grid-cols-2 gap-4">
						{#each PERSONAL as f (f.key)}
							<div class={fieldBox}>
								<label for={`ap-${f.key}`} class={labelCls}>{f.label}{f.optional ? '' : ' *'}</label>
								<input id={`ap-${f.key}`} class={field} bind:value={store.application[f.key]} placeholder={f.placeholder ?? ''} />
							</div>
						{/each}
						<div class={fieldBox}>
							<label for="ap-gender" class={labelCls}>Gender *</label>
							<select id="ap-gender" class="stitch-select {field}" bind:value={store.application.gender}>
								<option value="">Select…</option>
								<option value="Male">Male</option>
								<option value="Female">Female</option>
							</select>
						</div>
						<div class={fieldBox}>
							<label for="ap-rhet" class={labelCls}>Completed RHET 1020? *</label>
							<select id="ap-rhet" class="stitch-select {field}" bind:value={store.application.rhet1020}>
								<option value="">Select…</option>
								<option value="Yes">Yes</option>
								<option value="Currently Enrolled">Currently enrolled</option>
								<option value="Not Applicable">Not applicable</option>
							</select>
						</div>
						<div class={fieldBox}>
							<label for="ap-scholarship" class={labelCls}>AUC scholarship *</label>
							<select id="ap-scholarship" class="stitch-select {field}" bind:value={store.application.scholarship}>
								<option value="">Select…</option>
								<option value="None">No scholarship</option>
								<option value="Tomorrow's Leaders">Tomorrow's Leaders Undergraduate Program</option>
								<option value="Other">Other</option>
							</select>
						</div>
						<div class={fieldBox}>
							<span class={labelCls}>Accommodation support needed? *</span>
							<div class="flex gap-2">
								{#each [true, false] as v (String(v))}
									<label
										class="flex-1 h-11 px-3.5 rounded-xl border border-[#c6c6cd]/40 flex items-center gap-2 text-sm cursor-pointer {store.application.accommodation === v
											? 'bg-[#dce9ff]/80'
											: 'bg-white'}"
									>
										<input type="radio" name="accommodation" checked={store.application.accommodation === v} onchange={() => (store.application.accommodation = v)} class="accent-[#131b2e]" />
										{v ? 'Yes' : 'No'}
									</label>
								{/each}
							</div>
						</div>
					</div>
					<div class="rounded-xl bg-[#eff4ff] px-4 py-3 grid grid-cols-2 sm:grid-cols-4 gap-3 text-sm">
						<div><div class="text-xs text-[#515f74]">AUC ID</div><div class="font-mono-num font-semibold">{store.user?.aucId}</div></div>
						<div><div class="text-xs text-[#515f74]">GPA</div><div class="font-mono-num font-semibold">{store.user?.gpa.toFixed(2)}</div></div>
						<div><div class="text-xs text-[#515f74]">Status</div><div class="font-semibold">{myLevel === 'UG' ? 'Undergraduate' : 'Graduate'}</div></div>
						<div class="min-w-0"><div class="text-xs text-[#515f74]">Major</div><div class="font-semibold truncate">{store.user?.major}</div></div>
						<div class="col-span-2 sm:col-span-4 text-xs text-[#515f74]">From your profile. <a href="/profile" class="font-semibold text-[#003ea8] hover:underline">Change it there.</a></div>
					</div>

				<!-- STEP 3: PROGRAM + PREFERENCES -->
				{:else if step === 3}
					<div class="space-y-2">
						<span class={labelCls}>Type of study abroad *</span>
						<div class="grid grid-cols-1 sm:grid-cols-2 gap-3">
							{#each PROGRAMS as opt (opt.v)}
								<label
									class="p-4 rounded-xl border cursor-pointer flex gap-3 {store.application.programType === opt.v
										? 'border-[#131b2e] bg-[#dce9ff]/80'
										: 'border-[#c6c6cd]/40 bg-white hover:bg-[#eff4ff]'}"
								>
									<input type="radio" name="program" checked={store.application.programType === opt.v} onchange={() => (store.application.programType = opt.v)} class="mt-1 accent-[#131b2e]" />
									<span><span class="block text-sm font-semibold text-[#0b1c30]">{opt.v}</span><span class="block text-xs text-[#515f74]">{opt.d}</span></span>
								</label>
							{/each}
						</div>
					</div>

					<div class="space-y-3">
						<div class="flex items-end justify-between gap-3 flex-wrap">
							<span class={labelCls}>Your 3 preferences, in order *</span>
							<a href="/saved" class="text-xs font-semibold text-[#003ea8] hover:underline">Manage saved list ({store.savedIds.length})</a>
						</div>
						{#if store.saved.length < 3}
							<div class="rounded-xl bg-[#fffbeb] px-4 py-3 text-sm text-[#b45309] font-medium">
								You have {store.saved.length} saved. Save at least 3 universities on <a href="/explore" class="underline">Explore</a> first.
							</div>
						{/if}
						{#each [0, 1, 2] as slot (slot)}
							{@const err = store.prefErrors[slot]}
							{@const chosen = universityById(store.application.pref[slot])}
							<div class="grid grid-cols-[44px_minmax(0,1fr)] gap-3 items-start">
								<span class="h-11 rounded-xl flex items-center justify-center font-mono-num font-bold text-sm {slot === 0 ? 'bg-[#131b2e] text-white' : 'bg-[#e5eeff] text-[#0b1c30]'}">
									#{slot + 1}
								</span>
								<div class="space-y-1 min-w-0">
									<select
										id={`pref-${slot}`}
										aria-label={`Preference ${slot + 1}`}
										class="stitch-select {field} {err && store.application.pref[slot] >= 0 ? 'border-[#ba1a1a]' : ''}"
										value={store.application.pref[slot]}
										onchange={(e) => store.setPref(slot, Number((e.currentTarget as HTMLSelectElement).value))}
									>
										<option value={-1}>Choose from your saved list…</option>
										{#each store.saved as u (u.id)}
											<option value={u.id}>{u.name} · {u.country}{acceptsLevel(u, myLevel) ? '' : ` (not open to ${myLevel})`}</option>
										{/each}
									</select>
									{#if err && store.application.pref[slot] >= 0}
										<div class="text-xs font-semibold text-[#ba1a1a]">{err}</div>
									{:else if chosen}
										<div class="text-xs text-[#515f74]">{chosen.city} · {chosen.terms || 'Terms TBC'} · {chosen.scope}</div>
									{/if}
								</div>
							</div>
						{/each}
						<p class="text-xs text-[#515f74]">
							IPSO will do its best to place you in one of your choices, but seats are limited. Placement isn't guaranteed.
						</p>
					</div>

				<!-- STEP 4: GUARDIAN -->
				{:else if step === 4}
					<p class="text-sm text-[#515f74]">IPSO contacts this person only if needed while you are abroad.</p>
					<div class="grid grid-cols-1 sm:grid-cols-2 gap-4">
						{#each GUARDIAN as f (f.key)}
							<div class={fieldBox}>
								<label for={`ap-${f.key}`} class={labelCls}>{f.label}{f.optional ? '' : ' *'}</label>
								<input id={`ap-${f.key}`} class={field} bind:value={store.application[f.key]} placeholder={f.placeholder ?? ''} />
							</div>
						{/each}
					</div>

				<!-- STEP 5: QUESTIONNAIRE -->
				{:else if step === 5}
					<p class="text-sm text-[#515f74]">Answer as accurately as possible. The selection committee reads these.</p>
					{#each ESSAYS as q (q.key)}
						{@const words = wordCount(store.application[q.key])}
						<div class="space-y-1.5">
							<label for={`ap-${q.key}`} class="block text-sm font-semibold text-[#0b1c30]">{q.label} *</label>
							<p class="text-xs text-[#515f74]">{q.help}</p>
							<textarea
								id={`ap-${q.key}`}
								rows={q.min ? 8 : 4}
								bind:value={store.application[q.key]}
								class="w-full px-3.5 py-3 bg-white text-[#0b1c30] text-sm leading-relaxed rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm disabled:bg-[#eff4ff]"
							></textarea>
							<div class="flex justify-between text-xs">
								<span class="font-mono-num font-semibold {counterClass(words, q.min, q.max)}">
									{words} words{q.min ? ` · needs ${q.min}${q.max && q.max < 1000 ? `–${q.max}` : '+'}` : ''}
								</span>
								{#if q.min && words < q.min}<span class="text-[#515f74]">{q.min - words} to go</span>{/if}
							</div>
						</div>
						{#if q.key === 'qGoals'}
							<div class="space-y-2">
								<span class="block text-sm font-semibold text-[#0b1c30]">Previous international travel experience? *</span>
								<div class="grid grid-cols-2 sm:grid-cols-4 gap-2">
									{#each TRAVEL as t (t)}
										<label
											class="px-3 py-2.5 rounded-xl border text-xs sm:text-sm cursor-pointer flex items-center gap-2 {store.application.travelExperience === t
												? 'border-[#131b2e] bg-[#dce9ff]/80 font-semibold'
												: 'border-[#c6c6cd]/40 bg-white'}"
										>
											<input type="radio" name="travel" checked={store.application.travelExperience === t} onchange={() => (store.application.travelExperience = t)} class="accent-[#131b2e]" />
											{t}
										</label>
									{/each}
								</div>
							</div>
						{/if}
					{/each}
					<div class={fieldBox}>
						<label for="ap-budget" class={labelCls}>Estimated budget per semester (USD)</label>
						<input id="ap-budget" type="number" min="0" step="100" class="{field} font-mono-num sm:max-w-xs" bind:value={store.application.budget} />
						<span class="text-xs text-[#515f74]">Used by IPSO when matching you to universities you can afford.</span>
					</div>

				<!-- STEP 6: DOCUMENTS + LETTER -->
				{:else if step === 6}
					<p class="text-sm text-[#515f74]">Upload each document as its own PDF (max {RULES.maxFileMb} MB). The file is renamed to “Name - ID” for you.</p>
					<div class="grid grid-cols-1 md:grid-cols-2 gap-3">
						{#each DOCUMENTS as d (d.type)}
							{@const doc = store.docs[d.type]}
							<div class="p-4 rounded-xl border border-[#c6c6cd]/40 flex flex-col gap-3 {doc ? 'bg-white' : 'bg-[#eff4ff]'}">
								<div class="flex items-start gap-3">
									<span class="w-9 h-9 rounded-lg flex items-center justify-center shrink-0 {doc ? 'bg-[#f0fdf4] text-[#15803d]' : 'bg-white text-[#515f74]'}">
										<Icon name={doc ? 'check' : 'file'} class="w-4 h-4" strokeWidth={doc ? 2.4 : 1.75} />
									</span>
									<div class="min-w-0">
										<div class="text-sm font-semibold text-[#0b1c30]">{d.label}</div>
										<div class="text-xs text-[#515f74]">{d.hint}</div>
									</div>
								</div>
								{#if doc}
									<div class="text-xs text-[#515f74] font-mono-num truncate">{doc.fileName} · {doc.sizeKb} KB · {doc.uploadedAt}</div>
								{/if}
								{#if uploadErrors[d.type]}
									<div class="text-xs font-semibold text-[#ba1a1a]" role="alert">{uploadErrors[d.type]}</div>
								{/if}
								{#if !disabled}
									<div class="flex items-center gap-2 mt-auto">
										<label
											class="h-9 px-3.5 rounded-lg text-xs font-semibold inline-flex items-center gap-1.5 cursor-pointer {doc
												? 'bg-[#eff4ff] text-[#0b1c30] hover:bg-[#dce9ff]'
												: 'bg-[#131b2e] text-white hover:bg-[#1e293b]'}"
										>
											<Icon name="upload" class="w-3.5 h-3.5" />
											{doc ? 'Replace' : 'Upload PDF'}
											<input type="file" accept="application/pdf,.pdf" class="sr-only" onchange={(e) => pickFile(d.type, e)} />
										</label>
										{#if doc}
											<button type="button" onclick={() => store.removeDoc(d.type)} class="h-9 px-3 rounded-lg text-xs font-semibold text-[#515f74] hover:text-[#0b1c30] cursor-pointer">
												Remove
											</button>
										{/if}
										{#if d.template}
											<span class="ml-auto text-xs text-[#515f74]">Template: {d.template}</span>
										{/if}
									</div>
								{/if}
							</div>
						{/each}

						<div class="md:col-span-2 p-4 rounded-xl border border-[#c6c6cd]/40 {store.hasLetter ? 'bg-white' : 'bg-[#fffbeb]'} flex flex-col sm:flex-row sm:items-center gap-3">
							<span class="w-9 h-9 rounded-lg flex items-center justify-center shrink-0 {store.hasLetter ? 'bg-[#f0fdf4] text-[#15803d]' : 'bg-white text-[#b45309]'}">
								<Icon name={store.hasLetter ? 'check' : 'letters'} class="w-4 h-4" />
							</span>
							<div class="flex-1 min-w-0">
								<div class="text-sm font-semibold text-[#0b1c30]">Recommendation Letter from an AUC Faculty Member</div>
								<div class="text-xs text-[#515f74]">
									{#if store.myLetters.length === 0}
										Not requested yet. The faculty member writes it in the app (or emails studyabroad@aucegypt.edu).
									{:else}
										{store.myLetters.map((l) => `${l.doctorName}: ${l.status}`).join(' · ')}
									{/if}
								</div>
							</div>
							<a href="/letters" class="h-9 px-3.5 rounded-lg text-xs font-semibold bg-[#131b2e] text-white hover:bg-[#1e293b] inline-flex items-center gap-1.5 shrink-0">
								{store.myLetters.length === 0 ? 'Request a letter' : 'Manage requests'}
							</a>
						</div>
					</div>

				<!-- STEP 7: REVIEW -->
				{:else}
					<div class="grid grid-cols-1 md:grid-cols-2 gap-3">
						{#each reviewCards as card (card.n)}
							<div class="p-4 rounded-xl border border-[#c6c6cd]/40 bg-white">
								<div class="flex items-center justify-between gap-2">
									<span class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">{card.title}</span>
									{#if !disabled}
										<button type="button" onclick={() => go(card.n)} class="text-xs font-semibold text-[#003ea8] hover:underline cursor-pointer">Edit</button>
									{/if}
								</div>
								{#each card.lines as line, li (li)}
									<div class="text-sm text-[#0b1c30] truncate">{line}</div>
								{/each}
							</div>
						{/each}
					</div>

					{#if store.missing.length > 0}
						<div class="rounded-xl bg-[#fef2f2] px-4 py-3 space-y-1.5">
							<div class="text-sm font-semibold text-[#ba1a1a]">{store.missing.length} things to finish before you can submit</div>
							{#each store.missing as m (m.label)}
								<button type="button" onclick={() => go(m.step)} class="block text-left text-xs sm:text-sm text-[#0b1c30] hover:underline cursor-pointer">
									Step {m.step}: {m.label}
								</button>
							{/each}
						</div>
					{/if}

					<label class="flex items-start gap-3 p-4 rounded-xl border border-[#c6c6cd]/40 cursor-pointer {store.application.declaration ? 'bg-[#dce9ff]/80' : 'bg-white'}">
						<input type="checkbox" bind:checked={store.application.declaration} disabled={disabled} class="mt-0.5 rounded w-4 h-4 accent-[#131b2e]" />
						<span class="text-sm text-[#0b1c30]">
							I certify that all the above information is correct and I will submit the required documents by the deadline.
							I approve sharing all submitted documents with the selection panel.
						</span>
					</label>
				{/if}
			</fieldset>

			<!-- Footer actions -->
			<div class="px-4 sm:px-5 py-4 border-t border-[#c6c6cd]/30 bg-[#eff4ff] flex items-center justify-between gap-3 flex-wrap">
				<button
					type="button"
					onclick={() => go(step - 1)}
					disabled={step === 1}
					class="h-10 px-4 bg-white border border-[#c6c6cd]/40 rounded-xl text-xs sm:text-sm font-semibold shadow-sm inline-flex items-center gap-2 cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
				>
					<Icon name="arrow-left" class="w-4 h-4" /> Back
				</button>
				{#if step < 7}
					<button
						type="button"
						onclick={disabled ? () => go(step + 1) : saveAndContinue}
						class="h-10 px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white rounded-xl text-xs sm:text-sm font-semibold shadow-sm inline-flex items-center gap-2 cursor-pointer"
					>
						{disabled ? 'Next' : 'Save & continue'} <Icon name="arrow-right" class="w-4 h-4" />
					</button>
				{:else if !disabled}
					<div class="flex items-center gap-2.5">
						<button type="button" onclick={() => store.saveDraft()} class="h-10 px-4 bg-white border border-[#c6c6cd]/40 rounded-xl text-xs sm:text-sm font-semibold shadow-sm cursor-pointer">
							Save draft
						</button>
						<button
							type="button"
							onclick={submit}
							disabled={store.missing.length > 0}
							class="h-10 px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white rounded-xl text-xs sm:text-sm font-semibold shadow-sm inline-flex items-center gap-2 cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
						>
							<Icon name="lock" class="w-4 h-4" /> Submit application
						</button>
					</div>
				{/if}
			</div>
		</div>
	</div>
</section>
