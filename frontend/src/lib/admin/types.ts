export type AuditStatus = 'Draft' | 'Submitted' | 'In Review' | 'Verified' | 'Ineligible';
export type AllocationStrategy = 'best_fit' | 'highest_score' | 'most_placed' | 'shortlists';

export interface WeightConfig {
	pref: number;
	gpa: number;
	llm: number;
	equity: number;
	allowUnranked: boolean;
	mode: AllocationStrategy;
}

export const DEFAULT_WEIGHT_CONFIG: WeightConfig = {
	pref: 0.65,
	gpa: 0.45,
	llm: 0.35,
	equity: 0.2,
	allowUnranked: false,
	mode: 'best_fit'
};

export interface AdminOverviewStats {
	totalApplicants: number;
	eligibleCount: number;
	firstTimeAbroadCount: number;
	flaggedCount: number;
	lowGpaFlaggedCount: number;
	reviewQueueWaiting: number;
	demoApplicantCount: number;
	totalPartners: number;
	totalConfiguredSeats: number;
	customQuotaCount: number;
	hkMaxFeasible: number;
}

export interface AdminPartnerSummary {
	slug: string;
	officialName: string;
	shortName: string;
	country: string;
	stateOrProvince: string;
	city: string;
	region: string;
	agreementStatus: string;
	agreementScope: string;
	studentLevels: string[];
	eligibleAucSchools: string[];
	eligibleMajors: string[];
	excludedMajors: string[];
	availableTerms: string[];
	minGpa: number | null;
	seats: number;
	seatsAssumed: boolean;
	livingCostUsd: number | null;
	estMonthlyLocal: string;
	tuitionModel: string;
	housingAvailability: string;
	factSheetStatus: string;
	factSheetFilesCount: number;
	firstChoiceDemand: number;
	totalDemand: number;
}

export interface AdminPartnerDetail extends AdminPartnerSummary {
	minStandingNote: string;
	instructionLanguages: string[];
	englishCourseAvailability: string;
	englishProficiency: string;
	hostLanguageRequirement: string;
	fallTermDates: string;
	springTermDates: string;
	nominationDeadlines: string;
	finalSemesterAllowed: boolean | null;
	creditSystemAndLoad: string;
	courseRestrictionsNote: string;
	requiredDocs: string[];
	mandatoryHostFees: string;
	housingNotes: string;
	hostGrants: string;
	citizenshipRestrictions: string;
	visaNotes: string;
	academicStyle: string;
	internationalSupport: string;
	officialWebsiteUrl: string;
	exchangePageUrl: string;
	courseCatalogUrl: string;
	englishCourseListUrl: string;
}

export interface AdminPartnerStats {
	totalPartners: number;
	activePartners: number;
	checkSlotsPartners: number;
	loiPartners: number;
	totalConfiguredSeats: number;
	customQuotaCount: number;
	highDemandCount: number;
	factSheetMatchedCount: number;
	knownMinGpaCount: number;
}

export interface AdminPartnerListResponse {
	count: number;
	stats: AdminPartnerStats;
	universities: AdminPartnerSummary[];
}

export interface AdminApplicationSummary {
	email: string;
	firstName: string;
	surname: string;
	name: string;
	aucId: string;
	level: 'UG' | 'GR' | string;
	standing: string;
	major: string;
	gpa: number;
	credits: number;
	hasCredits: boolean;
	majorDeclared: boolean;
	rhet110: boolean;
	rhet120: boolean;
	rhet1020: string;
	expectedGraduation: string;
	finalSemester: boolean;
	academicProbation: boolean;
	disciplinaryProbation: boolean;
	englishComplete: boolean;
	firstSemesterNoGpa: boolean;
	eligibilitySaved: boolean;
	nationality: string;
	secondNationality: string;
	gender: string;
	phone: string;
	dateOfBirth: string;
	programType: string;
	scholarship: string;
	accommodation: boolean;
	budget: number;
	prior: boolean;
	travelExperience: string;
	status: AuditStatus;
	goodStanding: boolean | null;
	declaration: boolean;
	submittedAt: string;
	updatedAt: string;
	prefSlugs: [string, string, string];
	prefNames: [string, string, string];
	uploadedDocs: string[];
	letterReceivedCount: number;
	letterPendingCount: number;
	letterDeclinedCount: number;
	hasStatementRating: boolean;
	statementRating: number | null;
	adminNotes: string;
	isDemo: boolean;
	flagged: boolean;
	eligibilityReason: string;
	selected?: boolean;
}

export interface AdminApplicationDetail extends AdminApplicationSummary {
	guardianName: string;
	guardianRelation: string;
	guardianPhone: string;
	guardianEmail: string;
	guardianLanguage: string;
	qPreparedness: string;
	qGoals: string;
	qAdaptability: string;
	qTeamwork: string;
	qReflection: string;
	qFinance: string;
}

export interface AdminApplicationListResponse {
	totalAll: number;
	count: number;
	applications: AdminApplicationSummary[];
}

export interface AdminPlacementRow {
	studentEmail: string;
	studentName: string;
	studentAucId: string;
	studentMajor: string;
	studentGpa: number;
	universitySlug: string;
	universityName: string;
	seatLabel: string;
	prefRank: number;
	rankLabel: string;
	weight: number;
}

export interface AdminUnplacedRow {
	studentEmail: string;
	studentName: string;
	studentAucId: string;
	studentMajor: string;
	gpa: number;
	prefsLabel: string;
	bottleneck: string;
	reason: string;
}

export interface AdminUniversityFillRow {
	slug: string;
	name: string;
	country: string;
	region: string;
	totalSeats: number;
	seatsAssumed: boolean;
	filledSeats: number;
	leftSeats: number;
	demandCount: number;
	demandRatio: number;
	cutoffScore: number | null;
}

export interface AdminShortlistCandidateRow {
	rank: number;
	studentEmail: string;
	studentName: string;
	studentAucId: string;
	studentMajor: string;
	studentGpa: number;
	score: number;
	minGpaUnknown: boolean;
	statementMissing: boolean;
}

export interface AdminShortlistRow {
	slug: string;
	name: string;
	country: string;
	totalSeats: number;
	seatsAssumed: boolean;
	eligibleCount: number;
	candidates: AdminShortlistCandidateRow[];
}

export interface AdminRunDiffStudent {
	studentEmail: string;
	studentName: string;
	changeType: 'gained' | 'lost' | 'shifted' | string;
	fromUniSlug: string;
	fromUniName: string;
	toUniSlug: string;
	toUniName: string;
	scoreDelta: number;
}

export interface AdminRunDiff {
	hasBaseline: boolean;
	baselineRunId: number;
	baselineRunCode: string;
	baselineTitle: string;
	placedDelta: number;
	firstChoiceDelta: number;
	gainedCount: number;
	lostCount: number;
	shiftedCount: number;
	changes: AdminRunDiffStudent[];
}

export interface AdminSimulationResult {
	mode: AllocationStrategy;
	cycle: string;
	allowUnranked: boolean;
	weights: WeightConfig;
	placed: number;
	eligibleCount: number;
	unplacedCount: number;
	hkMax: number;
	firstChoiceCount: number;
	secondChoiceCount: number;
	thirdChoiceCount: number;
	unrankedCount: number;
	firstChoicePct: number;
	avgWeight: number;
	solveTimeMs: number;
	placements: AdminPlacementRow[];
	unplaced: AdminUnplacedRow[];
	universityFills: AdminUniversityFillRow[];
	shortlists: AdminShortlistRow[];
	diff: AdminRunDiff;
}

export interface AdminSavedRunSummary {
	id: number;
	runCode: string;
	cycle: string;
	title: string;
	mode: AllocationStrategy;
	allowUnranked: boolean;
	weights: WeightConfig;
	defaultSeats: number;
	status: 'Draft' | 'Published' | 'Archived';
	placed: number;
	eligibleCount: number;
	hkMax: number;
	firstChoiceCount: number;
	secondChoiceCount: number;
	thirdChoiceCount: number;
	unrankedCount: number;
	firstChoicePct: number;
	avgScore: number;
	createdBy: string;
	createdAt: string;
}

export interface AdminSavedRunDetail extends AdminSavedRunSummary {
	placements: AdminPlacementRow[];
	unplaced: AdminUnplacedRow[];
	universityFills?: AdminUniversityFillRow[];
}
