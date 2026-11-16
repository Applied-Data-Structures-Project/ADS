// State for Group 3's pages (application, status, letters, doctor).
//
// Right now each action works on mock data so the pages work before the backend does.
// Each action has a TODO naming the backend route that replaces it. When the route exists,
// call it with `api` from '#lib/api/client.ts' and keep the same function name, so the pages don't change.
//
// The logged-in student and the saved list belong to G2 (Muhammad); here they are read-only mocks
// until his login and saved routes exist.

import {
	DEMO_APPLICATION,
	DEMO_DOCS,
	DEMO_DOCTOR,
	DEMO_LETTERS,
	DEMO_SAVED,
	DEMO_STUDENT,
	DOCUMENTS,
	RULES,
	UNIVERSITIES
} from './mocks.ts';
import type {
	Application,
	DocType,
	EligibilityCheck,
	LetterRequest,
	Level,
	StudentProfile,
	University,
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

export function universityById(id: number): University | undefined {
	return UNIVERSITIES.find((u) => u.id === id);
}

export function acceptsLevel(u: University, level: Level): boolean {
	return level === 'UG' ? u.ug : u.gr;
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

class StudentStore {
	// ---- From G2 (read-only here) ------------------------------------------
	/** TODO: replace with the logged-in user from Muhammad's GET /api/me. */
	user = $state<StudentProfile | null>({ ...DEMO_STUDENT });
	doctor = $state<StudentProfile>({ ...DEMO_DOCTOR });

	/** TODO: replace with Muhammad's GET /api/saved. The 3 preferences are picked from this list. */
	savedIds = $state<number[]>([...DEMO_SAVED]);

	// ---- Application + letters (G3) -----------------------------------------
	application = $state<Application>({
		...DEMO_APPLICATION,
		pref: [...DEMO_APPLICATION.pref] as [number, number, number]
	});
	docs = $state<Record<DocType, UploadedDoc | null>>({ ...DEMO_DOCS });
	letters = $state<LetterRequest[]>(DEMO_LETTERS.map((l) => ({ ...l })));

	// ---- UI ----------------------------------------------------------------
	toast = $state<string | null>(null);
	darkMode = $state(false);
	private toastTimer: ReturnType<typeof setTimeout> | null = null;

	// ---- Derived values ------------------------------------------------------
	saved = $derived(
		this.savedIds.map(universityById).filter((u): u is University => u !== undefined)
	);

	/** GET /api/application/eligibility — the 3 IPSO checks. */
	eligibility = $derived.by((): EligibilityCheck[] => {
		const u = this.user;
		if (!u) return [];
		const min = RULES.minGpa[u.level];
		return [
			{
				label: 'Officially declared major',
				ok: u.declared,
				detail: u.declared ? u.major : 'Undeclared students’ applications are disregarded.'
			},
			{
				label: `At least ${RULES.minCredits} credits`,
				ok: u.credits >= RULES.minCredits,
				detail: `You have ${u.credits} credits.`
			},
			{
				label: `GPA of ${min.toFixed(1)} or higher (${u.level})`,
				ok: u.gpa >= min,
				detail: `Your GPA is ${u.gpa.toFixed(2)}.`
			}
		];
	});
	isEligible = $derived(this.eligibility.length > 0 && this.eligibility.every((c) => c.ok));

	/** Error for each of the 3 preference slots, or null if that slot is fine. */
	prefErrors = $derived.by((): (string | null)[] => {
		const level = this.user?.level ?? 'UG';
		return this.application.pref.map((id, i) => {
			if (id < 0) return 'Choose a university';
			const first = this.application.pref.indexOf(id);
			if (first !== i) return `Already your #${first + 1} choice`;
			if (!this.savedIds.includes(id)) return 'Not in your saved list';
			const u = universityById(id);
			if (u && !acceptsLevel(u, level)) return `Doesn't accept ${level} students`;
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

	// ---- G3: application -----------------------------------------------------

	/** TODO(G3): api.put('/api/application', this.application). Refused by the server after submit. */
	saveDraft(silent = false) {
		if (this.locked) return;
		this.application.prior = this.application.travelExperience === 'Yes, Study Abroad';
		this.application.updatedAt = new Date().toISOString();
		if (!silent) this.showToast('Draft saved.');
	}

	/** TODO(G3): api.put('/api/application/preferences', { pref }). */
	setPref(slot: number, id: number) {
		if (this.locked) return;
		const next = [...this.application.pref] as [number, number, number];
		next[slot] = id;
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
