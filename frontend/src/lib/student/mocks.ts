// Mock data for Group 3's pages, used until each backend route exists.
// Swap a mock for the real call in store.svelte.ts once the route lands (see the TODO next to each action).
// Universities are real rows from IPSO's "Fall 2027 list" (id = row number in the sheet).

import type {
	Application,
	DocType,
	LetterRequest,
	StudentProfile,
	University,
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

export const UNIVERSITIES: University[] = [
	{id: 1, name: "Aarhus University", country: "Denmark", city: "Aarhus", region: "Europe", ug: true, gr: true, terms: "AY/Spring, Summer", scope: "University-wide", website: "http://www.au.dk/en/"},
	{id: 2, name: "Akita International University", country: "Japan", city: "Akita", region: "East Asia", ug: true, gr: false, terms: "Fall/Spring", scope: "University-wide", website: "http://www.aiu.ac.jp/"},
	{id: 3, name: "Al Akhawayn University", country: "Morocco", city: "Ifrane", region: "Middle East & Africa", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "http://www.aui.ma/en/"},
	{id: 10, name: "Bocconi University", country: "Italy", city: "Milan", region: "Europe", ug: true, gr: false, terms: "Fall/Spring", scope: "Business and Economics", website: "http://www.unibocconi.eu"},
	{id: 11, name: "Boğaziçi University", country: "Turkey", city: "Istanbul", region: "Europe", ug: true, gr: true, terms: "Fall/Spring/Summer", scope: "University-wide", website: "https://bogazici.edu.tr"},
	{id: 15, name: "Catholic University of Sacred Heart (Università Cattolica del Sacro Cuore)", country: "Italy", city: "Milan", region: "Europe", ug: true, gr: true, terms: "Fall/Spring/Summer", scope: "University-wide", website: "http://www.unicatt.it/"},
	{id: 16, name: "Central European University", country: "Austria", city: "Vienna", region: "Europe", ug: true, gr: false, terms: "Fall/Spring", scope: "Political science", website: "https://www.ceu.edu/"},
	{id: 18, name: "Charles University (Univerzita Karlova)", country: "Czech Republic", city: "Prague", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://cuni.cz/"},
	{id: 19, name: "Chung-Ang University", country: "South Korea", city: "Seoul / Anseong", region: "East Asia", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://www.cau.ac.kr/"},
	{id: 21, name: "Concordia University", country: "Canada", city: "Montreal, Quebec", region: "North America", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "http://www.concordia.ca/"},
	{id: 23, name: "Copenhagen Business School", country: "Denmark", city: "Copenhagen", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "Business Administration", website: "https://www.cbs.dk/en"},
	{id: 24, name: "Cornell University", country: "USA", city: "Ithaca, New York", region: "North America", ug: true, gr: false, terms: "Fall/Spring", scope: "University-wide", website: "https://www.cornell.edu/"},
	{id: 31, name: "EDHEC Business School", country: "France", city: "Nice", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "Business Administration", website: "https://www.edhec.edu/en"},
	{id: 33, name: "Emlyon Business School", country: "France", city: "Lyon", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "Business Administration", website: "https://www.em-lyon.com"},
	{id: 36, name: "Freie Universität Berlin", country: "Germany", city: "Berlin", region: "Europe", ug: true, gr: true, terms: "AY/Spring", scope: "University-wide", website: "http://www.fu-berlin.de"},
	{id: 52, name: "John Cabot University", country: "Italy", city: "Rome", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://www.johncabot.edu"},
	{id: 57, name: "King's College London (KCL)", country: "UK", city: "London", region: "Europe", ug: true, gr: false, terms: "Summer", scope: "University-wide", website: "https://www.kcl.ac.uk/"},
	{id: 61, name: "Lingnan University", country: "China", city: "Tuen Mun, Hong Kong", region: "East Asia", ug: true, gr: false, terms: "Fall/Spring", scope: "University-wide", website: "https://www.ln.edu.hk"},
	{id: 65, name: "Lund University", country: "Sweden", city: "Lund", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "http://www.lunduniversity.lu.se/"},
	{id: 71, name: "Meiji Gakuin University", country: "Japan", city: "Tokyo", region: "East Asia", ug: true, gr: false, terms: "Fall/Spring", scope: "University-wide", website: "https://www.meijigakuin.ac.jp/en/"},
	{id: 79, name: "Nova School of Business and Economics (Nova SBE)", country: "Portugal", city: "Lisbon", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "Business and Economics", website: "http://www.novasbe.unl.pt"},
	{id: 82, name: "Ozyegin University", country: "Turkey", city: "Istanbul", region: "Europe", ug: true, gr: false, terms: "Fall/Spring", scope: "University-wide", website: "https://www.ozyegin.edu.tr"},
	{id: 88, name: "Pompeu Fabra University (UPF)", country: "Spain", city: "Barcelona", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "Global Studies and Humanities", website: "https://www.upf.edu"},
	{id: 90, name: "Radboud University", country: "Netherlands", city: "Nijmegen", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://www.ru.nl/io/english"},
	{id: 94, name: "Richmond American University London (RAUL)", country: "UK", city: "Richmond", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://www.richmond.ac.uk/"},
	{id: 98, name: "Sciences Po (Institute d'Etudes Politiques de Paris)", country: "France", city: "Paris", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "http://www.sciencespo.fr"},
	{id: 104, name: "Technical University Darmstadt", country: "Germany", city: "Darmstadt", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://www.tu-darmstadt.de"},
	{id: 105, name: "Technical University of Munich (TUM)", country: "Germany", city: "München", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "http://www.tum.de/"},
	{id: 108, name: "The American College of Greece (ACG)", country: "Greece", city: "Athens", region: "Europe", ug: true, gr: true, terms: "Fall/Spring/Summer", scope: "University-wide", website: "http://www.acg.edu/"},
	{id: 119, name: "Trinity College Dublin", country: "Ireland", city: "Dublin", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://www.tcd.ie"},
	{id: 122, name: "Universidad Carlos III de Madrid (UC3M)", country: "Spain", city: "Madrid", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://www.uc3m.es"},
	{id: 133, name: "University of Edinburgh", country: "UK", city: "Edinburgh", region: "Europe", ug: true, gr: false, terms: "Fall/Spring", scope: "University-wide", website: "https://www.ed.ac.uk"},
	{id: 134, name: "University of Geneva", country: "Switzerland", city: "Geneva", region: "Europe", ug: true, gr: true, terms: "AY/Spring", scope: "University-wide", website: "http://www.unige.ch/international/en/"},
	{id: 154, name: "Vrije Universiteit Amsterdam", country: "Netherlands", city: "Amsterdam", region: "Europe", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://vu.nl/nl"},
	{id: 158, name: "Washington State University", country: "USA", city: "Pullman, Washington", region: "North America", ug: true, gr: true, terms: "Fall/Spring", scope: "University-wide", website: "https://wsu.edu/"},
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

export const DEMO_SAVED: number[] = [10, 98, 154, 65, 23];

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
	pref: [98, 10, -1],
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
