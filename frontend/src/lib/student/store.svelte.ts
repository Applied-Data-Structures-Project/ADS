// State for Group 3's pages (application, status, letters, doctor).
//
// Right now each action works on mock data so the pages work before the backend does.
// Each action has a TODO naming the backend route that replaces it. When the route exists,
// call it with `api` from '#lib/api/client.ts' and keep the same function name, so the pages don't change.
//
// The logged-in student and the saved list belong to G2 (Muhammad). The saved,
// compare and notification state is frontend-only for now (see discovery.svelte.ts);
// the application and letters below are still G3's mock-backed pages.

import {
	DEMO_APPLICATION,
	DEMO_DOCS,
	DEMO_DOCTOR,
	DEMO_LETTERS,
	DEMO_STUDENT,
	DOCUMENTS,
	RULES
} from './mocks.ts';
import { catalog } from '../explore/catalog.svelte.ts';
import { discovery } from './discovery.svelte.ts';
import {
	evaluateEligibility,
	readEligibility,
	writeEligibility,
	type EligibilityProfile
} from './eligibility.ts';
import type {
	Application,
	DocType,
	LetterRequest,
	StudentProfile,
	UploadedDoc
} from './types.ts';

// ---------------------------------------------------------------------------
// Small helpers used by several pages
// ---------------------------------------------------------------------------

/** Number of words in a text (for the essay limits). */
export function wordCount(text: string): number {
	const t = text.trim();
	return t ? t.split(/\s+/).length : 0;
}

function today(): string {
	return new Date().toISOString().slice(0, 10);
}

/** One thing still needed before submit, and the /apply step that fixes it. */
export interface MissingItem {
	step: number;
	label: string;
}

const DARK_KEY = 'auc-student-dark-mode';
const budgetKey = (accountId: string) => `auc-budget-v1:${accountId}`;

class StudentStore {
	// ---- From G2 (read-only here) ------------------------------------------
	/** TODO: replace with the logged-in user from Muhammad's GET /api/me. */
	user = $state<StudentProfile | null>({ ...DEMO_STUDENT });
	doctor = $state<StudentProfile>({ ...DEMO_DOCTOR });

	// ---- Application + letters (G3) -----------------------------------------
	application = $state<Application>({
		...DEMO_APPLICATION,
		pref: [...DEMO_APPLICATION.pref] as [string, string, string]
	});
	docs = $state<Record<DocType, UploadedDoc | null>>({ ...DEMO_DOCS });
	letters = $state<LetterRequest[]>(DEMO_LETTERS.map((l) => ({ ...l })));

	/** The academic record the student enters on /profile. Null until they save it. */
	eligibilityProfile = $state<EligibilityProfile | null>(null);

	// ---- UI ----------------------------------------------------------------
	toast = $state<string | null>(null);
	darkMode = $state(false);
	private toastTimer: ReturnType<typeof setTimeout> | null = null;

	// ---- Derived values ------------------------------------------------------
	/** The saved schools, shared with the Explorer, Saved and Compare pages. */
	saved = $derived(discovery.saved);

	savedCount = $derived(discovery.savedSlugs.length);

	/** The rules IPSO checks, derived from the profile saved on /profile. */
	eligibilitySummary = $derived(
		this.eligibilityProfile ? evaluateEligibility(this.eligibilityProfile) : null
	);
	eligibility = $derived(this.eligibilitySummary?.checks ?? []);
	isEligible = $derived(this.eligibilitySummary?.eligible ?? false);
	eligibilityProvisional = $derived(this.eligibilitySummary?.provisional ?? false);

	/** False until the student saves the eligibility profile once. */
	hasEligibilityProfile = $derived(this.eligibilityProfile !== null);

	/** Error for each of the 3 preference slots, or null if that slot is fine. */
	prefErrors = $derived.by((): (string | null)[] => {
		const level = this.user?.level ?? 'UG';
		const pref = this.application.pref;
		return pref.map((slug, i) => {
			if (!slug) return 'Choose a university';
			const first = pref.indexOf(slug);
			if (first !== i) return `Already your #${first + 1} choice`;
			if (!discovery.savedSlugs.includes(slug)) return 'Not in your saved list';
			// The catalog loads once per visit; do not report false errors meanwhile.
			if (catalog.status !== 'ready') return null;
			const record = catalog.bySlug(slug);
			if (!record) return 'Not in the current university list';
			const levels = record.agreement_student_levels.value ?? [];
			if (levels.length > 0 && !levels.includes(level)) return `Doesn't accept ${level} students`;
			return null;
		});
	});

	myLetters = $derived(this.letters.filter((l) => l.studentEmail === this.user?.email));
	hasLetter = $derived(this.myLetters.some((l) => l.status === 'Received'));

	/** The doctor's waiting requests, oldest first (a queue). */
	doctorQueue = $derived(
		this.letters
			.filter((l) => l.doctorEmail === this.doctor.email && l.status === 'Pending')
			.sort((a, b) => a.requestedAt.localeCompare(b.requestedAt) || a.id - b.id)
	);
	doctorHistory = $derived(
		this.letters.filter((l) => l.doctorEmail === this.doctor.email && l.status !== 'Pending')
	);

	locked = $derived(this.application.status !== 'Draft');
	deadlinePassed = $derived(Date.now() > Date.parse(RULES.deadline));

	/** Which of the 7 /apply steps are complete. Index 0 = step 1. */
	stepsDone = $derived.by((): boolean[] => {
		const a = this.application;
		const personal = [
			a.firstName,
			a.surname,
			a.dateOfBirth,
			a.nationality,
			a.gender,
			a.phone,
			a.aucId,
			a.major,
			a.rhet1020,
			a.expectedGraduation,
			a.scholarship
		].every((v) => v !== '');
		const program = a.programType !== '' && this.prefErrors.every((e) => e === null);
		const guardian = [a.guardianName, a.guardianRelation, a.guardianPhone, a.guardianEmail].every(
			(v) => v.trim() !== ''
		);
		const goals = wordCount(a.qGoals);
		const adapt = wordCount(a.qAdaptability);
		const questions =
			a.travelExperience !== '' &&
			a.qPreparedness.trim() !== '' &&
			a.qTeamwork.trim() !== '' &&
			a.qReflection.trim() !== '' &&
			a.qFinance.trim() !== '' &&
			goals >= RULES.goalsWords.min &&
			goals <= RULES.goalsWords.max &&
			adapt >= RULES.adaptabilityWords.min &&
			adapt <= RULES.adaptabilityWords.max;
		const documents = DOCUMENTS.every((d) => this.docs[d.type] !== null) && this.hasLetter;
		return [this.isEligible, personal, program, guardian, questions, documents, a.declaration];
	});

	progress = $derived(Math.round((this.stepsDone.filter(Boolean).length / 7) * 100));

	/** Everything still needed before Submit. Empty list = ready. */
	missing = $derived.by((): MissingItem[] => {
		const out: MissingItem[] = [];
		const a = this.application;
		if (!this.isEligible) out.push({ step: 1, label: 'Meet the eligibility rules' });
		if (!this.stepsDone[1]) out.push({ step: 2, label: 'Complete your personal information' });
		if (a.programType === '') out.push({ step: 3, label: 'Choose Exchange or Direct Enrollment' });
		this.prefErrors.forEach((e, i) => {
			if (e) out.push({ step: 3, label: `Preference #${i + 1}: ${e.toLowerCase()}` });
		});
		if (!this.stepsDone[3]) out.push({ step: 4, label: 'Add your parent or guardian' });
		const goals = wordCount(a.qGoals);
		if (goals < RULES.goalsWords.min || goals > RULES.goalsWords.max)
			out.push({
				step: 5,
				label: `Academic goals: ${goals} of ${RULES.goalsWords.min}–${RULES.goalsWords.max} words`
			});
		const adapt = wordCount(a.qAdaptability);
		if (adapt < RULES.adaptabilityWords.min || adapt > RULES.adaptabilityWords.max)
			out.push({ step: 5, label: `Adaptability: ${adapt} of at least ${RULES.adaptabilityWords.min} words` });
		const blanks = [
			[a.qPreparedness, 'Academic preparedness'],
			[a.qTeamwork, 'Teamwork'],
			[a.qReflection, 'What you hope to gain'],
			[a.qFinance, 'Financial plan']
		].filter(([v]) => v.trim() === '');
		for (const [, label] of blanks) out.push({ step: 5, label: `Answer “${label}”` });
		if (a.travelExperience === '') out.push({ step: 5, label: 'Answer the travel experience question' });
		for (const d of DOCUMENTS) {
			if (!this.docs[d.type]) out.push({ step: 6, label: `Upload ${d.label}` });
		}
		if (!this.hasLetter) out.push({ step: 6, label: 'Receive a recommendation letter' });
		if (!a.declaration) out.push({ step: 7, label: 'Tick the declaration' });
		return out;
	});

	// ---- UI actions ----------------------------------------------------------
	showToast(message: string) {
		this.toast = message;
		if (this.toastTimer) clearTimeout(this.toastTimer);
		this.toastTimer = setTimeout(() => (this.toast = null), 2800);
	}

	/** Call once in the browser (onMount). */
	loadDarkMode() {
		try {
			this.darkMode = localStorage.getItem(DARK_KEY) === '1';
		} catch {
			this.darkMode = false;
		}
	}

	toggleDarkMode() {
		this.darkMode = !this.darkMode;
		try {
			localStorage.setItem(DARK_KEY, this.darkMode ? '1' : '0');
		} catch {
			// Storage blocked: the choice only lasts for this visit.
		}
	}

	// ---- Eligibility profile -------------------------------------------------

	/** Load the profile saved for this account, if any. Call once in the browser. */
	loadEligibility(accountId: string) {
		this.eligibilityProfile = readEligibility(accountId);
		if (this.eligibilityProfile) this.applyEligibilityToUser(this.eligibilityProfile);
	}

	/** Save the profile and make its academic fields the signed-in student's record. */
	saveEligibility(accountId: string, profile: EligibilityProfile) {
		writeEligibility(accountId, profile);
		this.eligibilityProfile = profile;
		this.applyEligibilityToUser(profile);
	}

	private applyEligibilityToUser(profile: EligibilityProfile) {
		const base = this.user ?? DEMO_STUDENT;
		this.user = {
			...base,
			level: profile.level === 'GR' ? 'GR' : 'UG',
			major: profile.major,
			declared: profile.majorDeclared,
			credits: profile.credits ?? 0,
			gpa: profile.gpa ?? 0
		};
	}

	// ---- Budget (used by Compare) --------------------------------------------

	/** Load the budget saved for this account, if any. Call once in the browser. */
	loadBudget(accountId: string) {
		if (!accountId || typeof localStorage === 'undefined') return;
		try {
			const raw = localStorage.getItem(budgetKey(accountId));
			if (raw === null) return;
			const amount = Number(raw);
			if (Number.isFinite(amount) && amount >= 0) this.application.budget = amount;
		} catch {
			// Storage blocked: keep the demo budget for this visit.
		}
	}

	/** Save the per-semester budget for this account. */
	saveBudget(accountId: string, amount: number) {
		const safe = Number.isFinite(amount) ? Math.max(0, Math.round(amount)) : 0;
		this.application.budget = safe;
		if (accountId && typeof localStorage !== 'undefined') {
			try {
				localStorage.setItem(budgetKey(accountId), String(safe));
			} catch {
				// Storage blocked: the value still lasts for this visit.
			}
		}
		this.showToast('Budget saved.');
	}

	/** Copy the first three saved schools into the application preference slots. */
	useSavedPreferences(slugs: readonly string[]): string[] {
		if (this.locked) return [];
		const unique = [...new Set(slugs.filter((slug) => slug !== ''))].slice(0, 3);
		this.application.pref = [unique[0] ?? '', unique[1] ?? '', unique[2] ?? ''];
		return unique;
	}

	// ---- G3: application -----------------------------------------------------

	/** TODO(G3): api.put('/api/application', this.application). Refused by the server after submit. */
	saveDraft(silent = false) {
		if (this.locked) return;
		this.application.prior = this.application.travelExperience === 'Yes, Study Abroad';
		this.application.updatedAt = new Date().toISOString();
		if (!silent) this.showToast('Draft saved.');
	}

	/** TODO(G3): api.put('/api/application/preferences', { pref }). */
	setPref(slot: number, slug: string) {
		if (this.locked) return;
		const next = [...this.application.pref] as [string, string, string];
		next[slot] = slug;
		this.application.pref = next;
	}

	/**
	 * Checks the file, then records it. Returns an error message or null.
	 * TODO(G3): api PUT /api/application/documents/<type> as multipart form data.
	 */
	upload(type: DocType, file: File): string | null {
		if (this.locked) return 'Your application is submitted, so files can no longer change.';
		const isPdf = file.type === 'application/pdf' || file.name.toLowerCase().endsWith('.pdf');
		if (!isPdf) return 'Only PDF files are accepted.';
		if (file.size > RULES.maxFileMb * 1024 * 1024) return `The file is larger than ${RULES.maxFileMb} MB.`;
		this.docs[type] = {
			docType: type,
			fileName: file.name,
			sizeKb: Math.max(1, Math.round(file.size / 1024)),
			uploadedAt: today()
		};
		this.showToast('File uploaded.');
		return null;
	}

	/** TODO(G3): api.del(`/api/application/documents/${type}`). */
	removeDoc(type: DocType) {
		if (this.locked) return;
		this.docs[type] = null;
	}

	/** TODO(G3): api.post('/api/application/submit'). The server re-checks everything. */
	submit(): boolean {
		if (this.locked) return false;
		if (this.deadlinePassed) {
			this.showToast('The deadline has passed. Late applications are not considered.');
			return false;
		}
		if (this.missing.length > 0) {
			this.showToast(`${this.missing.length} items are still missing.`);
			return false;
		}
		this.saveDraft(true);
		this.application.status = 'Submitted';
		this.application.submittedAt = new Date().toISOString();
		this.showToast('Application submitted. It can no longer be edited.');
		return true;
	}

	// ---- G3: recommendation letters --------------------------------------------

	/** TODO(G3): api.post('/api/letters', { doctorName, doctorEmail }). */
	requestLetter(doctorName: string, doctorEmail: string): string | null {
		const user = this.user;
		if (!user) return 'Log in first.';
		const email = doctorEmail.trim().toLowerCase();
		const active = this.myLetters.filter((l) => l.status !== 'Declined');
		if (doctorName.trim() === '') return 'Enter the faculty member’s name.';
		if (!email.endsWith('@aucegypt.edu')) return 'The letter must come from an AUC faculty member (@aucegypt.edu).';
		if (this.myLetters.some((l) => l.doctorEmail === email)) return 'You already asked this faculty member.';
		if (active.length >= RULES.maxLetters) return `You can have at most ${RULES.maxLetters} requests.`;
		const id = Math.max(0, ...this.letters.map((l) => l.id)) + 1;
		this.letters.push({
			id,
			studentEmail: user.email,
			studentName: user.name,
			studentMajor: user.major,
			studentGpa: user.gpa,
			doctorName: doctorName.trim(),
			doctorEmail: email,
			status: 'Pending',
			letterText: '',
			requestedAt: today(),
			answeredAt: ''
		});
		this.showToast(`Request sent to ${doctorName.trim()}.`);
		return null;
	}

	/** TODO(G3): api.post(`/api/doctor/letters/${id}/answer`, { text }). */
	answerLetter(id: number, text: string): string | null {
		if (text.trim().length < 20) return 'Write the letter before sending (at least a few sentences).';
		const l = this.letters.find((x) => x.id === id);
		if (!l) return 'Request not found.';
		l.status = 'Received';
		l.letterText = text.trim();
		l.answeredAt = today();
		this.showToast(`Letter sent for ${l.studentName}.`);
		return null;
	}

	/** TODO(G3): api.post(`/api/doctor/letters/${id}/decline`). */
	declineLetter(id: number) {
		const l = this.letters.find((x) => x.id === id);
		if (!l) return;
		l.status = 'Declined';
		l.answeredAt = today();
		this.showToast(`Declined the request from ${l.studentName}.`);
	}
}

/** One shared store for Group 3's pages. */
export const store = new StudentStore();
