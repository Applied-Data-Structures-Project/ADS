<script lang="ts">
	// The admin console is IPSO-staff only. Real enforcement lands with the C++
	// backend; for now the Entra app role decides whether the UI renders.
	import { onMount, type Snippet } from 'svelte';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import SessionGate from '#lib/components/auth/SessionGate.svelte';
	import { auth } from '#lib/auth/store.svelte.ts';
	import { roleHome, roleLabel } from '#lib/auth/roles.ts';

	let { children }: { children: Snippet } = $props();

	onMount(async () => {
		await auth.init();
		if (!auth.signedIn) {
			const next = page.url.pathname + page.url.search;
			await goto(`/login?next=${encodeURIComponent(next)}`, { replace: true });
		}
	});
</script>

{#if auth.status === 'loading'}
	<SessionGate message="Checking your session…" spinner />
{:else if !auth.signedIn}
	<SessionGate message="Taking you to Microsoft sign-in…" spinner />
{:else if auth.role === 'admin'}
	{@render children()}
{:else}
	<SessionGate
		message={`You are signed in as ${roleLabel(auth.role)}. The admin console is for IPSO staff.`}
		href={roleHome(auth.role)}
		actionLabel="Go to my area"
		signOut
	/>
{/if}
