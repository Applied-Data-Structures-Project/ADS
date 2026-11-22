<script lang="ts">
	import { formatShortSeatLabel } from '#lib/admin/matchingFilters.ts';
	import type { AdminApplicationDetail, AdminPlacementRow, AuditStatus } from '#lib/admin/types.ts';

	interface Props {
		student: AdminApplicationDetail | null;
		loading?: boolean;
		saving?: boolean;
		placement?: AdminPlacementRow | null;
		backLabel?: string | null;
		onClose: () => void;
		onBack?: () => void;
		onInspectPartner?: (slug: string) => void;
		onSaveReview: (
			email: string,
			payload: {
				status?: AuditStatus;
				goodStanding?: boolean | null;
				statementRating?: number | null;
				notes?: string;
			}
		) => void;
	}

	let {
		student,
		loading = false,
		saving = false,
		placement = null,
		backLabel = null,
		onClose,
		onBack,
		onInspectPartner,
		onSaveReview
	}: Props = $props();

	let ratingEnabled = $state(false);
	let ratingValue = $state(8.5);
	let notesDraft = $state('');
	let standingDraft = $state<'unknown' | 'good' | 'hold'>('unknown');
	let mobileTab = $state<'audit' | 'essays'>('audit');
	let lastLoadedEmail = '';

	$effect(() => {
		if (student) {
			ratingEnabled = student.hasStatementRating && student.statementRating !== null;
			ratingValue = student.statementRating ?? 8.0;
			notesDraft = student.adminNotes ?? '';
			standingDraft =
				student.goodStanding === true ? 'good' : student.goodStanding === false ? 'hold' : 'unknown';
			if (student.email !== lastLoadedEmail) {
				lastLoadedEmail = student.email;
				mobileTab = 'audit';
			}
		} else {
			lastLoadedEmail = '';
		}
	});

	const DOC_LABELS: { key: string; label: string }[] = [
		{ key: 'courses', label: 'Course List' },
		{ key: 'transcript', label: 'AUC Transcript' },
		{ key: 'cv', label: 'CV / Resume' },
		{ key: 'passport', label: 'Passport Copy' }
	];

	function initialsOf(name: string): string {
		return (
			name
				.split(' ')
				.filter(Boolean)
				.slice(0, 2)
				.map((w) => w[0])
				.join('')
				.toUpperCase() || 'AU'
		);
	}

	function submitUpdate(nextStatus?: AuditStatus) {
		if (!student) return;
		onSaveReview(student.email, {
			status: nextStatus ?? student.status,
			goodStanding: standingDraft === 'unknown' ? null : standingDraft === 'good',
			statementRating: ratingEnabled ? Number(ratingValue.toFixed(1)) : null,
			notes: notesDraft
		});
	}

	function statusBadgeClass(status: string): string {
		if (status === 'Verified') return 'bg-[#f0fdf4] text-[#15803d]';
		if (status === 'Ineligible') return 'bg-[#fef2f2] text-[#ba1a1a]';
		if (status === 'Draft') return 'bg-[#eff4ff] text-[#515f74]';
		return 'bg-[#fffbeb] text-[#b45309]';
	}
</script>

<svelte:window
	onkeydown={(e) => {
		if (e.key === 'Escape' && (student || loading)) onClose();
	}}
/>

{#if student || loading}
	<div
		class="fixed inset-0 z-50 flex items-center justify-center p-2.5 sm:p-5"
		role="dialog"
		aria-modal="true"
		aria-label="Student Application Audit Review"
	>
		<button
			type="button"
			aria-label="Close student review modal"
			onclick={onClose}
			class="fixed inset-0 bg-[#0b1c30]/45 backdrop-blur-[3px]"
		></button>

		<div class="modal-pop relative z-10 w-full max-w-4xl max-h-[92vh] bg-white border border-[#c6c6cd]/60 rounded-2xl shadow-2xl flex flex-col overflow-hidden">
			{#if backLabel && onBack}
				<div class="px-5 py-2 bg-[#e5eeff] border-b border-[#c6c6cd]/40 flex items-center justify-between gap-2">
					<button
						type="button"
						onclick={onBack}
						class="inline-flex items-center gap-1.5 text-xs font-semibold text-[#003ea8] hover:underline whitespace-nowrap shrink-0 cursor-pointer"
					>
						<span>&larr;</span>
						<span>Back to {backLabel}</span>
					</button>
					<span class="text-xs text-[#515f74] hidden sm:inline font-mono-num">Alt + &larr;</span>
				</div>
			{/if}

			<!-- Hero Header -->
			<div class="px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/40 flex items-start justify-between gap-3">
				<div class="flex items-center gap-3.5 min-w-0">
					{#if loading && !student}
						<div class="w-11 h-11 rounded-2xl bg-[#131b2e] text-white font-bold text-sm flex items-center justify-center shrink-0 shadow-sm">
							…
						</div>
						<span class="text-sm font-semibold text-[#515f74]">Loading application dossier…</span>
					{:else if student}
						<div class="w-11 h-11 rounded-2xl bg-[#131b2e] text-white font-bold text-sm flex items-center justify-center shrink-0 shadow-sm">
							{initialsOf(student.name)}
						</div>
						<div class="min-w-0">
							<div class="flex items-center gap-2 flex-wrap">
								<h2 class="text-lg sm:text-xl font-bold text-[#0b1c30] truncate">{student.name}</h2>
								<span class="px-2.5 py-0.5 rounded-full text-xs font-semibold {statusBadgeClass(student.status)}">
									{student.status}
								</span>
								{#if student.isDemo}
									<span class="px-2 py-0.5 rounded-full bg-[#e5eeff] text-[#003ea8] text-xs font-semibold">
										Demo
									</span>
								{/if}
							</div>
							<p class="font-mono-num text-xs text-[#515f74] truncate mt-0.5">
								#{student.aucId || 'No ID'} · {student.email}
							</p>
						</div>
					{/if}
				</div>

				<button
					type="button"
					onclick={onClose}
					aria-label="Close modal"
					class="h-9 w-9 rounded-xl border border-[#c6c6cd]/50 bg-white text-[#515f74] hover:text-[#0b1c30] shadow-sm flex items-center justify-center cursor-pointer shrink-0 text-lg"
				>
					&times;
				</button>
			</div>

			{#if student}
				<!-- Mobile Internal Tabs -->
				<div class="lg:hidden px-4 py-2 bg-white border-b border-[#c6c6cd]/30 flex gap-1.5">
					<button
						type="button"
						onclick={() => (mobileTab = 'audit')}
						class="flex-1 py-2 rounded-xl text-xs font-semibold cursor-pointer transition {mobileTab === 'audit'
							? 'bg-[#131b2e] text-white shadow-sm'
							: 'bg-[#eff4ff] text-[#515f74]'}"
					>
						Audit &amp; Rating
					</button>
					<button
						type="button"
						onclick={() => (mobileTab = 'essays')}
						class="flex-1 py-2 rounded-xl text-xs font-semibold cursor-pointer transition {mobileTab === 'essays'
							? 'bg-[#131b2e] text-white shadow-sm'
							: 'bg-[#eff4ff] text-[#515f74]'}"
					>
						Profile, Docs &amp; Essays
					</button>
				</div>

				<!-- Body -->
				<div class="p-4 sm:p-5 overflow-y-auto space-y-4 flex-1 bg-[#f8f9ff]">
					{#if student.eligibilityReason}
						<div class="p-3.5 rounded-xl bg-[#fef2f2] border border-[#c6c6cd]/40 text-xs sm:text-sm font-medium text-[#ba1a1a]">
							<strong>Eligibility Flag:</strong> {student.eligibilityReason}
						</div>
					{/if}

					{#if placement}
						<div class="p-3.5 rounded-xl bg-[#f0fdf4] border border-[#c6c6cd]/40 flex flex-wrap items-center justify-between gap-2 text-xs sm:text-sm text-[#15803d]">
							<div class="min-w-0">
								<strong>Current Simulation Placement:</strong>
								{#if onInspectPartner}
									<button
										type="button"
										onclick={() => onInspectPartner(placement.universitySlug)}
										class="font-bold underline hover:opacity-80 cursor-pointer ml-1"
									>
										{placement.universityName}
									</button>
								{:else}
									<span class="font-bold ml-1">{placement.universityName}</span>
								{/if}
								<span title={placement.seatLabel} class="font-mono-num ml-1">
									(Seat {formatShortSeatLabel(placement.seatLabel)}) · {placement.rankLabel}
								</span>
							</div>
							<span class="font-mono-num font-bold shrink-0">Fit {placement.weight.toFixed(3)}</span>
						</div>
					{/if}

					<div class="grid grid-cols-1 lg:grid-cols-12 gap-5 items-start">
						<!-- LEFT COLUMN: Profile, Preferences, Documents & Essays -->
						<div class="lg:col-span-7 space-y-4 {mobileTab === 'essays' ? 'block' : 'hidden lg:block'}">
							<!-- Academic Snapshot -->
							<div class="grid grid-cols-2 sm:grid-cols-3 gap-3">
								<div class="p-3.5 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm">
									<span class="text-xs font-semibold uppercase tracking-wider text-[#515f74] block">Cumulative GPA</span>
									<span class="text-lg font-bold font-mono-num mt-0.5 block {student.gpa < 2.8 ? 'text-[#ba1a1a]' : 'text-[#0b1c30]'}">
										{student.gpa.toFixed(2)} / 4.00
									</span>
									<span class="text-xs text-[#515f74] font-mono-num">
										{student.credits} cr · {student.majorDeclared ? 'Declared' : 'Undeclared'}
									</span>
								</div>

								<div class="p-3.5 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm">
									<span class="text-xs font-semibold uppercase tracking-wider text-[#515f74] block">Standing &amp; Major</span>
									<span class="text-sm font-bold text-[#0b1c30] mt-1 block">{student.level} · {student.standing}</span>
									<span class="text-xs text-[#515f74] truncate block">{student.major || 'Undeclared'}</span>
								</div>

								<div class="p-3.5 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm col-span-2 sm:col-span-1">
									<span class="text-xs font-semibold uppercase tracking-wider text-[#515f74] block">Budget &amp; Equity</span>
									<span class="text-sm font-bold font-mono-num text-[#0b1c30] mt-1 block">
										${student.budget.toLocaleString()}/sem
									</span>
									<span class="text-xs text-[#515f74] block">
										{student.prior ? 'Prior Study Abroad' : 'First-Time (+Bonus)'}
									</span>
								</div>
							</div>

							<!-- Ranked Preferences -->
							<div class="p-4 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm space-y-2.5">
								<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
									Ranked Top 3 Preferences
								</div>
								{#each [0, 1, 2] as idx (idx)}
									{#if onInspectPartner && student.prefSlugs[idx]}
										<button
											type="button"
											title={`${student.prefNames[idx] || student.prefSlugs[idx]} (${student.prefSlugs[idx]})`}
											onclick={() => onInspectPartner(student.prefSlugs[idx])}
											class="w-full flex items-center justify-between px-3.5 py-2.5 bg-[#eff4ff] hover:bg-[#dce9ff] rounded-xl text-xs sm:text-sm text-left transition cursor-pointer gap-3 overflow-hidden"
										>
											<div class="min-w-0 flex-1">
												<div class="font-semibold text-[#0b1c30] truncate">
													<span class="font-mono-num text-[#003ea8] mr-1.5">#{idx + 1}</span>
													{student.prefNames[idx] || student.prefSlugs[idx]}
												</div>
												<div class="font-mono-num text-xs text-[#515f74] truncate mt-0.5">
													{student.prefSlugs[idx]}
												</div>
											</div>
											<span class="text-xs font-semibold text-[#003ea8] whitespace-nowrap shrink-0">
												Dossier &#8599;
											</span>
										</button>
									{:else}
										<div class="w-full flex items-center px-3.5 py-2.5 bg-[#eff4ff] rounded-xl text-xs sm:text-sm gap-2 overflow-hidden">
											<span class="min-w-0 flex-1 truncate text-[#0b1c30]">
												<strong class="font-mono-num mr-1.5">#{idx + 1}</strong>
												{student.prefNames[idx] || 'Not chosen'}
											</span>
										</div>
									{/if}
								{/each}
							</div>

							<!-- Documents & Recommendations -->
							<div class="grid grid-cols-1 sm:grid-cols-2 gap-3">
								<div class="p-4 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm space-y-2.5">
									<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
										Uploaded PDFs ({student.uploadedDocs.length}/4)
									</div>
									<div class="flex flex-wrap gap-1.5">
										{#each DOC_LABELS as doc (doc.key)}
											{@const has = student.uploadedDocs.includes(doc.key)}
											<span
												class="px-2.5 py-1 rounded-full text-xs font-medium {has
													? 'bg-[#f0fdf4] text-[#15803d]'
													: 'bg-[#fef2f2] text-[#ba1a1a]'}"
											>
												{has ? '✓' : '×'} {doc.label}
											</span>
										{/each}
									</div>
								</div>

								<div class="p-4 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm space-y-2">
									<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
										Faculty Recommendations
									</div>
									<div class="text-xs sm:text-sm text-[#0b1c30]">
										<span class="font-semibold text-[#15803d]">{student.letterReceivedCount} received</span> ·
										<span class="font-medium text-[#b45309]">{student.letterPendingCount} pending</span>
										{#if student.letterDeclinedCount > 0}
											· <span class="text-[#ba1a1a]">{student.letterDeclinedCount} declined</span>
										{/if}
									</div>
									<div class="text-xs text-[#515f74]">
										Program: {student.programType || 'Exchange'} · Scholarship: {student.scholarship || 'None'}
									</div>
								</div>
							</div>

							<!-- Personal Statement & Questionnaire -->
							<div class="p-4 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm space-y-3">
								<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
									Personal Statement &amp; Questionnaire
								</div>
								{#if student.qGoals}
									<div class="p-3.5 bg-[#eff4ff] rounded-xl space-y-1">
										<div class="text-xs font-bold text-[#0b1c30]">Academic Goals Statement</div>
										<p class="text-xs sm:text-sm text-[#0b1c30] leading-relaxed whitespace-pre-line">{student.qGoals}</p>
									</div>
								{/if}
								{#if student.qAdaptability}
									<div class="p-3.5 border border-[#c6c6cd]/40 rounded-xl space-y-1">
										<div class="text-xs font-bold text-[#0b1c30]">Cultural Adaptability</div>
										<p class="text-xs sm:text-sm text-[#515f74] leading-relaxed">{student.qAdaptability}</p>
									</div>
								{/if}
								{#if student.qPreparedness || student.qFinance || student.qTeamwork || student.qReflection}
									<div class="grid grid-cols-1 sm:grid-cols-2 gap-2.5 text-xs">
										{#if student.qPreparedness}
											<div class="p-3 rounded-xl border border-[#c6c6cd]/30">
												<strong class="text-[#0b1c30] block mb-0.5">Preparedness</strong>
												<span class="text-[#515f74]">{student.qPreparedness}</span>
											</div>
										{/if}
										{#if student.qFinance}
											<div class="p-3 rounded-xl border border-[#c6c6cd]/30">
												<strong class="text-[#0b1c30] block mb-0.5">Finance Plan</strong>
												<span class="text-[#515f74]">{student.qFinance}</span>
											</div>
										{/if}
										{#if student.qTeamwork}
											<div class="p-3 rounded-xl border border-[#c6c6cd]/30">
												<strong class="text-[#0b1c30] block mb-0.5">Teamwork</strong>
												<span class="text-[#515f74]">{student.qTeamwork}</span>
											</div>
										{/if}
										{#if student.qReflection}
											<div class="p-3 rounded-xl border border-[#c6c6cd]/30">
												<strong class="text-[#0b1c30] block mb-0.5">Reflection</strong>
												<span class="text-[#515f74]">{student.qReflection}</span>
											</div>
										{/if}
									</div>
								{/if}
							</div>
						</div>

						<!-- RIGHT COLUMN: Sticky IPSO Statement Rating & Audit Panel -->
						<div class="lg:col-span-5 lg:sticky lg:top-0 p-4 rounded-2xl bg-white border border-[#c6c6cd]/50 shadow-sm space-y-4 {mobileTab === 'audit' ? 'block' : 'hidden lg:block'}">
							<div class="flex items-center justify-between gap-2">
								<span class="text-xs font-bold uppercase tracking-wider text-[#0b1c30]">
									IPSO Audit &amp; Statement Score
								</span>
								<span class="px-2.5 py-0.5 rounded-full font-mono-num text-xs font-bold bg-[#e5eeff] text-[#003ea8] whitespace-nowrap shrink-0">
									{ratingEnabled ? `${ratingValue.toFixed(1)} / 10` : 'Unrated'}
								</span>
							</div>

							<label class="inline-flex items-center gap-2 text-xs sm:text-sm font-semibold text-[#0b1c30] cursor-pointer">
								<input type="checkbox" bind:checked={ratingEnabled} class="rounded w-4 h-4" />
								<span>Include reviewer statement score in fit calculation</span>
							</label>

							<div class="space-y-2.5">
								{#if ratingEnabled}
									<input
										type="range"
										min="0"
										max="10"
										step="0.1"
										bind:value={ratingValue}
										class="w-full h-1.5 cursor-pointer"
									/>
								{/if}
								<div class="grid grid-cols-4 gap-1.5">
									{#each [6.0, 7.5, 8.5, 9.5] as preset (preset)}
										<button
											type="button"
											onclick={() => {
												ratingEnabled = true;
												ratingValue = preset;
											}}
											class="py-1.5 rounded-lg text-xs font-mono-num font-semibold cursor-pointer {ratingEnabled &&
											Math.abs(ratingValue - preset) < 0.05
												? 'bg-[#131b2e] text-white'
												: 'bg-[#eff4ff] text-[#515f74] hover:bg-[#dce9ff]'}"
										>
											{preset.toFixed(1)}
										</button>
									{/each}
								</div>
							</div>

							<!-- Segmented Good Standing Verification -->
							<div class="space-y-1.5">
								<span class="text-xs font-semibold text-[#515f74] block">AUC Good Standing Verification</span>
								<div class="grid grid-cols-3 gap-1 p-1 rounded-xl bg-[#d3e4fe]/40 text-xs font-semibold">
									{#each [['unknown', 'Unchecked'], ['good', '✓ Good'], ['hold', '⚠ Hold']] as [val, label] (val)}
										<button
											type="button"
											onclick={() => (standingDraft = val as 'unknown' | 'good' | 'hold')}
											class="py-2 rounded-lg cursor-pointer transition {standingDraft === val
												? 'bg-white text-[#0b1c30] shadow-sm font-bold'
												: 'text-[#515f74] hover:text-[#0b1c30]'}"
										>
											{label}
										</button>
									{/each}
								</div>
							</div>

							<!-- Reviewer Notes -->
							<div class="space-y-1.5">
								<label for="audit-notes" class="text-xs font-semibold text-[#515f74] block">
									IPSO Review Notes
								</label>
								<textarea
									id="audit-notes"
									rows="4"
									bind:value={notesDraft}
									placeholder="Reviewer notes, missing doc reminders, or interview observations..."
									class="w-full p-3 bg-white text-[#0b1c30] text-xs sm:text-sm rounded-xl border border-[#c6c6cd]/50 focus:outline-none focus:border-[#003ea8]"
								></textarea>
							</div>
						</div>
					</div>
				</div>

				<!-- Footer Actions -->
				<div class="px-5 py-3.5 border-t border-[#c6c6cd]/40 bg-[#eff4ff] flex flex-wrap justify-between items-center gap-2">
					<button
						type="button"
						disabled={saving}
						onclick={() => submitUpdate('Ineligible')}
						class="h-10 px-4 bg-[#fef2f2] hover:bg-[#fee2e2] text-[#ba1a1a] rounded-xl text-xs sm:text-sm font-semibold whitespace-nowrap cursor-pointer disabled:opacity-50"
					>
						Flag Ineligible
					</button>
					<div class="flex items-center gap-2 flex-wrap">
						<button
							type="button"
							disabled={saving}
							onclick={() => submitUpdate()}
							class="h-10 px-4 bg-white hover:bg-[#e5eeff] border border-[#c6c6cd]/50 text-[#0b1c30] rounded-xl text-xs sm:text-sm font-semibold whitespace-nowrap shadow-sm cursor-pointer disabled:opacity-50"
						>
							Save Rating / Notes
						</button>
						<button
							type="button"
							disabled={saving}
							onclick={() => submitUpdate('Verified')}
							class="h-10 px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white rounded-xl text-xs sm:text-sm font-semibold whitespace-nowrap shadow-sm cursor-pointer disabled:opacity-50"
						>
							{saving ? 'Saving…' : 'Mark Verified'}
						</button>
					</div>
				</div>
			{/if}
		</div>
	</div>
{/if}
