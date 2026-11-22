<script lang="ts">
	// The student's academic record, entered once and reused for the eligibility
	// pre-check on the application. Self-reported for now; IPSO verifies it later.
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import PageHeader from '#lib/components/student/PageHeader.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import { isSafeNext } from '#lib/auth/roles.ts';
	import { auth } from '#lib/auth/store.svelte.ts';
	import { store } from '#lib/student/store.svelte.ts';
	import {
		evaluateEligibility,
		validateEligibilityProfile,
		type EligibilityErrors,
		type EligibilityProfile
	} from '#lib/student/eligibility.ts';
	import type { Level } from '#lib/student/types.ts';

	let level = $state<Level | ''>('UG');
	let major = $state('');
	let gpa = $state('');
	let credits = $state('');
	let majorDeclared = $state(false);
	let rhet110 = $state(false);
	let rhet120 = $state(false);
	let finalSemester = $state(false);
	let academicProbation = $state(false);
	let disciplinaryProbation = $state(false);
	let englishComplete = $state(false);
	let firstSemesterNoGpa = $state(false);

	let errors = $state<EligibilityErrors>({});
	let loaded = $state(false);
	let saving = $state(false);
	let hadProfile = $state(false);

	const requestedNext = page.url.searchParams.get('next');
	const next = $derived(
		isSafeNext(requestedNext) && !requestedNext.startsWith('/profile') ? requestedNext : '/apply'
	);

	function numberOrNull(value: string): number | null {
		const text = value.trim();
		if (text === '') return null;
		const parsed = Number(text);
		return Number.isFinite(parsed) ? parsed : null;
	}

	const draft = $derived<EligibilityProfile>({
		level,
		major,
		gpa: numberOrNull(gpa),
		credits: numberOrNull(credits),
		majorDeclared,
		rhet110,
		rhet120,
		finalSemester,
		academicProbation,
		disciplinaryProbation,
		englishComplete,
		firstSemesterNoGpa
	});

	const summary = $derived(evaluateEligibility(draft));
	const badge = $derived(
		summary.provisional
			? { label: 'Provisionally eligible', cls: 'bg-[#fffbeb] text-[#b45309]' }
			: summary.eligible
				? { label: 'Eligible to apply', cls: 'bg-[#f0fdf4] text-[#15803d]' }
				: { label: 'Not eligible yet', cls: 'bg-[#fef2f2] text-[#ba1a1a]' }
	);

	onMount(async () => {
		await auth.init();
		const accountId = auth.account?.homeAccountId ?? auth.email;
		if (!store.meLoaded) {
			await store.loadMe(accountId, auth.email, auth.displayName);
		}

		const saved = store.eligibilityProfile;
		if (saved) {
			hadProfile = true;
			level = saved.level;
			major = saved.major;
			gpa = saved.gpa === null ? '' : String(saved.gpa);
			credits = saved.credits === null ? '' : String(saved.credits);
			majorDeclared = saved.majorDeclared;
			rhet110 = saved.rhet110;
			rhet120 = saved.rhet120;
			finalSemester = saved.finalSemester;
			academicProbation = saved.academicProbation;
			disciplinaryProbation = saved.disciplinaryProbation;
			englishComplete = saved.englishComplete;
			firstSemesterNoGpa = saved.firstSemesterNoGpa;
		} else {
			level = auth.profile?.level ?? 'UG';
			major = auth.profile?.major ?? '';
		}
		loaded = true;
	});

	function chooseLevel(value: Level) {
		level = value;
		if (value === 'UG') firstSemesterNoGpa = false;
	}

	async function submit(event: SubmitEvent) {
		event.preventDefault();
		if (saving) return;
		const found = validateEligibilityProfile(draft);
		errors = found;
		if (Object.keys(found).length > 0) return;

		saving = true;
		const accountId = auth.account?.homeAccountId ?? auth.email;
		const ok = await store.saveEligibility(accountId, draft, auth.displayName);
		saving = false;
		if (!ok) return;

		store.showToast('Eligibility profile saved.');
		await goto(next, { replace: true });
	}

	const field =
		'w-full h-11 px-3.5 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/60 focus:outline-none focus:border-black focus:ring-0 shadow-sm';
	const labelCls = 'text-xs font-semibold text-[#515f74] uppercase tracking-wider';
	const fieldBox = 'flex flex-col gap-1.5 min-w-0';
	const checkRow =
		'flex items-start gap-3 rounded-xl border border-[#c6c6cd]/40 bg-[#f8f9ff] px-4 py-3 cursor-pointer hover:bg-[#eff4ff] transition-colors';
	const checkbox = 'mt-0.5 w-4 h-4 rounded border-[#c6c6cd] accent-[#131b2e] shrink-0';
</script>

<svelte:head><title>Eligibility profile · AUC IPSO Exchange</title></svelte:head>

<section class="space-y-5 md:space-y-6">
	<PageHeader
		crumbs={['My Profile']}
		title="Eligibility profile"
		subtitle="Tell IPSO about your academic record once. We use it to check whether you can apply, then pre-fill the application."
	/>

	{#if !loaded}
		<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-8 text-center">
			<div class="w-10 h-10 mx-auto rounded-full border-[3px] border-[#c6c6cd] border-t-[#131b2e] animate-spin"></div>
			<p class="mt-4 text-sm text-[#515f74]">Loading your profile…</p>
		</div>
	{:else}
		<div class="grid grid-cols-1 xl:grid-cols-[minmax(0,1fr)_360px] gap-5 items-start">
			<form onsubmit={submit} class="space-y-5 min-w-0">
				<!-- Level and major -->
				<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-5 sm:p-6 space-y-5">
					<div class="flex items-center gap-3">
						<span class="w-9 h-9 rounded-lg bg-[#eff4ff] border border-[#c6c6cd]/40 flex items-center justify-center text-[#0b1c30] shrink-0">
							<Icon name="profile" class="w-4 h-4" />
						</span>
						<div class="min-w-0">
							<h2 class="text-sm font-semibold text-[#0b1c30]">Academic level</h2>
							<p class="text-xs text-[#515f74]">The rules differ for undergraduate and graduate students.</p>
						</div>
					</div>

					<div class="flex flex-col gap-2">
						<span class={labelCls}>Level</span>
						<div class="grid grid-cols-2 gap-1 rounded-xl bg-[#eff4ff] border border-[#c6c6cd]/40 p-1">
							<button
								type="button"
								onclick={() => chooseLevel('UG')}
								aria-pressed={level === 'UG'}
								class="h-9 rounded-lg text-sm font-semibold transition-colors cursor-pointer {level === 'UG'
									? 'bg-white text-[#0b1c30] shadow-sm'
									: 'text-[#515f74] hover:text-[#0b1c30]'}"
							>
								Undergraduate
							</button>
							<button
								type="button"
								onclick={() => chooseLevel('GR')}
								aria-pressed={level === 'GR'}
								class="h-9 rounded-lg text-sm font-semibold transition-colors cursor-pointer {level === 'GR'
									? 'bg-white text-[#0b1c30] shadow-sm'
									: 'text-[#515f74] hover:text-[#0b1c30]'}"
							>
								Graduate
							</button>
						</div>
						{#if errors.level}<p class="text-xs font-medium text-[#ba1a1a]">{errors.level}</p>{/if}
					</div>

					<div class={fieldBox}>
						<label for="major" class={labelCls}>Major</label>
						<input
							id="major"
							class={field}
							bind:value={major}
							placeholder="Economics (ECON)"
							autocomplete="organization-title"
							aria-invalid={!!errors.major}
						/>
						{#if errors.major}<p class="text-xs font-medium text-[#ba1a1a]">{errors.major}</p>{/if}
					</div>

					<label class={checkRow}>
						<input type="checkbox" class={checkbox} bind:checked={majorDeclared} />
						<span class="min-w-0">
							<span class="block text-sm font-semibold text-[#0b1c30]">My major is officially declared</span>
							<span class="block text-xs text-[#515f74] leading-relaxed">Undeclared students cannot apply for exchange.</span>
						</span>
					</label>
				</div>

				<!-- GPA and credits -->
				<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-5 sm:p-6 space-y-5">
					<div class="flex items-center gap-3">
						<span class="w-9 h-9 rounded-lg bg-[#eff4ff] border border-[#c6c6cd]/40 flex items-center justify-center text-[#0b1c30] shrink-0">
							<Icon name="status" class="w-4 h-4" />
						</span>
						<div class="min-w-0">
							<h2 class="text-sm font-semibold text-[#0b1c30]">Academic record</h2>
							<p class="text-xs text-[#515f74]">Use your most recent AUC record.</p>
						</div>
					</div>

					<div class="grid grid-cols-1 sm:grid-cols-2 gap-4">
						<div class={fieldBox}>
							<label for="gpa" class={labelCls}>Cumulative GPA</label>
							<input
								id="gpa"
								class="{field} font-mono-num"
								inputmode="decimal"
								bind:value={gpa}
								placeholder="3.50"
								autocomplete="off"
								aria-invalid={!!errors.gpa}
								disabled={level === 'GR' && firstSemesterNoGpa}
							/>
							<p class="text-[11px] text-[#515f74]">On a 4.00 scale.</p>
							{#if errors.gpa}<p class="text-xs font-medium text-[#ba1a1a]">{errors.gpa}</p>{/if}
						</div>

						<div class={fieldBox}>
							<label for="credits" class={labelCls}>Completed credits</label>
							<input
								id="credits"
								class="{field} font-mono-num"
								inputmode="numeric"
								bind:value={credits}
								placeholder="64"
								autocomplete="off"
								aria-invalid={!!errors.credits}
							/>
							<p class="text-[11px] text-[#515f74]">
								{level === 'GR' ? 'Credits completed in your graduate program.' : 'At least 24 are required to apply.'}
							</p>
							{#if errors.credits}<p class="text-xs font-medium text-[#ba1a1a]">{errors.credits}</p>{/if}
						</div>
					</div>

					{#if level === 'GR'}
						<label class={checkRow}>
							<input
								type="checkbox"
								class={checkbox}
								checked={firstSemesterNoGpa}
								onchange={(event) => {
									firstSemesterNoGpa = event.currentTarget.checked;
									if (firstSemesterNoGpa) gpa = '';
								}}
							/>
							<span class="min-w-0">
								<span class="block text-sm font-semibold text-[#0b1c30]">First semester — no GPA yet</span>
								<span class="block text-xs text-[#515f74] leading-relaxed">
									Your application is considered provisionally until your first grades are final.
								</span>
							</span>
						</label>
					{/if}
				</div>

				<!-- Writing and English requirements -->
				<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-5 sm:p-6 space-y-5">
					<div class="flex items-center gap-3">
						<span class="w-9 h-9 rounded-lg bg-[#eff4ff] border border-[#c6c6cd]/40 flex items-center justify-center text-[#0b1c30] shrink-0">
							<Icon name="file" class="w-4 h-4" />
						</span>
						<div class="min-w-0">
							<h2 class="text-sm font-semibold text-[#0b1c30]">
								{level === 'GR' ? 'English requirement' : 'Core writing requirements'}
							</h2>
							<p class="text-xs text-[#515f74]">
								{level === 'GR'
									? 'All required English courses must be complete before you apply.'
									: 'Both freshman-level requirements must be complete.'}
							</p>
						</div>
					</div>

					{#if level === 'UG'}
						<label class={checkRow}>
							<input type="checkbox" class={checkbox} bind:checked={rhet110} />
							<span class="min-w-0">
								<span class="block text-sm font-semibold text-[#0b1c30]">RHET 110/1010 + CORE 110/1010</span>
								<span class="block text-xs text-[#515f74] leading-relaxed">Analytical and persuasive writing, plus the core requirement.</span>
							</span>
						</label>

						<label class={checkRow}>
							<input type="checkbox" class={checkbox} bind:checked={rhet120} />
							<span class="min-w-0">
								<span class="block text-sm font-semibold text-[#0b1c30]">RHET 120/1020</span>
								<span class="block text-xs text-[#515f74] leading-relaxed">Research writing.</span>
							</span>
						</label>
					{:else}
						<label class={checkRow}>
							<input type="checkbox" class={checkbox} bind:checked={englishComplete} />
							<span class="min-w-0">
								<span class="block text-sm font-semibold text-[#0b1c30]">Required English courses are complete</span>
								<span class="block text-xs text-[#515f74] leading-relaxed">Complete every English course your program requires.</span>
							</span>
						</label>

						<div class="rounded-xl bg-[#eff4ff] px-4 py-3 text-xs text-[#0b1c30] leading-relaxed">
							During the exchange you will enroll in three courses (nine credit hours), excluding thesis and
							capstone courses, to keep full-time status.
						</div>
					{/if}
				</div>

				<!-- Standing -->
				<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-5 sm:p-6 space-y-5">
					<div class="flex items-center gap-3">
						<span class="w-9 h-9 rounded-lg bg-[#eff4ff] border border-[#c6c6cd]/40 flex items-center justify-center text-[#0b1c30] shrink-0">
							<Icon name="alert" class="w-4 h-4" />
						</span>
						<div class="min-w-0">
							<h2 class="text-sm font-semibold text-[#0b1c30]">Standing and timing</h2>
							<p class="text-xs text-[#515f74]">These can make an otherwise qualified student ineligible.</p>
						</div>
					</div>

					{#if level === 'UG'}
						<label class="{checkRow} {finalSemester ? 'border-[#ba1a1a]/40 bg-[#fef2f2]' : ''}">
							<input type="checkbox" class={checkbox} bind:checked={finalSemester} />
							<span class="min-w-0">
								<span class="block text-sm font-semibold text-[#0b1c30]">This will be my final semester at AUC</span>
								<span class="block text-xs text-[#515f74] leading-relaxed">Students in their last semester are not eligible to apply.</span>
							</span>
						</label>
					{/if}

					<label class="{checkRow} {academicProbation ? 'border-[#ba1a1a]/40 bg-[#fef2f2]' : ''}">
						<input type="checkbox" class={checkbox} bind:checked={academicProbation} />
						<span class="min-w-0">
							<span class="block text-sm font-semibold text-[#0b1c30]">I am currently on academic probation</span>
							<span class="block text-xs text-[#515f74] leading-relaxed">Academic probation of any form makes you ineligible.</span>
						</span>
					</label>

					<label class="{checkRow} {disciplinaryProbation ? 'border-[#ba1a1a]/40 bg-[#fef2f2]' : ''}">
						<input type="checkbox" class={checkbox} bind:checked={disciplinaryProbation} />
						<span class="min-w-0">
							<span class="block text-sm font-semibold text-[#0b1c30]">I am currently on disciplinary probation</span>
							<span class="block text-xs text-[#515f74] leading-relaxed">
								Conduct or academic-integrity violations in the semester of application make you ineligible.
							</span>
						</span>
					</label>
				</div>

				<div class="flex flex-col sm:flex-row sm:items-center gap-3">
					<button
						type="submit"
						disabled={saving}
						class="h-11 px-5 rounded-xl bg-[#131b2e] hover:bg-[#1e293b] disabled:opacity-60 text-white text-sm font-semibold shadow-sm inline-flex items-center justify-center gap-2 cursor-pointer"
					>
						{saving ? 'Saving…' : hadProfile ? 'Save changes' : 'Save and continue'}
						<Icon name="arrow-right" class="w-4 h-4" />
					</button>
					{#if hadProfile}
						<a href={next} class="h-11 px-4 rounded-xl bg-white border border-[#c6c6cd]/50 text-[#0b1c30] text-sm font-semibold inline-flex items-center justify-center hover:bg-[#eff4ff] transition-colors">
							Back to application
						</a>
					{/if}
				</div>
			</form>

			<!-- Live eligibility summary -->
			<aside class="space-y-4 xl:sticky xl:top-24">
				<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-5 space-y-4">
					<div class="flex items-center justify-between gap-2">
						<h2 class="text-sm font-semibold text-[#0b1c30]">Eligibility check</h2>
						<span class="inline-flex items-center px-2.5 py-1 rounded-full text-[11px] font-semibold {badge.cls}">
							{badge.label}
						</span>
					</div>
					<p class="text-xs text-[#515f74] leading-relaxed">
						This updates as you fill the form. A provisional result is reviewed once grades are final.
					</p>

					<ul class="space-y-3">
						{#each summary.checks as check (check.label)}
							<li class="flex items-start gap-2.5">
								<span class="w-5 h-5 rounded-full flex items-center justify-center shrink-0 mt-0.5 {check.ok
									? 'bg-[#f0fdf4] text-[#15803d]'
									: 'bg-[#fef2f2] text-[#ba1a1a]'}">
									<Icon name={check.ok ? 'check' : 'x'} class="w-3.5 h-3.5" strokeWidth={2.6} />
								</span>
								<div class="min-w-0">
									<div class="flex items-center gap-1.5 flex-wrap">
										<span class="text-xs font-semibold text-[#0b1c30]">{check.label}</span>
										{#if check.provisional}
											<span class="px-1.5 py-0.5 rounded bg-[#fffbeb] text-[10px] font-semibold text-[#b45309]">Provisional</span>
										{/if}
									</div>
									<p class="text-[11px] text-[#515f74] leading-relaxed">{check.detail}</p>
								</div>
							</li>
						{/each}
					</ul>
				</div>

				<div class="rounded-2xl border border-[#c6c6cd]/40 bg-[#eff4ff] p-5 space-y-2">
					<div class="flex items-center gap-2 text-[#0b1c30]">
						<Icon name="lock" class="w-4 h-4 shrink-0" />
						<h2 class="text-sm font-semibold">Self-reported for now</h2>
					</div>
					<p class="text-xs text-[#515f74] leading-relaxed">
						Nothing is sent to the server yet. IPSO verifies your GPA, credits and standing before
						nomination. This page only pre-checks whether you can apply.
					</p>
				</div>
			</aside>
		</div>
	{/if}
</section>
