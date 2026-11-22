// Mock data for Group 3's pages, used until each backend route exists.
// Swap a mock for the real call in store.svelte.ts once the route lands (see the TODO next to each action).
// Universities are real rows from IPSO's "Fall 2027 list" (id = row number in the sheet).

import type {
	Application,
	DocType,
	LetterRequest,
	StudentProfile,
	UploadedDoc
} from './types.ts';

/** IPSO rules from the real Fall 2027 application form. */
export const RULES = {
	minGpa: { UG: 2.8, GR: 3.2 },
	minCredits: 24,
	maxLetters: 2,
	maxFileMb: 5,
	goalsWords: { min: 300, max: 500 },
	adaptabilityWords: { min: 300, max: 5000 },
	// Real deadline was 25 Oct 2026, 3:00 PM Cairo. Kept as a setting so the demo can run after it.
	deadline: '2026-11-30T15:00:00+02:00'
} as const;

export const DOCUMENTS: { type: DocType; label: string; hint: string; template?: string }[] = [
	{
		type: 'courses',
		label: 'Draft List of Intended Courses',
		hint: 'One list for each of your 3 universities. Plan it with your academic advisor first.',
		template: 'Draft List of Courses Template'
	},
	{ type: 'transcript', label: 'Unofficial AUC Transcript', hint: 'Download the PDF from AUC Banner Self Service.' },
	{ type: 'cv', label: 'Curriculum Vitae (CV)', hint: 'Include your extracurricular activities.' },
	{ type: 'passport', label: 'Copy of Your Passport', hint: 'Make sure your passport is valid.' }
];

/** The logged-in demo student. Same person as AUC-900219 in the admin mocks. */
export const DEMO_STUDENT: StudentProfile = {
	email: 's.abdelrahman@aucegypt.edu',
	name: 'Salma Abdelrahman',
	role: 'student',
	level: 'UG',
	major: 'Economics (ECON)',
	declared: true,
	credits: 64,
	gpa: 3.92,
	aucId: '900219004'
};

export const DEMO_DOCTOR: StudentProfile = {
	email: 'amr.hassan@aucegypt.edu',
	name: 'Dr. Amr Hassan',
	role: 'doctor',
	level: 'GR',
	major: 'Department of Economics',
	declared: true,
	credits: 0,
	gpa: 0,
	aucId: '100045871'
};

export const DEMO_APPLICATION: Application = {
	studentEmail: DEMO_STUDENT.email,
	firstName: 'Salma',
	surname: 'Abdelrahman',
	dateOfBirth: '04/17/2005',
	nationality: 'Egyptian',
	secondNationality: '',
	gender: 'Female',
	phone: '+20 100 555 2190',
	aucId: DEMO_STUDENT.aucId,
	gpa: DEMO_STUDENT.gpa,
	level: DEMO_STUDENT.level,
	major: DEMO_STUDENT.major,
	rhet1020: 'Yes',
	expectedGraduation: '06/2028',
	accommodation: false,
	scholarship: 'None',
	programType: 'Exchange',
	pref: [
		'sciences-po-institute-d-etudes-politiques-de-paris',
		'bocconi-university',
		''
	],
	guardianName: 'Hany Abdelrahman',
	guardianRelation: 'Father',
	guardianPhone: '+20 122 410 7733',
	guardianEmail: 'hany.abdelrahman@gmail.com',
	guardianLanguage: 'Arabic',
	qPreparedness:
		'I have completed the core economics sequence, including ECON 2011 and 2021, and I am taking econometrics this semester. I met my advisor to map which courses abroad can replace my remaining electives.',
	qGoals:
		'I am researching trade policy across the Mediterranean, and I want to understand how European institutions shape trade agreements with North African countries. Sciences Po offers seminars on European political economy that AUC does not, and its faculty publish on exactly the questions my senior thesis will ask. Studying there would let me read primary sources in French and attend policy events in Paris. Bocconi is my second choice because its economics department is strong in quantitative trade models, which complements the policy angle. After graduating I plan to apply for a master in international economics and to work on trade negotiations in the public sector. A semester abroad would give me the comparative view my research needs and the language practice to work in French professionally.',
	travelExperience: 'Yes, Non-Study Abroad',
	qAdaptability:
		'I adapt by learning the everyday rules first: how people greet each other, how classes run, how to ask for help. When I travelled to France with my family I practised ordering, asking for directions and reading signs in French, which made the city feel familiar within days.',
	qTeamwork: '',
	qReflection: '',
	qFinance: '',
	budget: 7500,
	prior: false,
	declaration: false,
	status: 'Draft',
	goodStanding: null,
	submittedAt: '',
	updatedAt: '2026-11-14T21:40:00'
};

export const DEMO_DOCS: Record<DocType, UploadedDoc | null> = {
	courses: { docType: 'courses', fileName: 'Salma_Abdelrahman_courses.pdf', sizeKb: 182, uploadedAt: '2026-11-12' },
	transcript: { docType: 'transcript', fileName: 'unofficial_transcript.pdf', sizeKb: 96, uploadedAt: '2026-11-12' },
	cv: { docType: 'cv', fileName: 'Salma_CV_2026.pdf', sizeKb: 143, uploadedAt: '2026-11-13' },
	passport: null
};

export const DEMO_LETTERS: LetterRequest[] = [
	{
		id: 4,
		studentEmail: 'y.elmasry@aucegypt.edu',
		studentName: 'Youssef El-Masry',
		studentMajor: 'Computer Science (CSCE)',
		studentGpa: 3.78,
		doctorName: 'Dr. Amr Hassan',
		doctorEmail: DEMO_DOCTOR.email,
		status: 'Pending',
		letterText: '',
		requestedAt: '2026-11-09',
		answeredAt: ''
	},
	{
		id: 7,
		studentEmail: 'l.mansour@aucegypt.edu',
		studentName: 'Laila Mansour',
		studentMajor: 'Architecture (ARCH)',
		studentGpa: 3.85,
		doctorName: 'Dr. Amr Hassan',
		doctorEmail: DEMO_DOCTOR.email,
		status: 'Pending',
		letterText: '',
		requestedAt: '2026-11-11',
		answeredAt: ''
	},
	{
		id: 9,
		studentEmail: DEMO_STUDENT.email,
		studentName: DEMO_STUDENT.name,
		studentMajor: DEMO_STUDENT.major,
		studentGpa: DEMO_STUDENT.gpa,
		doctorName: 'Dr. Amr Hassan',
		doctorEmail: DEMO_DOCTOR.email,
		status: 'Pending',
		letterText: '',
		requestedAt: '2026-11-13',
		answeredAt: ''
	},
	{
		id: 2,
		studentEmail: 'm.nabil@aucegypt.edu',
		studentName: 'Mariam Nabil',
		studentMajor: 'Business Admin (FINC)',
		studentGpa: 3.51,
		doctorName: 'Dr. Amr Hassan',
		doctorEmail: DEMO_DOCTOR.email,
		status: 'Received',
		letterText: 'Mariam is one of the most prepared students in my finance seminar…',
		requestedAt: '2026-11-02',
		answeredAt: '2026-11-06'
	}
];
