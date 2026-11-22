<script lang="ts">
	// Real AUC Microsoft sign-in. Students and IPSO staff share one door;
	// the account's Entra app roles decide where they land.
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import MicrosoftMark from '#lib/components/auth/MicrosoftMark.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import { DEMO_ROLES, demoLabel } from '#lib/auth/demo.ts';
	import { isSafeNext, roleHome, type Role } from '#lib/auth/roles.ts';
	import { auth, type LoginKind } from '#lib/auth/store.svelte.ts';
	import { callServer } from '#lib/student/api.ts';

	let kind = $state<LoginKind>('student');
	let busy = $state(false);
	let dark = $state(false);

	const requestedNext = $derived(page.url.searchParams.get('next'));
	const next = $derived(isSafeNext(requestedNext) ? requestedNext : null);

	const copy = $derived(
		kind === 'student'
			? {
					title: 'Sign in to your exchange portal',
					body: 'Rank partner universities, complete one application, and track your recommendation letter.'
				}
			: {
					title: 'IPSO staff sign in',
					body: 'Open student applications, the matching console, and partner university seat controls.'
				}
	);

	onMount(async () => {
		try {
			dark = localStorage.getItem('auc-student-dark-mode') === '1';
		} catch {
			dark = false;
		}

		auth.rememberNext(next);
		await auth.init();
		if (auth.signedIn && !auth.error) {
			await goto(auth.needsProfile ? '/signup' : (next ?? roleHome(auth.role)), { replace: true });
		}
	});

	function toggleDark() {
		dark = !dark;
		try {
			localStorage.setItem('auc-student-dark-mode', dark ? '1' : '0');
		} catch {
			// Storage blocked: the choice only lasts for this visit.
		}
	}

	async function signIn() {
		busy = true;
		await auth.signIn(kind);
		// Reached only when the redirect could not start; auth.error explains why.
		busy = false;
	}

	async function demoSignIn(role: Role) {
		auth.signInAsDemo(role);
		if (role === 'student') {
			try {
				const me = await callServer<{
					profile: { name?: string; level?: string; major?: string; aucId?: string; phone?: string } | null;
				}>('GET', '/api/me');
				auth.syncFromServerProfile(me.profile);
			} catch {
				// Offline: fall back to any locally cached profile.
			}
			await goto(auth.needsProfile ? '/signup' : (next ?? roleHome(role)), { replace: true });
			return;
		}
		await goto(roleHome(role), { replace: true });
	}
</script>

<svelte:head>
	<title>Sign in · AUC IPSO Exchange</title>
</svelte:head>

<div class="min-h-screen bg-[#f8f9ff] text-[#0b1c30] transition-colors duration-200 {dark ? 'admin-dark' : ''}">
	<div class="min-h-screen lg:grid lg:grid-cols-[1.08fr_1fr]">
		<!-- BRAND PANEL -->
		<aside class="relative hidden lg:flex flex-col justify-between overflow-hidden bg-[#0b1c30] p-10 xl:p-14 text-white">
			<div class="absolute -right-28 -top-28 h-80 w-80 rounded-full bg-[#dce9ff]/10 blur-3xl"></div>
			<div class="absolute -left-20 bottom-10 h-64 w-64 rounded-full bg-[#0b1c30]/60 blur-2xl"></div>

			<div class="relative flex items-center gap-3.5">
				<img
					src="/auc-logo.png"
					alt="The American University in Cairo"
					class="h-9 w-auto brightness-0 invert opacity-95"
				/>
				<span class="h-6 w-px bg-white/20"></span>
				<span class="text-sm font-semibold tracking-tight text-white/90">IPSO Exchange</span>
			</div>

			<div class="relative max-w-md space-y-6">
				<p class="text-[11px] font-semibold uppercase tracking-[0.18em] text-[#9db6d8]">
					Study abroad · Fall 2027
				</p>
				<h1 class="text-4xl xl:text-[44px] font-bold leading-[1.1] tracking-tight">
					One application.<br />The right university.
				</h1>
				<p class="text-[15px] leading-relaxed text-[#c7d5e8]">
					AUC's partner universities, your ranked choices, documents and recommendation letters — in one
					place, matched fairly.
				</p>

				<ul class="space-y-3.5 pt-2">
					{#each ['Explore 30+ partner universities', 'Rank three choices and apply once', 'Track your status and letters'] as point (point)}
						<li class="flex items-center gap-3 text-sm text-[#e4ecf7]">
							<span class="w-6 h-6 rounded-full bg-white/10 border border-white/15 flex items-center justify-center shrink-0">
								<Icon name="check" class="w-3.5 h-3.5" strokeWidth={2.2} />
							</span>
							{point}
						</li>
					{/each}
				</ul>
			</div>

			<p class="relative text-xs text-white/45">
				The American University in Cairo · International Programs and Study Abroad
			</p>
		</aside>

		<!-- SIGN-IN SIDE -->
		<main class="flex min-h-screen flex-col">
			<div class="flex items-center justify-between gap-3 p-5 sm:p-7">
				<img
					src="/auc-logo.png"
					alt="The American University in Cairo"
					class="h-7 w-auto lg:hidden {dark ? 'brightness-0 invert opacity-95' : ''}"
				/>
				<span class="hidden lg:inline text-xs font-semibold uppercase tracking-[0.16em] text-[#515f74]">
					IPSO Exchange
				</span>

				<div class="flex items-center gap-2 shrink-0">
					{#if auth.demoEnabled}
						<span
							class="inline-flex items-center gap-1.5 h-9 px-3 rounded-xl bg-[#fffbeb] border border-[#c6c6cd]/40 text-[11px] font-semibold text-[#b45309]"
						>
							<Icon name="alert" class="w-3.5 h-3.5" />
							<span>Demo mode</span>
						</span>
					{/if}

					<button
						type="button"
						onclick={toggleDark}
						aria-label={dark ? 'Switch to light mode' : 'Switch to dark mode'}
						class="h-9 px-3 rounded-xl bg-[#eff4ff] hover:bg-[#e5eeff] border border-[#c6c6cd]/50 text-[#0b1c30] flex items-center gap-2 text-xs font-semibold cursor-pointer transition-colors shrink-0"
					>
						<Icon name={dark ? 'sun' : 'moon'} class="w-4 h-4" strokeWidth={2} />
						<span class="hidden sm:inline">{dark ? 'Light' : 'Dark'}</span>
					</button>
				</div>
			</div>

			<div class="flex flex-1 items-center justify-center px-5 pb-8 sm:px-7">
				<div class="w-full max-w-[420px] space-y-5">
					<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-6 sm:p-8 space-y-6">
						{#if auth.demoEnabled}
							<div class="space-y-4">
								<div class="flex items-start gap-3 rounded-xl bg-[#fffbeb] border border-[#c6c6cd]/40 p-4">
									<span class="w-9 h-9 rounded-lg bg-white border border-[#c6c6cd]/40 flex items-center justify-center shrink-0 text-[#b45309]">
										<Icon name="profile" class="w-4 h-4" />
									</span>
									<div class="space-y-1 min-w-0">
										<div class="text-[13px] font-semibold text-[#b45309]">Demo mode</div>
										<p class="text-[11px] text-[#515f74] leading-relaxed">
											Microsoft sign-in is not connected yet. Pick a role to explore the portal with
											sample data. Nothing here is real authentication.
										</p>
									</div>
								</div>

								<div class="space-y-1.5">
									<h2 class="text-xl sm:text-[22px] font-bold tracking-tight text-[#0b1c30]">
										Try the portal
									</h2>
									<p class="text-sm text-[#515f74] leading-relaxed">
										Each role opens its own home with the same pages and guards as the real thing.
									</p>
								</div>

								<div class="grid gap-2">
									{#each DEMO_ROLES as role (role)}
										<button
											type="button"
											onclick={() => demoSignIn(role)}
											class="h-11 w-full rounded-xl bg-white border border-[#c6c6cd] text-[#0b1c30] text-sm font-semibold shadow-sm inline-flex items-center justify-between gap-3 px-4 hover:bg-[#f8f9ff] transition-colors cursor-pointer"
										>
											<span class="inline-flex items-center gap-2.5">
												<Icon name="profile" class="w-4 h-4 text-[#515f74]" />
												{demoLabel(role)}
											</span>
											<Icon name="arrow-right" class="w-4 h-4 text-[#515f74]" />
										</button>
									{/each}
								</div>

								{#if auth.error}
									<div
										class="rounded-xl bg-[#fef2f2] border border-[#c6c6cd]/40 p-4 text-sm text-[#ba1a1a]"
										role="alert"
									>
										{auth.error}
									</div>
								{/if}
							</div>
						{:else}
							<!-- Student / Administrator -->
							<div>
								<div class="grid grid-cols-2 gap-1 rounded-xl bg-[#eff4ff] border border-[#c6c6cd]/40 p-1">
								<button
									type="button"
									onclick={() => (kind = 'student')}
									aria-pressed={kind === 'student'}
									class="h-9 rounded-lg text-sm font-semibold transition-colors cursor-pointer {kind ===
									'student'
										? 'bg-white text-[#0b1c30] shadow-sm'
										: 'text-[#515f74] hover:text-[#0b1c30]'}"
								>
									Student
								</button>
								<button
									type="button"
									onclick={() => (kind = 'admin')}
									aria-pressed={kind === 'admin'}
									class="h-9 rounded-lg text-sm font-semibold transition-colors cursor-pointer {kind ===
									'admin'
										? 'bg-white text-[#0b1c30] shadow-sm'
										: 'text-[#515f74] hover:text-[#0b1c30]'}"
								>
									Administrator
								</button>
							</div>
							<p class="mt-2 text-[11px] text-[#515f74]">
								{kind === 'student'
									? 'Students and applicants sign in here.'
									: 'IPSO staff and faculty use the same AUC account.'}
							</p>
						</div>

						<div class="space-y-1.5">
							<h2 class="text-xl sm:text-[22px] font-bold tracking-tight text-[#0b1c30]">
								{copy.title}
							</h2>
							<p class="text-sm text-[#515f74] leading-relaxed">{copy.body}</p>
						</div>

						{#if auth.configMissing.length > 0}
							<div class="rounded-xl bg-[#fffbeb] border border-[#c6c6cd]/40 p-4 text-xs text-[#b45309] space-y-1.5">
								<div class="flex items-center gap-2 text-[13px] font-semibold">
									<Icon name="alert" class="w-4 h-4" />
									Microsoft sign-in is not configured
								</div>
								<p class="leading-relaxed">
									Add {auth.configMissing.join(' and ')} to
									<code class="font-mono-num bg-white/60 px-1 py-0.5 rounded">frontend/.env</code>
									and restart the dev server. See <code class="font-mono-num">frontend/.env.example</code>.
								</p>
							</div>
						{:else if auth.error}
							<div class="rounded-xl bg-[#fef2f2] border border-[#c6c6cd]/40 p-4 text-sm text-[#ba1a1a] space-y-2" role="alert">
								<div class="flex items-start gap-2">
									<Icon name="alert" class="w-4 h-4 mt-0.5 shrink-0" />
									<span class="font-medium leading-relaxed">{auth.error}</span>
								</div>
								<button
									type="button"
									onclick={() => auth.signOut()}
									class="text-xs font-semibold underline underline-offset-2 hover:text-[#7f1d1d] cursor-pointer"
								>
									Sign in with a different account
								</button>
							</div>
						{/if}

						<button
							type="button"
							onclick={signIn}
							disabled={busy || auth.configMissing.length > 0}
							class="w-full h-11 rounded-xl bg-white border border-[#c6c6cd] text-[#0b1c30] text-sm font-semibold shadow-sm inline-flex items-center justify-center gap-3 hover:bg-[#f8f9ff] transition-colors cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
						>
							<MicrosoftMark class="w-[18px] h-[18px]" />
							{busy ? 'Opening Microsoft…' : 'Continue with Microsoft'}
						</button>

						<div class="flex items-start gap-2 text-[11px] text-[#515f74] leading-relaxed">
							<Icon name="lock" class="w-3.5 h-3.5 mt-0.5 shrink-0" />
							<span>
								You'll continue on Microsoft's sign-in page. IPSO never sees your password, and only
								<code class="font-mono-num">@aucegypt.edu</code> accounts are accepted.
							</span>
						</div>
						{/if}
					</div>

					<p class="text-center text-xs text-[#515f74]">
						Problems signing in? Contact
						<a
							href="mailto:studyabroad@aucegypt.edu"
							class="font-semibold text-[#003ea8] hover:underline"
						>
							studyabroad@aucegypt.edu
						</a>
					</p>
				</div>
			</div>
		</main>
	</div>
</div>
