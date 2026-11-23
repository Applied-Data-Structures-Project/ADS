<script lang="ts" module>
	// Line icons for the student side, drawn in the same style as the admin console.
	export type IconName =
		| 'explore'
		| 'compare'
		| 'saved'
		| 'apply'
		| 'status'
		| 'letters'
		| 'profile'
		| 'inbox'
		| 'search'
		| 'check'
		| 'x'
		| 'upload'
		| 'file'
		| 'arrow-right'
		| 'arrow-left'
		| 'lock'
		| 'globe'
		| 'alert'
		| 'moon'
		| 'sun'
		| 'logout'
		| 'plus'
		| 'book'
		| 'download';
</script>

<script lang="ts">
	interface Props {
		name: IconName;
		class?: string;
		strokeWidth?: number | string;
	}

	let { name, class: className = 'w-[18px] h-[18px]', strokeWidth = 1.75 }: Props = $props();
</script>

<svg
	class={className}
	viewBox="0 0 24 24"
	fill="none"
	stroke="currentColor"
	stroke-width={strokeWidth}
	stroke-linecap="round"
	stroke-linejoin="round"
	aria-hidden="true"
>
	{#if name === 'explore'}
		<circle cx="12" cy="12" r="9" />
		<polygon points="15.5 8.5 13.5 13.5 8.5 15.5 10.5 10.5 15.5 8.5" />
	{:else if name === 'compare'}
		<rect x="3" y="4" width="7" height="16" rx="1.5" />
		<rect x="14" y="4" width="7" height="16" rx="1.5" />
	{:else if name === 'saved'}
		<path d="M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z" />
	{:else if name === 'apply'}
		<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
		<polyline points="14 2 14 8 20 8" />
		<line x1="8" y1="13" x2="16" y2="13" />
		<line x1="8" y1="17" x2="13" y2="17" />
	{:else if name === 'status'}
		<circle cx="12" cy="12" r="9" />
		<polyline points="12 7 12 12 15.5 14" />
	{:else if name === 'letters'}
		<rect x="3" y="5" width="18" height="14" rx="2" />
		<polyline points="3 7 12 13 21 7" />
	{:else if name === 'profile'}
		<path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2" />
		<circle cx="12" cy="7" r="4" />
	{:else if name === 'inbox'}
		<polyline points="22 12 16 12 14 15 10 15 8 12 2 12" />
		<path d="M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z" />
	{:else if name === 'search'}
		<circle cx="11" cy="11" r="8" />
		<line x1="21" y1="21" x2="16.65" y2="16.65" />
	{:else if name === 'check'}
		<polyline points="20 6 9 17 4 12" />
	{:else if name === 'x'}
		<line x1="18" y1="6" x2="6" y2="18" />
		<line x1="6" y1="6" x2="18" y2="18" />
	{:else if name === 'upload'}
		<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
		<polyline points="17 8 12 3 7 8" />
		<line x1="12" y1="3" x2="12" y2="15" />
	{:else if name === 'file'}
		<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
		<polyline points="14 2 14 8 20 8" />
	{:else if name === 'arrow-right'}
		<line x1="5" y1="12" x2="19" y2="12" />
		<polyline points="12 5 19 12 12 19" />
	{:else if name === 'arrow-left'}
		<line x1="19" y1="12" x2="5" y2="12" />
		<polyline points="12 19 5 12 12 5" />
	{:else if name === 'lock'}
		<rect x="4" y="11" width="16" height="10" rx="2" />
		<path d="M8 11V7a4 4 0 0 1 8 0v4" />
	{:else if name === 'globe'}
		<circle cx="12" cy="12" r="9" />
		<line x1="3" y1="12" x2="21" y2="12" />
		<path d="M12 3a14 14 0 0 1 0 18a14 14 0 0 1 0-18z" />
	{:else if name === 'alert'}
		<circle cx="12" cy="12" r="10" />
		<line x1="12" y1="8" x2="12" y2="12" />
		<line x1="12" y1="16" x2="12.01" y2="16" />
	{:else if name === 'moon'}
		<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z" />
	{:else if name === 'sun'}
		<circle cx="12" cy="12" r="4" />
		<line x1="12" y1="2" x2="12" y2="4" />
		<line x1="12" y1="20" x2="12" y2="22" />
		<line x1="4.93" y1="4.93" x2="6.34" y2="6.34" />
		<line x1="17.66" y1="17.66" x2="19.07" y2="19.07" />
		<line x1="2" y1="12" x2="4" y2="12" />
		<line x1="20" y1="12" x2="22" y2="12" />
		<line x1="6.34" y1="17.66" x2="4.93" y2="19.07" />
		<line x1="19.07" y1="4.93" x2="17.66" y2="6.34" />
	{:else if name === 'logout'}
		<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4" />
		<polyline points="16 17 21 12 16 7" />
		<line x1="21" y1="12" x2="9" y2="12" />
	{:else if name === 'plus'}
		<line x1="12" y1="5" x2="12" y2="19" />
		<line x1="5" y1="12" x2="19" y2="12" />
	{:else if name === 'book'}
		<path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20" />
		<path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z" />
	{:else if name === 'download'}
		<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
		<polyline points="7 10 12 15 17 10" />
		<line x1="12" y1="15" x2="12" y2="3" />
	{/if}
</svg>
