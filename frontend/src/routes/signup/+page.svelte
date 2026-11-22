<script lang="ts">
	// First sign-in: Microsoft supplies the name and email, this page supplies
	// the academic details the exchange application needs.
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';
	import Icon from '#lib/components/student/Icon.svelte';
	import MajorSelect from '#lib/components/student/MajorSelect.svelte';
	import { roleHome } from '#lib/auth/roles.ts';
	import { auth } from '#lib/auth/store.svelte.ts';
	import { validateProfile, type ProfileErrors, type StudentProfileInput } from '#lib/auth/profile.ts';
	import { isValidAucMajor } from '#lib/student/auc-majors.ts';
	import { callServer } from '#lib/student/api.ts';

	let ready = $state(false);
	let saving = $state(false);
	let fullName = $state('');
	let nameError = $state('');
	let serverError = $state('');
	let level = $state<'UG' | 'GR' | ''>('UG');
	let major = $state('');
	let aucId = $state('');
	let phone = $state('');
	let errors = $state<ProfileErrors>({});

	function chooseLevel(nextLevel: 'UG' | 'GR') {
		level = nextLevel;
		if (major && !isValidAucMajor(major, nextLevel)) {
			major = '';
		}
	}

	const firstName = $derived(
		(fullName.trim() || auth.displayName).split(/\s+/)[0] || 'there'
	);

	onMount(async () => {
		await auth.init();
		if (!auth.signedIn) {
			await goto(`/login?next=${encodeURIComponent('/signup')}`, { replace: true });
			return;
		}
		if (auth.role !== 'student') {
			await goto(roleHome(auth.role), { replace: true });
			return;
		}
		try {
			const me = await callServer<{
				profile: {
					name?: string;
					level?: string;
					major?: string;
					aucId?: string;
					phone?: string;
				} | null;
			}>('GET', '/api/me');
			auth.syncFromServerProfile(me.profile);
		} catch {
			// Offline: continue with local state.
		}
		if (auth.profile) {
			fullName = auth.profile.name ?? (auth.demoEnabled ? '' : auth.displayName);
			level = auth.profile.level;
			major = auth.profile.major;
			aucId = auth.profile.aucId;
			phone = auth.profile.phone;
		} else {
			fullName = auth.demoEnabled ? '' : auth.displayName;
		}
		if (!auth.needsProfile) {
			await goto('/apply', { replace: true });
			return;
		}
		ready = true;
	});

	async function submit(event: SubmitEvent) {
		event.preventDefault();
		serverError = '';
		const trimmedName = fullName.trim();
		nameError = trimmedName.length < 2 ? 'Enter your full name.' : '';

		const input: StudentProfileInput = { name: trimmedName, level, major, aucId, phone };
		const found = validateProfile(input);
		errors = found;
		if (nameError || Object.keys(found).length > 0) return;

		saving = true;
		let eligibilitySaved = false;
		try {
			const res = await callServer<{
				profile: { eligibilitySaved?: boolean } | null;
			}>('PUT', '/api/me', {
				name: trimmedName,
				level: input.level,
				major: input.major.trim(),
				aucId: input.aucId.trim(),
				phone: input.phone.trim()
			});
			eligibilitySaved = Boolean(res.profile?.eligibilitySaved);
		} catch (e) {
			serverError = e instanceof Error ? e.message : 'Could not save your profile to the server.';
			saving = false;
			return;
		}

		const saved = auth.saveProfile(input);
		errors = saved;
		if (Object.keys(saved).length > 0) {
			saving = false;
			return;
		}
		await goto(eligibilitySaved ? '/apply' : '/profile', { replace: true });
	}

	const field =
		'w-full h-11 px-3.5 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/60 focus:outline-none focus:border-black focus:ring-0 shadow-sm';
	const labelCls = 'text-xs font-semibold text-[#515f74] uppercase tracking-wider';
</script>

<svelte:head>
	<title>Finish your profile · AUC IPSO Exchange</title>
</svelte:head>

<main class="min-h-screen bg-[#f8f9ff] text-[#0b1c30]">
	<header class="flex items-center justify-between gap-3 px-5 sm:px-8 py-5 max-w-5xl mx-auto">
		<div class="flex items-center gap-3">
			<img src="/auc-logo.png" alt="The American University in Cairo" class="h-7 w-auto" />
			<span class="hidden sm:inline text-sm font-semibold text-[#0b1c30]">IPSO Exchange</span>
		</div>
		<div class="flex items-center gap-2 sm:gap-3 min-w-0">
			{#if !auth.demoEnabled}
				<span class="hidden sm:inline text-xs font-medium text-[#515f74] font-mono-num truncate max-w-[220px]">
					{auth.email}
				</span>
			{/if}
			<button
				type="button"
				onclick={() => auth.signOut()}
				class="h-9 px-3 rounded-xl bg-white hover:bg-[#eff4ff] border border-[#c6c6cd]/50 text-[#0b1c30] text-xs font-semibold inline-flex items-center gap-2 cursor-pointer shrink-0"
			>
				<Icon name="logout" class="w-4 h-4" />
				Sign out
			</button>
		</div>
	</header>

	<div class="px-5 sm:px-8 pb-24">
		<div class="mx-auto w-full max-w-xl space-y-5">
			{#if !ready}
				<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-8 text-center">
					<div class="w-10 h-10 mx-auto rounded-full border-[3px] border-[#c6c6cd] border-t-[#131b2e] animate-spin"></div>
					<p class="mt-4 text-sm text-[#515f74]">Loading your profile…</p>
				</div>
			{:else}
				<div class="space-y-2">
					<p class="text-[11px] font-semibold uppercase tracking-wider text-[#515f74]">
						One-time setup
					</p>
					<h1 class="text-2xl sm:text-[30px] font-bold tracking-tight leading-tight">
						{#if auth.demoEnabled}
							Set up your student profile
						{:else}
							Welcome, {firstName}
						{/if}
					</h1>
					<p class="text-sm text-[#515f74] leading-relaxed max-w-lg">
						{#if auth.demoEnabled}
							Enter your student details to set up your demo profile — this is saved to the backend and pre-fills your portal.
						{:else}
							Microsoft gave us your name and AUC email. Add the academic details the exchange
							application needs — you only do this once.
						{/if}
					</p>
				</div>

				{#if !auth.demoEnabled}
					<div class="flex items-center gap-3 rounded-xl bg-[#eff4ff] border border-[#c6c6cd]/40 px-4 py-3">
						<div class="w-10 h-10 rounded-full bg-[#dce9ff] border border-[#c6c6cd]/60 flex items-center justify-center text-sm font-bold">
							{auth.initials}
						</div>
						<div class="min-w-0">
							<div class="text-sm font-semibold text-[#0b1c30] truncate">{fullName.trim() || auth.displayName}</div>
							<div class="text-xs text-[#515f74] font-mono-num truncate">{auth.email}</div>
						</div>
						<span class="ml-auto px-2.5 py-1 rounded-full bg-white border border-[#c6c6cd]/40 text-[11px] font-semibold text-[#15803d] inline-flex items-center gap-1.5 shrink-0">
							<Icon name="check" class="w-3 h-3" strokeWidth={2.4} /> Verified by Microsoft
						</span>
					</div>
				{/if}

				<form
					onsubmit={submit}
					class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-5 sm:p-7 space-y-5"
				>
					{#if serverError}
						<div class="rounded-xl bg-[#fef2f2] border border-[#ba1a1a]/30 px-4 py-3 text-xs font-medium text-[#ba1a1a]">
							{serverError}
						</div>
					{/if}

					<div class="flex flex-col gap-1.5">
						<label for="full-name" class={labelCls}>Full name</label>
						<input
							id="full-name"
							class={field}
							bind:value={fullName}
							placeholder="Mohamed Hassan"
							autocomplete="name"
						/>
						{#if nameError}<p class="text-xs font-medium text-[#ba1a1a]">{nameError}</p>{/if}
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

					<div class="flex flex-col gap-1.5 min-w-0">
						<label for="major" class={labelCls}>AUC Major</label>
						<MajorSelect id="major" {level} bind:value={major} invalid={!!errors.major} />
						{#if errors.major}<p class="text-xs font-medium text-[#ba1a1a]">{errors.major}</p>{/if}
					</div>

					<div class="grid grid-cols-1 sm:grid-cols-2 gap-4">
						<div class="flex flex-col gap-1.5">
							<label for="auc-id" class={labelCls}>AUC ID</label>
							<input
								id="auc-id"
								class="{field} font-mono-num"
								bind:value={aucId}
								inputmode="numeric"
								maxlength="9"
								placeholder="900219004"
							/>
							{#if errors.aucId}<p class="text-xs font-medium text-[#ba1a1a]">{errors.aucId}</p>{/if}
						</div>

						<div class="flex flex-col gap-1.5">
							<label for="phone" class={labelCls}>Phone</label>
							<input
								id="phone"
								type="tel"
								class="{field} font-mono-num"
								bind:value={phone}
								placeholder="+20 100 555 2190"
							/>
							{#if errors.phone}<p class="text-xs font-medium text-[#ba1a1a]">{errors.phone}</p>{/if}
						</div>
					</div>

					<div class="pt-1 border-t border-[#c6c6cd]/30 space-y-3">
						<button
							type="submit"
							disabled={saving}
							class="w-full h-11 rounded-xl bg-[#131b2e] hover:bg-[#1e293b] text-white text-sm font-semibold shadow-sm inline-flex items-center justify-center gap-2 cursor-pointer disabled:opacity-60 disabled:cursor-not-allowed"
						>
							{saving ? 'Saving…' : 'Save and continue'}
							<Icon name="arrow-right" class="w-4 h-4" />
						</button>
						<p class="text-[11px] text-[#515f74] leading-relaxed">
							These details pre-fill your application. You can change them later in your profile.
						</p>
					</div>
				</form>
			{/if}
		</div>
	</div>
</main>
