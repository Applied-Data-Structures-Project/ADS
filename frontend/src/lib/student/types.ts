// Student-side types. Shapes match the database tables and docs/api.md.
// G1 = Maya (Explore) · G2 = Muhammad (Login, Compare, Saved) · G3 = Malak (Application, Letters)

export type Level = 'UG' | 'GR';
export type Role = 'student' | 'doctor';

/** One row of `students` (G2). Read by G3's eligibility check. */
export interface StudentProfile {
	email: string;
	name: string;
	role: Role;
	level: Level;
	major: string;
	declared: boolean;
	credits: number;
	gpa: number;
	aucId: string;
}

export type AppStatus = 'Draft' | 'Submitted' | 'In Review' | 'Verified' | 'Ineligible';
export type ProgramType = '' | 'Exchange' | 'Direct Enrollment';
export type TravelExperience = '' | 'Yes, Study Abroad' | 'Yes, Non-Study Abroad' | 'No' | 'Other';

/** One row of `applications` (G3). Same columns as 002_application.sql, in camelCase. */
export interface Application {
	studentEmail: string;
	// Personal info
	firstName: string;
	surname: string;
	dateOfBirth: string;
	nationality: string;
	secondNationality: string;
	gender: '' | 'Male' | 'Female';
	phone: string;
	aucId: string;
	gpa: number;
	level: Level;
	major: string;
	rhet1020: '' | 'Yes' | 'Currently Enrolled' | 'Not Applicable';
	expectedGraduation: string;
	accommodation: boolean;
	scholarship: '' | 'None' | "Tomorrow's Leaders" | 'Other';
	programType: ProgramType;
	// Preferences: Explore university slugs, rank 1..3. '' = not chosen.
	pref: [string, string, string];
	// Guardian
	guardianName: string;
	guardianRelation: string;
	guardianPhone: string;
	guardianEmail: string;
	guardianLanguage: string;
	// Questionnaire
	qPreparedness: string;
	qGoals: string;
	travelExperience: TravelExperience;
	qAdaptability: string;
	qTeamwork: string;
	qReflection: string;
	qFinance: string;
	// Extras for the admin side
	budget: number;
	prior: boolean;
	// Submit and review
	declaration: boolean;
	status: AppStatus;
	goodStanding: boolean | null;
	submittedAt: string;
	updatedAt: string;
}

export type DocType = 'courses' | 'transcript' | 'cv' | 'passport';

/** One row of `application_documents` (G3). */
export interface UploadedDoc {
	docType: DocType;
	fileName: string;
	sizeKb: number;
	uploadedAt: string;
}

export type LetterStatus = 'Pending' | 'Received' | 'Declined';

/** One row of `letters` (G3), with the student's name/major/GPA joined in for the doctor page. */
export interface LetterRequest {
	id: number;
	studentEmail: string;
	studentName: string;
	studentMajor: string;
	studentGpa: number;
	doctorName: string;
	doctorEmail: string;
	status: LetterStatus;
	letterText: string;
	requestedAt: string;
	answeredAt: string;
}

/** One eligibility rule shown to the student. */
export interface EligibilityCheck {
	label: string;
	ok: boolean;
	detail: string;
	/** True for the graduate first-semester GPA rule. */
	provisional?: boolean;
}
