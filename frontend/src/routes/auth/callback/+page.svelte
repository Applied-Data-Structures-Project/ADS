<script lang="ts">
	// Microsoft redirects back here. Finish the sign-in, then send the person to
	// the destination their role and the starting page earned.
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';
	import Icon from '#lib/components/student/Icon.svelte';
	import { roleHome } from '#lib/auth/roles.ts';
	import { auth } from '#lib/auth/store.svelte.ts';

	let error = $state<string | null>(null);
	let done = $state(false);

	onMount(async () => {
		await auth.init();
		if (auth.error) {
			error = auth.error;
			return;
		}
		if (!auth.signedIn) {
			await goto('/login', { replace: true });
			return;
		}

		const intended = auth.takePendingKind();
		const next = auth.takePendingNext();

		if (intended === 'admin' && auth.role !== 'admin') {
			error = 'This account does not have IPSO staff access. Sign in with your staff account, or continue as a student.';
			return;
		}

		done = true;
		if (auth.role === 'admin') {
			await goto('/admin', { replace: true });
			return;
		}
		if (auth.role === 'faculty') {
			await goto('/doctor', { replace: true });
			return;
		}
		await goto(next ?? (auth.needsProfile ? '/signup' : roleHome('student')), { replace: true });
	});
</script>

<svelte:head>
	<title>Signing in · AUC IPSO Exchange</title>
</svelte:head>

<main class="min-h-screen bg-[#f8f9ff] flex items-center justify-center p-6">
	<div class="w-full max-w-md bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm p-8 text-center space-y-5">
		<img src="/auc-logo.png" alt="The American University in Cairo" class="h-8 mx-auto" />

		{#if error}
			<div class="space-y-4">
				<div class="w-11 h-11 mx-auto rounded-full bg-[#fef2f2] text-[#ba1a1a] flex items-center justify-center">
					<Icon name="alert" class="w-5 h-5" />
				</div>
				<div class="space-y-1.5">
					<h1 class="text-lg font-bold text-[#0b1c30]">Sign-in needs attention</h1>
					<p class="text-sm text-[#515f74] leading-relaxed">{error}</p>
				</div>
				<div class="flex flex-col gap-2">
					<button
						type="button"
						onclick={() => {
							auth.clearError();
							goto('/login');
						}}
						class="h-11 w-full rounded-xl bg-[#131b2e] hover:bg-[#1e293b] text-white text-sm font-semibold shadow-sm cursor-pointer"
					>
						Back to sign in
					</button>
					<button
						type="button"
						onclick={() => auth.signOut()}
						class="h-11 w-full rounded-xl bg-[#eff4ff] hover:bg-[#e5eeff] text-[#0b1c30] text-sm font-semibold border border-[#c6c6cd]/40 cursor-pointer"
					>
						Use a different account
					</button>
				</div>
			</div>
		{:else}
			<div class="space-y-4 py-4">
				<div class="w-10 h-10 mx-auto rounded-full border-[3px] border-[#c6c6cd] border-t-[#131b2e] animate-spin"></div>
				<div class="space-y-1.5">
					<h1 class="text-lg font-bold text-[#0b1c30]">Completing sign-in</h1>
					<p class="text-sm text-[#515f74]">
						{done ? 'Taking you to your portal…' : 'Checking your AUC Microsoft account…'}
					</p>
				</div>
			</div>
		{/if}
	</div>
</main>
