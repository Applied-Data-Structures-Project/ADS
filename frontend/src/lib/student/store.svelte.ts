// State for Group 3's pages (application, status, letters, doctor).
//
// The application, documents and letters live on the C++ server (Malak's HashMaps and Queues,
// saved in SQLite). Each action below calls one route from docs/api.md through callServer(),
// then updates this state so the pages redraw. The function names stayed the same as in the
// mock version, so the pages barely changed.
//
// The logged-in student and the saved list belong to G2 (Muhammad). Saved, compare and
// notification state is frontend-only for now (see discovery.svelte.ts); the application and
// letters above are server-backed.

import { DEMO_DOCTOR, DEMO_STUDENT, DOCUMENTS, RULES } from './mocks.ts';
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
import { callServer, errorText } from './api.ts';
import { auth } from '../auth/store.svelte.ts';
import { catalog } from '../explore/catalog.svelte.ts';
import { discovery } from './discovery.svelte.ts';

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

// ---------------------------------------------------------------------------
// Talking to the server: shapes and conversions
// ---------------------------------------------------------------------------

/** What GET/PUT /api/application and POST /api/application/submit answer. */
interface ServerState {
	application: Record<string, unknown> | null;
	documents: DocType[];
	letterReceived: boolean;
	missing: string[];
}

/** One letter request as the server sends it (doctor view adds the student's details). */
interface ServerLetter {
	id: number;
	studentEmail: string;
	doctorName: string;
	doctorEmail: string;
	status: LetterRequest['status'];
	requestedAt: string;
	answeredAt: string;
	studentName?: string;
	studentMajor?: string;
	studentGpa?: number;
}

/** What GET/PUT /api/me answers. */
interface ServerStudentProfile {
	email: string;
	name: string;
	aucId: string;
	phone: string;
	level: 'UG' | 'GR' | '';
	major: string;
	gpa: number | null;
	credits: number | null;
	majorDeclared: boolean;
	rhet110: boolean;
	rhet120: boolean;
	finalSemester: boolean;
	academicProbation: boolean;
	disciplinaryProbation: boolean;
	englishComplete: boolean;
	firstSemesterNoGpa: boolean;
	eligibilitySaved: boolean;
	budget: number;
	hasBudget: boolean;
	updatedAt: string;
}

interface ServerMe {
	profile: ServerStudentProfile | null;
	eligibility: {
		eligible: boolean;
		provisional: boolean;
		reason?: string;
	};
}

/** A blank application for a student who has not started yet. */
function emptyApplication(email: string): Application {
	return {
		studentEmail: email,
		firstName: '',
		surname: '',
		dateOfBirth: '',
		nationality: '',
		secondNationality: '',
		gender: '',
		phone: '',
		aucId: '',
		gpa: 0,
		level: 'UG',
		major: '',
		rhet1020: '',
		expectedGraduation: '',
		accommodation: false,
		scholarship: '',
		programType: '',
		pref: ['', '', ''],
		guardianName: '',
		guardianRelation: '',
		guardianPhone: '',
		guardianEmail: '',
		guardianLanguage: '',
		qPreparedness: '',
		qGoals: '',
		travelExperience: '',
		qAdaptability: '',
		qTeamwork: '',
		qReflection: '',
		qFinance: '',
		budget: 0,
		prior: false,
		declaration: false,
		status: 'Draft',
		goodStanding: null,
		submittedAt: '',
		updatedAt: ''
	};
}

/** Server JSON -> Application. Only copies fields with the right type, so a missing one stays blank. */
function fromServer(json: Record<string, unknown>, email: string): Application {
	const a = emptyApplication(email) as unknown as Record<string, unknown>;
	for (const key of Object.keys(a)) {
		if (key === 'pref' || key === 'goodStanding') continue;
		if (typeof json[key] === typeof a[key]) a[key] = json[key];
	}
	const pref = Array.isArray(json.pref) ? json.pref : [];
	const slug = (value: unknown) => (typeof value === 'string' ? value : '');
	a.pref = [slug(pref[0]), slug(pref[1]), slug(pref[2])];
	a.goodStanding = typeof json.goodStanding === 'boolean' ? json.goodStanding : null;
	return a as unknown as Application;
}

/** Application -> what PUT /api/application expects (preferences are already slugs). */
function toServer(a: Application) {
	return { ...a, pref: [...a.pref] };
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

	// ---- Application + letters (G3), filled from the server ------------------
	application = $state<Application>(emptyApplication(''));
	docs = $state<Record<DocType, UploadedDoc | null>>({
		courses: null,
		transcript: null,
		cv: null,
		passport: null
	});
	/** The signed-in student's own requests (GET /api/letters). */
	letters = $state<LetterRequest[]>([]);
	/** The signed-in doctor's queue, oldest first (GET /api/doctor/letters). */
	doctorLetters = $state<LetterRequest[]>([]);
	/** What the doctor answered in this visit (the server only lists waiting requests). */
	doctorAnswered = $state<LetterRequest[]>([]);
	/** True after the first load from the server. */
	loaded = $state(false);
	/** True once GET /api/me has completed (or fallen back offline). */
	meLoaded = $state(false);

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

	/** The server only sends the signed-in student's own requests. */
	myLetters = $derived(this.letters);
	hasLetter = $derived(this.myLetters.some((l) => l.status === 'Received'));

	/** The doctor's waiting requests, oldest first: the server's Queue for this doctor. */
	doctorQueue = $derived(this.doctorLetters);
	doctorHistory = $derived(this.doctorAnswered);

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

	// ---- Student profile, eligibility & budget (GET/PUT /api/me) -------------

	private applyServerProfile(
		profile: ServerStudentProfile | null,
		accountId: string,
		email: string,
		displayName?: string
	) {
		auth.syncFromServerProfile(profile);
		const fallbackName = displayName?.trim() || email.split('@')[0] || DEMO_STUDENT.name;
		if (!profile) {
			this.eligibilityProfile = null;
			this.user = {
				...(this.user ?? DEMO_STUDENT),
				email: email || DEMO_STUDENT.email,
				name: fallbackName
			};
			return;
		}

		if (profile.eligibilitySaved) {
			const elig: EligibilityProfile = {
				level: profile.level === 'GR' ? 'GR' : profile.level === 'UG' ? 'UG' : '',
				major: profile.major,
				gpa: profile.gpa,
				credits: profile.credits,
				majorDeclared: profile.majorDeclared,
				rhet110: profile.rhet110,
				rhet120: profile.rhet120,
				finalSemester: profile.finalSemester,
				academicProbation: profile.academicProbation,
				disciplinaryProbation: profile.disciplinaryProbation,
				englishComplete: profile.englishComplete,
				firstSemesterNoGpa: profile.firstSemesterNoGpa
			};
			this.eligibilityProfile = elig;
			if (accountId) writeEligibility(accountId, elig);
		} else {
			this.eligibilityProfile = null;
		}

		this.user = {
			...(this.user ?? DEMO_STUDENT),
			email: profile.email || email || DEMO_STUDENT.email,
			name: profile.name || fallbackName,
			level: profile.level === 'GR' ? 'GR' : 'UG',
			major: profile.major || this.user?.major || '',
			declared: profile.majorDeclared,
			credits: profile.credits ?? 0,
			gpa: profile.gpa ?? 0
		};

		// Pre-fill a brand-new (unsaved) application draft from /signup + /profile fields.
		if (!this.application.updatedAt) {
			if (!this.application.firstName && profile.name) {
				const parts = profile.name.trim().split(/\s+/);
				this.application.firstName = parts[0] ?? '';
				if (!this.application.surname && parts.length > 1) {
					this.application.surname = parts.slice(1).join(' ');
				}
			}
			if (!this.application.phone && profile.phone) this.application.phone = profile.phone;
			if (!this.application.aucId && profile.aucId) this.application.aucId = profile.aucId;
			if (!this.application.major && profile.major) this.application.major = profile.major;
			if (profile.level === 'UG' || profile.level === 'GR') this.application.level = profile.level;
			if (profile.gpa !== null && this.application.gpa === 0) this.application.gpa = profile.gpa;
		}

		if (profile.hasBudget && Number.isFinite(profile.budget) && profile.budget >= 0) {
			this.application.budget = profile.budget;
			if (accountId && typeof localStorage !== 'undefined') {
				try {
					localStorage.setItem(budgetKey(accountId), String(profile.budget));
				} catch {
					// Storage blocked: server already holds the value.
				}
			}
		}
	}

	/** Load the student's profile, eligibility and budget from GET /api/me. */
	async loadMe(accountId: string, email: string, displayName?: string): Promise<boolean> {
		try {
			const res = await callServer<ServerMe>('GET', '/api/me');
			this.applyServerProfile(res.profile, accountId, email, displayName);
			this.meLoaded = true;
			void discovery.refreshNotifications();
			return true;
		} catch (e) {
			// Offline fallback: keep local cache usable if the C++ server isn't running.
			this.loadEligibility(accountId);
			this.loadBudget(accountId);
			this.meLoaded = true;
			this.showToast(errorText(e));
			return false;
		}
	}

	/** Load the locally cached profile for this account, if any. */
	loadEligibility(accountId: string) {
		this.eligibilityProfile = readEligibility(accountId);
		if (this.eligibilityProfile) this.applyEligibilityToUser(this.eligibilityProfile);
	}

	/** Save the eligibility profile to PUT /api/me and mirror it into the student record. */
	async saveEligibility(
		accountId: string,
		profile: EligibilityProfile,
		displayName?: string
	): Promise<boolean> {
		const email = this.user?.email ?? this.application.studentEmail;
		try {
			const res = await callServer<ServerMe>('PUT', '/api/me', {
				name: displayName?.trim() || this.user?.name || '',
				level: profile.level,
				major: profile.major,
				gpa: profile.gpa,
				credits: profile.credits,
				majorDeclared: profile.majorDeclared,
				rhet110: profile.rhet110,
				rhet120: profile.rhet120,
				finalSemester: profile.finalSemester,
				academicProbation: profile.academicProbation,
				disciplinaryProbation: profile.disciplinaryProbation,
				englishComplete: profile.englishComplete,
				firstSemesterNoGpa: profile.firstSemesterNoGpa,
				eligibilitySaved: true
			});
			this.applyServerProfile(res.profile, accountId, email, displayName);
			this.meLoaded = true;
			void discovery.refreshNotifications();
			return true;
		} catch (e) {
			this.showToast(errorText(e));
			return false;
		}
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

	/** Load the locally cached budget for this account, if any. */
	loadBudget(accountId: string) {
		if (!accountId || typeof localStorage === 'undefined') return;
		try {
			const raw = localStorage.getItem(budgetKey(accountId));
			if (raw === null) return;
			const amount = Number(raw);
			if (Number.isFinite(amount) && amount >= 0) this.application.budget = amount;
		} catch {
			// Storage blocked: keep the server/demo budget for this visit.
		}
	}

	/** Save the per-semester budget to PUT /api/me (and cache locally). */
	async saveBudget(accountId: string, amount: number): Promise<boolean> {
		const safe = Number.isFinite(amount) ? Math.max(0, Math.round(amount)) : 0;
		this.application.budget = safe;
		if (accountId && typeof localStorage !== 'undefined') {
			try {
				localStorage.setItem(budgetKey(accountId), String(safe));
			} catch {
				// Storage blocked: server still persists the budget.
			}
		}
		try {
			const res = await callServer<ServerMe>('PUT', '/api/me', { budget: safe });
			this.applyServerProfile(res.profile, accountId, this.user?.email ?? '');
			this.showToast('Budget saved.');
			return true;
		} catch (e) {
			this.showToast(errorText(e));
			return false;
		}
	}

	/** Copy the first three saved schools into the application preference slots. */
	useSavedPreferences(slugs: readonly string[]): string[] {
		if (this.locked) return [];
		const unique = [...new Set(slugs.filter((slug) => slug !== ''))].slice(0, 3);
		this.application.pref = [unique[0] ?? '', unique[1] ?? '', unique[2] ?? ''];
		return unique;
	}

	// ---- G3: loading from the server -------------------------------------------

	/** Puts a server answer (application + documents) into the page state. */
	private applyState(state: ServerState, email: string, keepTyping = false) {
		if (state.application) {
			const fresh = fromServer(state.application, email);
			if (keepTyping) {
				// After a save, keep what the student typed meanwhile; take only what the server decides.
				this.application.status = fresh.status;
				this.application.prior = fresh.prior;
				this.application.submittedAt = fresh.submittedAt;
				this.application.updatedAt = fresh.updatedAt;
			} else {
				this.application = fresh;
			}
		} else if (!keepTyping) {
			this.application = emptyApplication(email);
		}
		for (const d of DOCUMENTS) {
			const onServer = state.documents.includes(d.type);
			if (!onServer) this.docs[d.type] = null;
			else if (!this.docs[d.type])
				this.docs[d.type] = { docType: d.type, fileName: `${d.type}.pdf`, sizeKb: 0, uploadedAt: '' };
		}
	}

	private toLetter(l: ServerLetter): LetterRequest {
		return {
			id: l.id,
			studentEmail: l.studentEmail,
			studentName: l.studentName ?? this.user?.name ?? '',
			studentMajor: l.studentMajor ?? this.user?.major ?? '',
			studentGpa: l.studentGpa ?? this.user?.gpa ?? 0,
			doctorName: l.doctorName,
			doctorEmail: l.doctorEmail,
			status: l.status,
			letterText: '',
			requestedAt: l.requestedAt,
			answeredAt: l.answeredAt
		};
	}

	/** Student pages: GET /api/application, GET /api/letters, and GET /api/me. Call once after sign-in. */
	async loadFromServer(email: string, accountId = email, displayName?: string) {
		try {
			const [state, list, me] = await Promise.all([
				callServer<ServerState>('GET', '/api/application'),
				callServer<ServerLetter[]>('GET', '/api/letters'),
				callServer<ServerMe>('GET', '/api/me')
			]);
			this.applyState(state, email);
			this.applyServerProfile(me.profile, accountId, email, displayName);
			this.letters = list.map((l) => this.toLetter(l));
			this.meLoaded = true;
			this.loaded = true;
		} catch (e) {
			this.loadEligibility(accountId);
			this.loadBudget(accountId);
			this.meLoaded = true;
			this.showToast(errorText(e));
		}
	}

	async loadLetters() {
		const list = await callServer<ServerLetter[]>('GET', '/api/letters');
		this.letters = list.map((l) => this.toLetter(l));
	}

	/** Doctor page: GET /api/doctor/letters (the doctor's queue, oldest first). */
	async loadDoctor() {
		try {
			const list = await callServer<ServerLetter[]>('GET', '/api/doctor/letters');
			this.doctorLetters = list.map((l) => this.toLetter(l));
		} catch (e) {
			this.showToast(errorText(e));
		}
	}

	// ---- G3: application -----------------------------------------------------

	/** PUT /api/application. The server refuses it after submit. */
	async saveDraft(silent = false): Promise<boolean> {
		if (this.locked) return false;
		this.application.prior = this.application.travelExperience === 'Yes, Study Abroad';
		try {
			const state = await callServer<ServerState>('PUT', '/api/application', toServer(this.application));
			this.applyState(state, this.application.studentEmail, true);
			if (!silent) this.showToast('Draft saved.');
			return true;
		} catch (e) {
			this.showToast(errorText(e));
			return false;
		}
	}

	/** Changes one preference on the page. It is saved with the next saveDraft(). */
	setPref(slot: number, slug: string) {
		if (this.locked) return;
		const next = [...this.application.pref] as [string, string, string];
		next[slot] = slug;
		this.application.pref = next;
	}

	/**
	 * Checks the file, shows it at once, then sends it: PUT /api/application/documents/<type>.
	 * Returns an error message for the quick checks, or null. A server refusal shows as a toast.
	 */
	upload(type: DocType, file: File): string | null {
		if (this.locked) return 'Your application is submitted, so files can no longer change.';
		const isPdf = file.type === 'application/pdf' || file.name.toLowerCase().endsWith('.pdf');
		if (!isPdf) return 'Only PDF files are accepted.';
		if (file.size > RULES.maxFileMb * 1024 * 1024) return `The file is larger than ${RULES.maxFileMb} MB.`;

		const before = this.docs[type];
		this.docs[type] = {
			docType: type,
			fileName: file.name,
			sizeKb: Math.max(1, Math.round(file.size / 1024)),
			uploadedAt: today()
		};
		callServer<ServerState>('PUT', `/api/application/documents/${type}`, file)
			.then(() => this.showToast('File uploaded.'))
			.catch((e) => {
				this.docs[type] = before;
				this.showToast(errorText(e));
			});
		return null;
	}

	/** DELETE /api/application/documents/<type>. */
	async removeDoc(type: DocType) {
		if (this.locked) return;
		const before = this.docs[type];
		this.docs[type] = null;
		try {
			await callServer<ServerState>('DELETE', `/api/application/documents/${type}`);
		} catch (e) {
			this.docs[type] = before;
			this.showToast(errorText(e));
		}
	}

	/** Saves the latest answers, then POST /api/application/submit. The server re-checks everything. */
	async submit(): Promise<boolean> {
		if (this.locked) return false;
		if (this.deadlinePassed) {
			this.showToast('The deadline has passed. Late applications are not considered.');
			return false;
		}
		if (this.missing.length > 0) {
			this.showToast(`${this.missing.length} items are still missing.`);
			return false;
		}
		if (!(await this.saveDraft(true))) return false;
		try {
			// The server verifies full IPSO eligibility directly from /api/me's StudentService HashMap.
			const state = await callServer<ServerState>('POST', '/api/application/submit', {});
			this.applyState(state, this.application.studentEmail);
			void discovery.refreshNotifications();
			this.showToast('Application submitted. It can no longer be edited.');
			return true;
		} catch (e) {
			this.showToast(errorText(e));
			return false;
		}
	}

	// ---- G3: recommendation letters --------------------------------------------

	/** POST /api/letters. Returns an error message, or null when the request was sent. */
	async requestLetter(doctorName: string, doctorEmail: string): Promise<string | null> {
		const email = doctorEmail.trim().toLowerCase();
		if (doctorName.trim() === '') return 'Enter the faculty member’s name.';
		if (!email.endsWith('@aucegypt.edu')) return 'The letter must come from an AUC faculty member (@aucegypt.edu).';
		try {
			await callServer('POST', '/api/letters', { doctorName: doctorName.trim(), doctorEmail: email });
			await this.loadLetters();
			void discovery.refreshNotifications();
			this.showToast(`Request sent to ${doctorName.trim()}.`);
			return null;
		} catch (e) {
			return errorText(e);
		}
	}

	/** Doctor: POST /api/doctor/letters/<id> with the letter. Returns an error message or null. */
	async answerLetter(id: number, text: string): Promise<string | null> {
		if (text.trim().length < 20) return 'Write the letter before sending (at least a few sentences).';
		return this.answer(id, true, text.trim());
	}

	/** Doctor: POST /api/doctor/letters/<id> with send = false. */
	async declineLetter(id: number) {
		const error = await this.answer(id, false, '');
		if (error) this.showToast(error);
	}

	private async answer(id: number, send: boolean, letterText: string): Promise<string | null> {
		const l = this.doctorLetters.find((x) => x.id === id);
		try {
			await callServer('POST', `/api/doctor/letters/${id}`, { send, letterText });
		} catch (e) {
			return errorText(e);
		}
		if (l) {
			this.doctorAnswered = [
				{ ...l, status: send ? 'Received' : 'Declined', answeredAt: today() },
				...this.doctorAnswered
			];
			this.showToast(send ? `Letter sent for ${l.studentName}.` : `Declined the request from ${l.studentName}.`);
		}
		await this.loadDoctor();
		return null;
	}
}

/** One shared store for Group 3's pages. */
export const store = new StudentStore();
