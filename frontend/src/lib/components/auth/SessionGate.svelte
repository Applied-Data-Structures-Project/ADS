<script lang="ts">
	// Shared full-screen state for the auth guards: checking, redirecting to
	// Microsoft, or signed in with the wrong role.
	import { auth } from '#lib/auth/store.svelte.ts';

	interface Props {
		message: string;
		spinner?: boolean;
		actionLabel?: string;
		href?: string;
		signOut?: boolean;
	}

	let { message, spinner = false, actionLabel, href, signOut = false }: Props = $props();
</script>

<main class="min-h-screen bg-[#f8f9ff] text-[#0b1c30] flex items-center justify-center p-6">
	<div class="w-full max-w-sm bg-white rounded-2xl shadow-sm border border-[#c6c6cd]/40 p-6 text-center space-y-4">
		<img src="/auc-logo.png" alt="The American University in Cairo" class="h-8 mx-auto" />

		{#if spinner}
			<div class="w-8 h-8 mx-auto rounded-full border-[3px] border-[#c6c6cd] border-t-[#131b2e] animate-spin"></div>
		{/if}

		<p class="text-sm text-[#515f74] leading-relaxed">{message}</p>

		{#if href && actionLabel}
			<a
				href={href}
				class="h-11 w-full bg-[#131b2e] hover:bg-[#1e293b] text-white text-sm font-semibold rounded-xl shadow-sm flex items-center justify-center transition-colors"
			>
				{actionLabel}
			</a>
		{/if}

		{#if signOut}
			<button
				type="button"
				onclick={() => auth.signOut()}
				class="text-xs font-semibold text-[#515f74] hover:text-[#0b1c30] underline underline-offset-2 cursor-pointer"
			>
				Sign out
			</button>
		{/if}
	</div>
</main>
