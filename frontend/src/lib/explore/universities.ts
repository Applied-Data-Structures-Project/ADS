/** Frontend-only discovery adapter. Replace the fetch path with the Crow API later. */
export type Sourced<T> = {
  value: T | null;
  status: 'verified' | 'partial' | 'unknown';
  source_type: 'excel_list' | 'fact_sheet' | 'official_web' | 'unknown';
  source_ref: string | null;
  notes: string | null;
};

export type UniversityRecord = {
  slug: string;
  official_name: Sourced<string>;
  short_name: Sourced<string>;
  country: Sourced<string>;
  city: Sourced<string>;
  region: Sourced<string>;
  agreement_status: Sourced<string>;
  academic_style: Sourced<string>;
  campus_type: Sourced<string>;
  international_support_services: Sourced<string>;
  host_grants_or_stipends: Sourced<string>;
  min_standing_note: Sourced<string>;
  fall_term_dates: Sourced<string>;
  spring_term_dates: Sourced<string>;
  mandatory_host_fees_and_insurance: Sourced<string>;
  credit_system_and_load: Sourced<string>;
  eligible_auc_schools: Sourced<string[]>;
  agreement_student_levels: Sourced<string[]>;
  agreement_scope_raw: Sourced<string>;
  eligible_majors: Sourced<string[]>;
  excluded_majors: Sourced<string[]>;
  min_gpa_host: Sourced<number>;
  instruction_languages: Sourced<string[]>;
  english_proficiency_and_rhet1020: Sourced<string>;
  host_language_requirement: Sourced<string>;
  available_terms: Sourced<string[]>;
  tuition_model: Sourced<string>;
  est_monthly_living_local: Sourced<{currency: string; min: number; max: number}>;
  housing_availability: Sourced<string>;
  housing_notes: Sourced<string>;
  visa_lead_time_and_financial_proof: Sourced<string>;
  required_application_docs: Sourced<string[]>;
  official_website_url: Sourced<string>;
  exchange_page_url: Sourced<string>;
  course_catalog_url: Sourced<string>;
  course_restrictions_note: Sourced<string>;
  nomination_deadlines: Sourced<string>;
};

export function shown(value: string | null | undefined): string {
  return value && value.trim() ? value : 'Not available';
}

export function textList(field: Sourced<string[]>): string {
  return field.value?.length ? field.value.join(', ') : 'Not available';
}

export async function loadUniversities(): Promise<UniversityRecord[]> {
	// Address of our C++ university API.
	  const url = '/api/universities';

	  // Send a request to the backend.
	  const response = await fetch(url);

	  // Check whether the request succeeded.
	  if (!response.ok) {
		  throw new Error(
			  `Unable to load universities (${response.status})`
		  );
	  }

	  // Convert the response from JSON into a JavaScript object.
	  const body: { universities: UniversityRecord[] } =
		  await response.json();

	  // Make sure the response contains a university list.
	  if (!Array.isArray(body.universities)) {
		  throw new Error('Unexpected university data format');
	  }

	  // Return the university records to our Explore page.
	  return body.universities;
  }



/*Load one university using its unique slug.
 * For example, if the student opens Aarhus University, we request /api/universities/aarhus-university.
 * The C++ backend uses our custom HashMap to locate the university instead of searching the entire Vector.
 */

export async function loadUniversity(slug: string): Promise<UniversityRecord> {

	// Build the API URL using the university slug.
	const url =
	'/api/universities/'  + encodeURIComponent(slug);

	// Request the university's details from the backend.
	const response = await fetch(url);

	// Check that the university was found.
	if (!response.ok) {
		throw new Error(
			`Unable to load university (${response.status})`
		);
	}

	// Read and return the university information.
	const university: UniversityRecord = await response.json();

	return university;
}



/** Keep only a handful of recently opened universities, locally in the browser. */
const RECENT_KEY = 'auc-explore-recent-v1';
export function recentSlugs(): string[] {
  if (typeof localStorage === 'undefined') return [];
  try {
    const value = JSON.parse(localStorage.getItem(RECENT_KEY) || '[]');
    return Array.isArray(value) ? value.filter((item): item is string => typeof item === 'string').slice(0, 5) : [];
  } catch { return []; }
}
export function rememberUniversity(slug: string): void {
  if (typeof localStorage === 'undefined') return;
  try { localStorage.setItem(RECENT_KEY, JSON.stringify([slug, ...recentSlugs().filter((s) => s !== slug)].slice(0, 5))); } catch { /* storage optional */ }
}
export function statusLabel(status: string | null): string {
  if (status === 'active') return 'Active partner';
  if (status === 'letter_of_intent') return 'Agreement pending';
  if (status === 'check_slots') return 'Check availability';
  return 'Status to confirm';
}

/** QS 2027 ranking data is stored separately from partner information. */
export type QsMatch = { rank: string; name: string; status: 'matched' | 'parent_institution' };
export type QsMatches = Record<string, QsMatch>;
export async function loadQsRankings(): Promise<QsMatches> {
  const response = await fetch('/data/qs-2027-matches.json');
  if (!response.ok) throw new Error(`Unable to load QS rankings (${response.status})`);
  const body: {matches: QsMatches} = await response.json();
  return body.matches ?? {};
}
export function qsSortKey(rank: string | undefined): number {
  if (!rank) return Number.POSITIVE_INFINITY;
  const first = rank.replace(/^=/, '').match(/\d+/);
  return first ? Number(first[0]) : Number.POSITIVE_INFINITY;
}
export function qsDisplay(match: QsMatch | undefined): string {
	// If we cannot confirm a matching QS ranking, display a short message instead of a long sentence.
	   if (!match) {
		   return 'Not listed';
	   }

	   // If the university has a confirmed ranking, display the ranking as usual.
	   return `${match.status === 'parent_institution' ? 'Parent institution · ' : ''}QS ${match.rank}`;
}
