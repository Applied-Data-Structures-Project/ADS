<script lang="ts">
	import type { AdminRunDiff, AdminSavedRunSummary } from '#lib/admin/types.ts';

	interface Props {
		open: boolean;
		runs: AdminSavedRunSummary[];
		selectedRunA: number;
		selectedRunB: number;
		diff: AdminRunDiff | null;
		loading?: boolean;
		onSelectRuns: (runA: number, runB: number) => void;
		onLoadRun?: (run: AdminSavedRunSummary) => void;
		onClose: () => void;
	}

	let {
		open,
		runs,
		selectedRunA,
		selectedRunB,
		diff,
		loading = false,
		onSelectRuns,
		onLoadRun,
		onClose
	}: Props = $props();

	let changeFilter = $state<'all' | 'gained' | 'lost' | 'shifted'>('all');

	const runAObj = $derived(runs.find((r) => r.id === selectedRunA) ?? null);
	const runBObj = $derived(runs.find((r) => r.id === selectedRunB) ?? null);

	const filteredChanges = $derived.by(() => {
		if (!diff) return [];
		if (changeFilter === 'all') return diff.changes;
		return diff.changes.filter((c) => c.changeType === changeFilter);
	});

	function badgeClass(kind: string): string {
		if (kind === 'gained') return 'bg-[#f0fdf4] text-[#15803d]';
		if (kind === 'lost') return 'bg-[#fef2f2] text-[#ba1a1a]';
		return 'bg-[#fffbeb] text-[#b45309]';
	}
</script>

<svelte:window
	onkeydown={(e) => {
		if (e.key === 'Escape' && open) onClose();
	}}
/>

{#if open}
	<div
		class="fixed inset-0 z-50 flex items-center justify-center p-3 sm:p-5"
		role="dialog"
		aria-modal="true"
		aria-label="Compare Saved Matching Runs"
	>
		<button
			type="button"
			aria-label="Close run comparison modal"
			onclick={onClose}
			class="fixed inset-0 bg-[#0b1c30]/45 backdrop-blur-[2px]"
		></button>

		<div class="modal-pop relative z-10 bg-white rounded-2xl shadow-2xl border border-[#c6c6cd]/60 w-full max-w-4xl max-h-[90vh] flex flex-col overflow-hidden">
			<!-- Header -->
			<div class="px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/40 flex items-center justify-between gap-3">
				<div>
					<h2 class="text-lg font-bold text-[#0b1c30]">Saved Run Comparison &amp; Student Diff</h2>
					<p class="text-xs text-[#515f74] mt-0.5">
						Inspect exact placement shifts computed in C++ between any two persisted simulation runs.
					</p>
				</div>
				<button
					type="button"
					onclick={onClose}
					class="h-9 w-9 rounded-xl border border-[#c6c6cd]/40 bg-white text-[#515f74] hover:text-[#0b1c30] flex items-center justify-center cursor-pointer text-lg shrink-0"
				>
					&times;
				</button>
			</div>

			<!-- Run Selectors -->
			<div class="p-4 sm:p-5 border-b border-[#c6c6cd]/30 bg-white grid grid-cols-1 sm:grid-cols-2 gap-3.5">
				<div class="flex flex-col gap-1">
					<div class="flex items-center justify-between gap-2">
						<label for="diff-run-a" class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
							Target Run (New)
						</label>
						{#if onLoadRun && runAObj}
							<button
								type="button"
								onclick={() => {
									onLoadRun(runAObj);
									onClose();
								}}
								class="text-xs font-semibold text-[#003ea8] hover:underline cursor-pointer"
							>
								Load {runAObj.runCode} &rarr;
							</button>
						{/if}
					</div>
					<select
						id="diff-run-a"
						value={selectedRunA}
						onchange={(e) =>
							onSelectRuns(Number((e.currentTarget as HTMLSelectElement).value), selectedRunB)}
						class="stitch-select h-10 px-3 bg-[#eff4ff] text-[#0b1c30] text-sm font-medium rounded-xl border border-[#c6c6cd]/40"
					>
						{#each runs as r (r.id)}
							<option value={r.id}>{r.runCode} · {r.title} ({r.placed} placed)</option>
						{/each}
					</select>
				</div>

				<div class="flex flex-col gap-1">
					<div class="flex items-center justify-between gap-2">
						<label for="diff-run-b" class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
							Baseline Run (Compare Against)
						</label>
						{#if onLoadRun && runBObj}
							<button
								type="button"
								onclick={() => {
									onLoadRun(runBObj);
									onClose();
								}}
								class="text-xs font-semibold text-[#003ea8] hover:underline cursor-pointer"
							>
								Load {runBObj.runCode} &rarr;
							</button>
						{/if}
					</div>
					<select
						id="diff-run-b"
						value={selectedRunB}
						onchange={(e) =>
							onSelectRuns(selectedRunA, Number((e.currentTarget as HTMLSelectElement).value))}
						class="stitch-select h-10 px-3 bg-[#eff4ff] text-[#0b1c30] text-sm font-medium rounded-xl border border-[#c6c6cd]/40"
					>
						{#each runs as r (r.id)}
							<option value={r.id}>{r.runCode} · {r.title} ({r.placed} placed)</option>
						{/each}
					</select>
				</div>
			</div>

			<!-- Content -->
			<div class="p-4 sm:p-5 overflow-y-auto space-y-4 flex-1 bg-[#f8f9ff]">
				{#if loading}
					<div class="py-12 text-center text-sm text-[#515f74]">Computing run diff in C++…</div>
				{:else if !diff || !diff.hasBaseline}
					<div class="py-12 text-center text-sm text-[#515f74]">
						Select two saved runs above to compare student assignments.
					</div>
				{:else}
					<div class="grid grid-cols-2 sm:grid-cols-5 gap-3">
						<div class="bg-white p-3.5 rounded-xl border border-[#c6c6cd]/40 shadow-sm">
							<div class="text-[11px] font-semibold uppercase text-[#515f74]">Net Placed</div>
							<div
								class="text-xl font-bold font-mono-num mt-1 {diff.placedDelta >= 0
									? 'text-[#15803d]'
									: 'text-[#ba1a1a]'}"
							>
								{diff.placedDelta >= 0 ? `+${diff.placedDelta}` : diff.placedDelta}
							</div>
						</div>
						<div class="bg-white p-3.5 rounded-xl border border-[#c6c6cd]/40 shadow-sm">
							<div class="text-[11px] font-semibold uppercase text-[#515f74]">1st Choice</div>
							<div
								class="text-xl font-bold font-mono-num mt-1 {diff.firstChoiceDelta >= 0
									? 'text-[#15803d]'
									: 'text-[#ba1a1a]'}"
							>
								{diff.firstChoiceDelta >= 0 ? `+${diff.firstChoiceDelta}` : diff.firstChoiceDelta}
							</div>
						</div>
						<div class="bg-white p-3.5 rounded-xl border border-[#c6c6cd]/40 shadow-sm">
							<div class="text-[11px] font-semibold uppercase text-[#515f74]">Gained Seat</div>
							<div class="text-xl font-bold font-mono-num text-[#15803d] mt-1">{diff.gainedCount}</div>
						</div>
						<div class="bg-white p-3.5 rounded-xl border border-[#c6c6cd]/40 shadow-sm">
							<div class="text-[11px] font-semibold uppercase text-[#515f74]">Shifted Uni</div>
							<div class="text-xl font-bold font-mono-num text-[#b45309] mt-1">{diff.shiftedCount}</div>
						</div>
						<div class="bg-white p-3.5 rounded-xl border border-[#c6c6cd]/40 shadow-sm col-span-2 sm:col-span-1">
							<div class="text-[11px] font-semibold uppercase text-[#515f74]">Lost Seat</div>
							<div class="text-xl font-bold font-mono-num text-[#ba1a1a] mt-1">{diff.lostCount}</div>
						</div>
					</div>

					<!-- Filter Chips -->
					<div class="flex items-center gap-2 flex-wrap">
						{#each [['all', `All (${diff.changes.length})`], ['shifted', `Shifted (${diff.shiftedCount})`], ['gained', `Gained (${diff.gainedCount})`], ['lost', `Lost (${diff.lostCount})`]] as [key, label] (key)}
							<button
								type="button"
								onclick={() => (changeFilter = key as 'all' | 'gained' | 'lost' | 'shifted')}
								class="px-3 py-1 rounded-full text-xs font-semibold cursor-pointer {changeFilter === key
									? 'bg-[#131b2e] text-white'
									: 'bg-[#e5eeff] text-[#0b1c30]'}"
							>
								{label}
							</button>
						{/each}
					</div>

					{#if filteredChanges.length === 0}
						<div class="bg-white rounded-xl p-8 text-center text-sm text-[#515f74] border border-[#c6c6cd]/40">
							No placement differences in this category.
						</div>
					{:else}
						<div class="bg-white rounded-xl border border-[#c6c6cd]/40 shadow-sm overflow-x-auto">
							<table class="w-full text-left border-collapse text-xs sm:text-sm">
								<thead class="bg-[#eff4ff] text-[#515f74] text-xs font-semibold uppercase border-b border-[#c6c6cd]/30">
									<tr>
										<th class="py-3 px-4">Student</th>
										<th class="py-3 px-4">Change</th>
										<th class="py-3 px-4">Previous Partner</th>
										<th class="py-3 px-4">New Partner</th>
										<th class="py-3 px-4 text-right">Score Δ</th>
									</tr>
								</thead>
								<tbody class="divide-y divide-[#c6c6cd]/20">
									{#each filteredChanges.slice(0, 150) as c (c.studentEmail + c.changeType)}
										<tr class="hover:bg-[#eff4ff]/50">
											<td class="py-2.5 px-4">
												<div class="font-semibold text-[#0b1c30]">{c.studentName}</div>
												<div class="text-[11px] font-mono-num text-[#515f74]">{c.studentEmail}</div>
											</td>
											<td class="py-2.5 px-4">
												<span class="px-2 py-0.5 rounded-full text-[11px] font-semibold capitalize {badgeClass(c.changeType)}">
													{c.changeType}
												</span>
											</td>
											<td class="py-2.5 px-4 text-[#515f74]">{c.fromUniName || 'Unplaced'}</td>
											<td class="py-2.5 px-4 font-medium text-[#0b1c30]">{c.toUniName || 'Unplaced'}</td>
											<td class="py-2.5 px-4 text-right font-mono-num font-semibold">
												{c.scoreDelta >= 0 ? `+${c.scoreDelta.toFixed(3)}` : c.scoreDelta.toFixed(3)}
											</td>
										</tr>
									{/each}
								</tbody>
							</table>
						</div>
					{/if}
				{/if}
			</div>

			<div class="px-5 py-3.5 bg-[#eff4ff] border-t border-[#c6c6cd]/30 flex justify-end">
				<button
					type="button"
					onclick={onClose}
					class="h-10 px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl cursor-pointer"
				>
					Close Comparison
				</button>
			</div>
		</div>
	</div>
{/if}
