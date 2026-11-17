<script lang="ts">
	// Official four-square Microsoft mark. Brand colours are fixed by Microsoft,
	// so this one element ignores the app palette.
	interface Props {
		class?: string;
	}
	let { class: className = 'w-4 h-4' }: Props = $props();
</script>

<svg class={className} viewBox="0 0 23 23" aria-hidden="true">
	<rect x="1" y="1" width="10" height="10" fill="#f25022" />
	<rect x="12" y="1" width="10" height="10" fill="#7fba00" />
	<rect x="1" y="12" width="10" height="10" fill="#00a4ef" />
	<rect x="12" y="12" width="10" height="10" fill="#ffb900" />
</svg>
