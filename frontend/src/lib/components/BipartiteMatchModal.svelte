<script lang="ts">
	import { onDestroy } from 'svelte';

	interface Props {
		open: boolean;
		allowUnranked: boolean;
		darkMode?: boolean;
		onDone: () => void;
	}

	interface RootPoint {
		x: number;
		y: number;
	}

	interface GraphEdge {
		id: string;
		d: string;
		matched: boolean;
		contested: boolean;
		uIdx: number;
		vIdx: number;
		delayOffset: number;
	}

	interface GraphScene {
		uNodes: RootPoint[];
		fNodes: RootPoint[];
		sNodes: RootPoint[];
		vNodes: RootPoint[];
		trunks: { id: string; d: string; matched: boolean; unplaced: boolean; delay: number }[];
		branches: GraphEdge[];
		uMatched: boolean[];
		uUnplaced: boolean[];
		vMatched: boolean[];
		fActive: boolean[];
		sMatched: boolean[];
		lockOrder: number[];
	}

	let { open, allowUnranked, darkMode = false, onDone }: Props = $props();

	const BASE_Y = [42, 86, 130, 174, 218, 262];

	function randRange(min: number, max: number) {
		return Math.round(min + Math.random() * (max - min));
	}

	function shuffle<T>(arr: T[]): T[] {
		const copy = [...arr];
		for (let i = copy.length - 1; i > 0; i--) {
			const j = Math.floor(Math.random() * (i + 1));
			[copy[i], copy[j]] = [copy[j], copy[i]];
		}
		return copy;
	}

	function smoothCurve(a: RootPoint, b: RootPoint): string {
		const dx = b.x - a.x;
		const c1x = Math.round(a.x + dx * 0.5);
		const c2x = Math.round(b.x - dx * 0.5);
		return `M ${a.x} ${a.y} C ${c1x} ${a.y}, ${c2x} ${b.y}, ${b.x} ${b.y}`;
	}

	function generateScene(unrankedEnabled: boolean): GraphScene {
		const uNodes: RootPoint[] = BASE_Y.map((y) => ({ x: 56, y }));
		const vNodes: RootPoint[] = BASE_Y.map((y) => ({ x: 504, y }));

		// Locality-preserving permutation: 2 passes of random adjacent swaps keep |i - targetPerm[i]| <= 2
		const targetPerm = [0, 1, 2, 3, 4, 5];
		for (let pass = 0; pass < 2; pass++) {
			for (let i = 0; i < 5; i++) {
				if (Math.random() > 0.5) {
					[targetPerm[i], targetPerm[i + 1]] = [targetPerm[i + 1], targetPerm[i]];
				}
			}
		}

		// Distribute F and S junctions along smooth interpolated tracks and clamp adjacent separation
		const clampSeparated = (rawYs: number[], minGap = 24): number[] => {
			const order = rawYs.map((y, idx) => ({ y, idx })).sort((a, b) => a.y - b.y);
			for (let k = 1; k < order.length; k++) {
				if (order[k].y - order[k - 1].y < minGap) {
					order[k].y = order[k - 1].y + minGap;
				}
			}
			const maxY = order[order.length - 1].y;
			if (maxY > 274) {
				const shift = maxY - 274;
				for (const item of order) item.y = Math.max(26, item.y - shift);
			}
			const out = Array(rawYs.length).fill(0);
			for (const item of order) out[item.idx] = item.y;
			return out;
		};

		const rawFY = BASE_Y.map((y, i) =>
			Math.max(26, Math.min(274, Math.round(y * 0.78 + BASE_Y[targetPerm[i]] * 0.22)))
		);
		const rawSY = BASE_Y.map((y, i) =>
			Math.max(26, Math.min(274, Math.round(y * 0.32 + BASE_Y[targetPerm[i]] * 0.68)))
		);
		const fYs = clampSeparated(rawFY);
		const sYs = clampSeparated(rawSY);

		const fNodes: RootPoint[] = fYs.map((y) => ({
			x: 186 + randRange(-8, 8),
			y
		}));
		const sNodes: RootPoint[] = sYs.map((y) => ({
			x: 346 + randRange(-8, 8),
			y
		}));

		const winCount = unrankedEnabled ? (Math.random() > 0.45 ? 6 : 5) : 4 + (Math.random() > 0.5 ? 1 : 0);
		const pickedOrder = shuffle([0, 1, 2, 3, 4, 5]);
		const lockOrder = pickedOrder.slice(0, winCount);
		const uMatched = Array.from({ length: 6 }, (_, i) => lockOrder.includes(i));
		const uUnplaced = Array.from({ length: 6 }, (_, i) => !uMatched[i]);
		const vMatched = Array(6).fill(false);
		const sMatched = Array.from({ length: 6 }, (_, i) => uMatched[i]);

		const trunks = uNodes.map((u, i) => ({
			id: `tr-${i}`,
			d: smoothCurve(u, fNodes[i]),
			matched: uMatched[i],
			unplaced: uUnplaced[i],
			delay: i * 42
		}));

		const branches: GraphEdge[] = [];
		const usedSegments = new Set<string>();
		let edgeSeq = 0;

		const addBranch = (
			key: string,
			from: RootPoint,
			to: RootPoint,
			matched: boolean,
			contested: boolean,
			uIdx: number,
			vIdx: number
		) => {
			if (usedSegments.has(key)) return;
			usedSegments.add(key);
			branches.push({
				id: `br-${edgeSeq++}`,
				d: smoothCurve(from, to),
				matched,
				contested,
				uIdx,
				vIdx,
				delayOffset: randRange(90, 290)
			});
		};

		// Winning & primary smooth root paths (F[i] -> S[i] -> V[targetPerm[i]])
		for (let i = 0; i < 6; i++) {
			const vIdx = targetPerm[i];
			const isWin = uMatched[i];
			if (isWin) vMatched[vIdx] = true;

			addBranch(`fs-${i}-${i}`, fNodes[i], sNodes[i], isWin, !isWin && Math.random() > 0.35, i, vIdx);
			addBranch(`sv-${i}-${vIdx}`, sNodes[i], vNodes[vIdx], isWin, !isWin && Math.random() > 0.35, i, vIdx);
		}

		// Clean secondary rootlets: adjacent F -> S and nearby S -> V (no cross-canvas clutter)
		for (let i = 0; i < 6; i++) {
			const vIdx = targetPerm[i];
			const adjF = [i - 1, i + 1].filter((idx) => idx >= 0 && idx < 6);
			const altS = adjF[Math.floor(Math.random() * adjF.length)];
			if (altS !== undefined) {
				addBranch(`fs-${i}-${altS}`, fNodes[i], sNodes[altS], false, Math.random() > 0.6, i, vIdx);
			}

			const adjV = [vIdx - 1, vIdx + 1].filter((idx) => idx >= 0 && idx < 6);
			const altV = adjV[Math.floor(Math.random() * adjV.length)];
			if (altV !== undefined) {
				addBranch(`sv-${i}-${altV}`, sNodes[i], vNodes[altV], false, Math.random() > 0.55, i, altV);
			}
		}

		return {
			uNodes,
			fNodes,
			sNodes,
			vNodes,
			trunks,
			branches,
			uMatched,
			uUnplaced,
			vMatched,
			fActive: uMatched,
			sMatched,
			lockOrder
		};
	}

	let scene = $state<GraphScene>(generateScene(true));
	let stage = $state<0 | 1 | 2 | 3 | 4>(0);

	let timers: ReturnType<typeof setTimeout>[] = [];

	function clearAllTimers() {
		for (const t of timers) clearTimeout(t);
		timers = [];
	}

	function skip() {
		clearAllTimers();
		onDone();
	}

	$effect(() => {
		if (!open) {
			clearAllTimers();
			stage = 0;
			return;
		}

		clearAllTimers();
		scene = generateScene(allowUnranked);
		stage = 0;

		timers.push(setTimeout(() => (stage = 1), 50));
		timers.push(setTimeout(() => (stage = 2), 680));
		timers.push(setTimeout(() => (stage = 3), 1220));
		timers.push(setTimeout(() => (stage = 4), 1680));
		timers.push(
			setTimeout(() => {
				clearAllTimers();
				onDone();
			}, 2680)
		);

		return () => clearAllTimers();
	});

	onDestroy(clearAllTimers);

	const headerTitle = $derived(stage === 4 ? 'Optimal Match Complete' : 'Running Bipartite Match…');

	const stagePill = $derived.by(() => {
		if (stage <= 1) return 'Building Roots';
		if (stage === 2) return 'Augmenting Tree';
		if (stage === 3) return 'Rebalancing';
		return 'Complete';
	});

	const progressPct = $derived(
		stage === 0 ? 8 : stage === 1 ? 36 : stage === 2 ? 64 : stage === 3 ? 82 : 100
	);

	function branchStateClass(e: GraphEdge) {
		if (stage === 4) return e.matched ? 'matched' : 'pruned';
		if (stage === 3 && e.contested) return 'amber';
		if (stage >= 2) return 'bfs';
		return '';
	}

	function trunkStateClass(matched: boolean, unplaced: boolean) {
		if (stage === 4) return matched ? 'matched' : unplaced ? 'amber' : 'pruned';
		if (stage >= 2) return 'bfs';
		return '';
	}
</script>

{#if open}
	<div
		class="fixed inset-0 bg-[#0b1c30]/40 backdrop-blur-[2px] z-50 flex items-center justify-center p-3 sm:p-4"
		role="dialog"
		aria-modal="true"
		aria-label="Bipartite Matching Animation"
	>
		<div class="modal-pop bg-white rounded-2xl shadow-2xl border border-[#c6c6cd]/60 w-full max-w-xl overflow-hidden">
			<!-- Minimal Visual Header -->
			<div class="px-4 sm:px-5 py-3.5 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex items-center justify-between gap-3">
				<div class="flex items-center gap-2.5 min-w-0">
					<span
						class="w-2.5 h-2.5 rounded-full shrink-0 {stage === 4 ? 'bg-[#15803d]' : 'bg-[#2563eb] animate-pulse'}"
					></span>
					<span class="text-sm font-semibold text-[#0b1c30] truncate">{headerTitle}</span>
				</div>
				<button
					type="button"
					onclick={skip}
					class="text-xs font-semibold text-[#515f74] hover:text-[#0b1c30] px-2 py-1 rounded-md hover:bg-white/70 cursor-pointer shrink-0"
				>
					Skip
				</button>
			</div>

			<!-- Pure Visual Bipartite Tree & Root Graph (zero SVG text) -->
			<div class="p-3 sm:p-4 bg-[#f8f9ff]">
				<svg viewBox="0 0 560 300" class="w-full h-auto block select-none">
					<!-- Subtle layer dividers -->
					<line
						x1="186"
						y1="20"
						x2="186"
						y2="282"
						stroke={darkMode ? '#334155' : '#c6c6cd'}
						stroke-opacity={darkMode ? '0.45' : '0.28'}
						stroke-dasharray="3 5"
					/>
					<line
						x1="346"
						y1="20"
						x2="346"
						y2="282"
						stroke={darkMode ? '#334155' : '#c6c6cd'}
						stroke-opacity={darkMode ? '0.45' : '0.28'}
						stroke-dasharray="3 5"
					/>

					<!-- Root Trunks (U -> F) -->
					<g>
						{#each scene.trunks as tr, i (tr.id)}
							<path
								class="bg-edge {stage >= 1 ? 'grown' : ''} {trunkStateClass(tr.matched, tr.unplaced)}"
								style="transition-delay: {stage === 1 ? tr.delay : stage === 4 && tr.matched ? i * 55 : 0}ms"
								stroke={darkMode ? '#475569' : '#94a3b8'}
								stroke-width="2.1"
								d={tr.d}
							/>
						{/each}
					</g>

					<!-- Root Branches & Augmenting Tendrils -->
					<g>
						{#each scene.branches as e (e.id)}
							<path
								class="bg-edge {stage >= 1 ? 'grown' : ''} {branchStateClass(e)}"
								style="transition-delay: {stage === 1
									? e.delayOffset
									: stage === 4 && e.matched
										? e.uIdx * 65
										: 0}ms"
								stroke={darkMode ? '#334155' : '#cbd5e1'}
								stroke-width="1.5"
								d={e.d}
							/>
						{/each}
					</g>

					<!-- Root Fork Junctions (F & S) -->
					<g>
						{#each scene.fNodes as pt, i (`f-${i}`)}
							<circle
								cx={pt.x}
								cy={pt.y}
								r="3.8"
								class="bg-node {stage >= 1 ? 'opacity-100' : 'opacity-0'}"
								fill={stage === 4
									? scene.fActive[i]
										? darkMode
											? '#4ade80'
											: '#15803d'
										: darkMode
											? '#fbbf24'
											: '#d97706'
									: stage >= 2
										? darkMode
											? '#60a5fa'
											: '#2563eb'
										: darkMode
											? '#64748b'
											: '#94a3b8'}
							/>
						{/each}
						{#each scene.sNodes as pt, k (`s-${k}`)}
							<circle
								cx={pt.x}
								cy={pt.y}
								r="3.5"
								class="bg-node {stage >= 1 ? 'opacity-100' : 'opacity-0'}"
								fill={stage === 4
									? scene.sMatched[k]
										? darkMode
											? '#4ade80'
											: '#15803d'
										: darkMode
											? '#475569'
											: '#cbd5e1'
									: stage >= 2
										? darkMode
											? '#60a5fa'
											: '#2563eb'
										: darkMode
											? '#64748b'
											: '#94a3b8'}
							/>
						{/each}
					</g>

					<!-- Outer Bipartite Nodes (U left, V right) -->
					<g>
						{#each scene.uNodes as pt, i (`u-${i}`)}
							{@const matched = stage === 4 && scene.uMatched[i]}
							{@const warn = stage === 4 && scene.uUnplaced[i]}
							<circle
								cx={pt.x}
								cy={pt.y}
								r="7.5"
								class="bg-node {stage >= 1 ? 'opacity-100' : 'opacity-0'}"
								fill={matched
									? darkMode
										? '#064e3b'
										: '#dcfce7'
									: warn
										? darkMode
											? '#451a03'
											: '#fef3c7'
										: stage >= 2
											? darkMode
												? '#1e3a5f'
												: '#eff6ff'
											: darkMode
												? '#1e293b'
												: '#ffffff'}
								stroke={matched
									? darkMode
										? '#4ade80'
										: '#15803d'
									: warn
										? darkMode
											? '#fbbf24'
											: '#d97706'
										: stage >= 2
											? darkMode
												? '#60a5fa'
												: '#2563eb'
											: darkMode
												? '#cbd5e1'
												: '#131b2e'}
								stroke-width="2.4"
							/>
						{/each}

						{#each scene.vNodes as pt, j (`v-${j}`)}
							{@const matched = stage === 4 && scene.vMatched[j]}
							<circle
								cx={pt.x}
								cy={pt.y}
								r="7.5"
								class="bg-node {stage >= 1 ? 'opacity-100' : 'opacity-0'}"
								fill={matched
									? darkMode
										? '#064e3b'
										: '#dcfce7'
									: stage >= 2
										? darkMode
											? '#1e3a5f'
											: '#eff6ff'
										: darkMode
											? '#1e293b'
											: '#ffffff'}
								stroke={matched
									? darkMode
										? '#4ade80'
										: '#15803d'
									: stage >= 2
										? darkMode
											? '#60a5fa'
											: '#2563eb'
										: darkMode
											? '#94a3b8'
											: '#515f74'}
								stroke-width="2.4"
							/>
						{/each}
					</g>
				</svg>
			</div>

			<!-- Minimal Progress Bar & Compact Stage Pill -->
			<div class="px-4 sm:px-5 py-3 bg-white border-t border-[#c6c6cd]/30 flex items-center gap-3">
				<div class="flex-1 h-2 bg-[#eff4ff] rounded-full overflow-hidden">
					<div
						class="h-full transition-all duration-200 rounded-full {stage === 4 ? 'bg-[#15803d]' : 'bg-[#131b2e]'}"
						style="width: {progressPct}%"
					></div>
				</div>
				<span
					class="px-2.5 py-0.5 rounded-full text-[11px] font-semibold shrink-0 {stage === 4
						? 'bg-[#f0fdf4] text-[#15803d]'
						: stage === 3
							? 'bg-[#fffbeb] text-[#b45309]'
							: 'bg-[#e5eeff] text-[#003ea8]'}"
				>
					{stagePill}
				</span>
			</div>
		</div>
	</div>
{/if}
