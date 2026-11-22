#include "repo/admin_repo.hpp"

#include <sstream>

using namespace std;

namespace
{

string colText(sqlite3_stmt *s, int i)
{
    const unsigned char *t = sqlite3_column_text(s, i);
    return t ? reinterpret_cast<const char *>(t) : "";
}

int colInt(sqlite3_stmt *s, int i, int ifNull = 0)
{
    if (sqlite3_column_type(s, i) == SQLITE_NULL)
        return ifNull;
    return sqlite3_column_int(s, i);
}

double colDouble(sqlite3_stmt *s, int i, double ifNull = 0.0)
{
    if (sqlite3_column_type(s, i) == SQLITE_NULL)
        return ifNull;
    return sqlite3_column_double(s, i);
}

bool colIsNull(sqlite3_stmt *s, int i)
{
    return sqlite3_column_type(s, i) == SQLITE_NULL;
}

void bindTextOrNull(sqlite3_stmt *s, int i, const string &value)
{
    if (value.empty())
        sqlite3_bind_null(s, i);
    else
        sqlite3_bind_text(s, i, value.c_str(), -1, SQLITE_TRANSIENT);
}

// Lightweight parser for JSON string arrays stored in SQLite TEXT columns (e.g. ["Fall","Spring"]).
// Zero STL containers used.
Vector<string> parseJsonStringArray(const string &json)
{
    Vector<string> out;
    size_t i = 0;
    while (i < json.size())
    {
        if (json[i] == '"')
        {
            i++;
            string item;
            while (i < json.size())
            {
                if (json[i] == '\\' && i + 1 < json.size())
                {
                    char esc = json[i + 1];
                    if (esc == '"' || esc == '\\' || esc == '/')
                        item += esc;
                    else if (esc == 'n')
                        item += '\n';
                    else if (esc == 't')
                        item += '\t';
                    else
                        item += esc;
                    i += 2;
                }
                else if (json[i] == '"')
                {
                    i++;
                    break;
                }
                else
                {
                    item += json[i++];
                }
            }
            if (!item.empty())
                out.push_back(item);
        }
        else
        {
            i++;
        }
    }
    return out;
}

string formatMonthlyLocal(const string &currency, bool hasMin, double minVal, bool hasMax, double maxVal)
{
    if (currency.empty() || (!hasMin && !hasMax))
        return "";
    std::ostringstream os;
    if (hasMin && hasMax && minVal != maxVal)
        os << static_cast<long long>(minVal) << "–" << static_cast<long long>(maxVal) << " " << currency << "/mo";
    else
        os << static_cast<long long>(hasMin ? minVal : maxVal) << " " << currency << "/mo";
    return os.str();
}

bool execSql(sqlite3 *db, const char *sql)
{
    char *err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (err)
        sqlite3_free(err);
    return rc == SQLITE_OK;
}

} // namespace

AdminRepo::AdminRepo(sqlite3 *db) : db(db) {}

Vector<AdminPartnerDetail> AdminRepo::loadPartners()
{
    Vector<AdminPartnerDetail> result;
    if (!db)
        return result;

    const char *sql =
        "SELECT slug, official_name, short_name, country, state_or_province, city, region, "
        "agreement_status, agreement_student_levels_json, min_standing_note, agreement_scope_raw, "
        "eligible_auc_schools_json, eligible_majors_json, excluded_majors_json, min_gpa_host, "
        "instruction_languages_json, english_course_availability, english_proficiency_and_rhet1020, "
        "host_language_requirement, available_terms_json, fall_term_dates, spring_term_dates, "
        "final_semester_allowed, seat_quota, tuition_model, est_monthly_living_currency, "
        "est_monthly_living_min, est_monthly_living_max, living_cost_usd_est, "
        "mandatory_host_fees_and_insurance, housing_availability, housing_notes, "
        "host_grants_or_stipends, citizenship_restrictions, visa_lead_time_and_financial_proof, "
        "official_website_url, exchange_page_url, course_catalog_url, english_course_list_url, "
        "credit_system_and_load, course_restrictions_note, required_application_docs_json, "
        "academic_style, international_support_services, nomination_deadlines, "
        "fact_sheet_match_status, fact_sheet_files_json "
        "FROM universities ORDER BY official_name ASC";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        AdminPartnerDetail p;
        p.slug = colText(stmt, 0);
        p.officialName = colText(stmt, 1);
        if (p.officialName.empty())
            p.officialName = p.slug;
        p.shortName = colText(stmt, 2);
        p.country = colText(stmt, 3);
        p.stateOrProvince = colText(stmt, 4);
        p.city = colText(stmt, 5);
        p.region = colText(stmt, 6);
        p.agreementStatus = colText(stmt, 7);
        p.studentLevels = parseJsonStringArray(colText(stmt, 8));
        p.minStandingNote = colText(stmt, 9);
        p.agreementScope = colText(stmt, 10);
        p.eligibleAucSchools = parseJsonStringArray(colText(stmt, 11));
        p.eligibleMajors = parseJsonStringArray(colText(stmt, 12));
        p.excludedMajors = parseJsonStringArray(colText(stmt, 13));
        p.minGpaKnown = !colIsNull(stmt, 14);
        p.minGpa = p.minGpaKnown ? colDouble(stmt, 14, 0.0) : 0.0;
        p.instructionLanguages = parseJsonStringArray(colText(stmt, 15));
        p.englishCourseAvailability = colText(stmt, 16);
        p.englishProficiency = colText(stmt, 17);
        p.hostLanguageRequirement = colText(stmt, 18);
        p.availableTerms = parseJsonStringArray(colText(stmt, 19));
        p.fallTermDates = colText(stmt, 20);
        p.springTermDates = colText(stmt, 21);
        p.finalSemesterAllowed = colInt(stmt, 22, -1);
        p.seatQuotaSeeded = colIsNull(stmt, 23) ? -1 : colInt(stmt, 23, -1);
        p.tuitionModel = colText(stmt, 24);
        p.estMonthlyLocal = formatMonthlyLocal(
            colText(stmt, 25),
            !colIsNull(stmt, 26), colDouble(stmt, 26, 0.0),
            !colIsNull(stmt, 27), colDouble(stmt, 27, 0.0));
        p.livingCostUsdKnown = !colIsNull(stmt, 28);
        p.livingCostUsd = p.livingCostUsdKnown ? colDouble(stmt, 28, 0.0) : 0.0;
        p.mandatoryHostFees = colText(stmt, 29);
        p.housingAvailability = colText(stmt, 30);
        p.housingNotes = colText(stmt, 31);
        p.hostGrants = colText(stmt, 32);
        p.citizenshipRestrictions = colText(stmt, 33);
        p.visaNotes = colText(stmt, 34);
        p.officialWebsiteUrl = colText(stmt, 35);
        p.exchangePageUrl = colText(stmt, 36);
        p.courseCatalogUrl = colText(stmt, 37);
        p.englishCourseListUrl = colText(stmt, 38);
        p.creditSystemAndLoad = colText(stmt, 39);
        p.courseRestrictionsNote = colText(stmt, 40);
        p.requiredDocs = parseJsonStringArray(colText(stmt, 41));
        p.academicStyle = colText(stmt, 42);
        p.internationalSupport = colText(stmt, 43);
        p.nominationDeadlines = colText(stmt, 44);
        p.factSheetStatus = colText(stmt, 45);
        p.factSheetFilesCount = parseJsonStringArray(colText(stmt, 46)).size();
        result.push_back(p);
    }

    sqlite3_finalize(stmt);
    return result;
}

Vector<AdminSeatCount> AdminRepo::loadSeatOverrides()
{
    Vector<AdminSeatCount> result;
    if (!db)
        return result;

    const char *sql = "SELECT slug, seats FROM admin_seat_overrides";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        AdminSeatCount c;
        c.slug = colText(stmt, 0);
        c.seats = colInt(stmt, 1, 0);
        result.push_back(c);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool AdminRepo::saveSeatOverride(const string &slug, int seats, const string &updatedAt)
{
    if (!db)
        return false;
    const char *sql =
        "INSERT OR REPLACE INTO admin_seat_overrides (slug, seats, updated_at) "
        "VALUES (?, ?, ?)";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bindTextOrNull(stmt, 1, slug);
    sqlite3_bind_int(stmt, 2, seats);
    bindTextOrNull(stmt, 3, updatedAt);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

Vector<StudentRecord> AdminRepo::loadStudents()
{
    Vector<StudentRecord> result;
    if (!db)
        return result;

    const char *sql =
        "SELECT email, name, auc_id, phone, level, major, "
        "gpa, has_gpa, credits, has_credits, "
        "major_declared, rhet110, rhet120, final_semester, "
        "academic_probation, disciplinary_probation, "
        "english_complete, first_semester_no_gpa, eligibility_saved, "
        "budget, has_budget, updated_at "
        "FROM students";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        StudentRecord r;
        r.email = colText(stmt, 0);
        r.name = colText(stmt, 1);
        r.aucId = colText(stmt, 2);
        r.phone = colText(stmt, 3);
        r.level = colText(stmt, 4);
        r.major = colText(stmt, 5);
        r.gpa = colDouble(stmt, 6, 0.0);
        r.hasGpa = colInt(stmt, 7) == 1;
        r.credits = colInt(stmt, 8, 0);
        r.hasCredits = colInt(stmt, 9) == 1;
        r.majorDeclared = colInt(stmt, 10) == 1;
        r.rhet110 = colInt(stmt, 11) == 1;
        r.rhet120 = colInt(stmt, 12) == 1;
        r.finalSemester = colInt(stmt, 13) == 1;
        r.academicProbation = colInt(stmt, 14) == 1;
        r.disciplinaryProbation = colInt(stmt, 15) == 1;
        r.englishComplete = colInt(stmt, 16) == 1;
        r.firstSemesterNoGpa = colInt(stmt, 17) == 1;
        r.eligibilitySaved = colInt(stmt, 18) == 1;
        r.budget = colInt(stmt, 19, 0);
        r.hasBudget = colInt(stmt, 20) == 1;
        r.updatedAt = colText(stmt, 21);
        result.push_back(r);
    }

    sqlite3_finalize(stmt);
    return result;
}

Vector<Application> AdminRepo::loadApplications()
{
    Vector<Application> result;
    if (!db)
        return result;

    const char *sql =
        "SELECT student_email, first_name, surname, date_of_birth, nationality, "
        "second_nationality, gender, phone, auc_id, gpa, level, major, rhet1020, "
        "expected_graduation, accommodation, scholarship, program_type, "
        "pref1, pref2, pref3, "
        "guardian_name, guardian_relation, guardian_phone, guardian_email, guardian_language, "
        "q_preparedness, q_goals, travel_experience, q_adaptability, q_teamwork, "
        "q_reflection, q_finance, budget, prior, declaration, status, good_standing, "
        "submitted_at, created_at, updated_at "
        "FROM applications ORDER BY COALESCE(submitted_at, updated_at, created_at) DESC, student_email ASC";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        Application a;
        a.studentEmail = colText(stmt, 0);
        a.firstName = colText(stmt, 1);
        a.surname = colText(stmt, 2);
        a.dateOfBirth = colText(stmt, 3);
        a.nationality = colText(stmt, 4);
        a.secondNationality = colText(stmt, 5);
        a.gender = colText(stmt, 6);
        a.phone = colText(stmt, 7);
        a.aucId = colText(stmt, 8);
        a.gpa = colDouble(stmt, 9, 0.0);
        a.level = colText(stmt, 10);
        a.major = colText(stmt, 11);
        a.rhet1020 = colText(stmt, 12);
        a.expectedGraduation = colText(stmt, 13);
        a.accommodation = colInt(stmt, 14) == 1;
        a.scholarship = colText(stmt, 15);
        a.programType = colText(stmt, 16);
        a.pref[0] = colText(stmt, 17);
        a.pref[1] = colText(stmt, 18);
        a.pref[2] = colText(stmt, 19);
        a.guardianName = colText(stmt, 20);
        a.guardianRelation = colText(stmt, 21);
        a.guardianPhone = colText(stmt, 22);
        a.guardianEmail = colText(stmt, 23);
        a.guardianLanguage = colText(stmt, 24);
        a.qPreparedness = colText(stmt, 25);
        a.qGoals = colText(stmt, 26);
        a.travelExperience = colText(stmt, 27);
        a.qAdaptability = colText(stmt, 28);
        a.qTeamwork = colText(stmt, 29);
        a.qReflection = colText(stmt, 30);
        a.qFinance = colText(stmt, 31);
        a.budget = colInt(stmt, 32, 0);
        a.prior = colInt(stmt, 33) == 1;
        a.declaration = colInt(stmt, 34) == 1;
        a.status = colText(stmt, 35);
        a.goodStanding = colInt(stmt, 36, -1);
        a.submittedAt = colText(stmt, 37);
        a.createdAt = colText(stmt, 38);
        a.updatedAt = colText(stmt, 39);
        result.push_back(a);
    }

    sqlite3_finalize(stmt);
    return result;
}

Vector<ApplicationDocument> AdminRepo::loadDocuments()
{
    Vector<ApplicationDocument> result;
    if (!db)
        return result;

    const char *sql = "SELECT student_email, doc_type, file_path, uploaded_at FROM application_documents";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ApplicationDocument d;
        d.studentEmail = colText(stmt, 0);
        d.docType = colText(stmt, 1);
        d.filePath = colText(stmt, 2);
        d.uploadedAt = colText(stmt, 3);
        result.push_back(d);
    }
    sqlite3_finalize(stmt);
    return result;
}

Vector<LetterRequest> AdminRepo::loadLetters()
{
    Vector<LetterRequest> result;
    if (!db)
        return result;

    const char *sql =
        "SELECT id, student_email, doctor_name, doctor_email, status, letter_text, requested_at, answered_at "
        "FROM letters";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        LetterRequest l;
        l.id = colInt(stmt, 0, -1);
        l.studentEmail = colText(stmt, 1);
        l.doctorName = colText(stmt, 2);
        l.doctorEmail = colText(stmt, 3);
        l.status = colText(stmt, 4);
        l.letterText = colText(stmt, 5);
        l.requestedAt = colText(stmt, 6);
        l.answeredAt = colText(stmt, 7);
        result.push_back(l);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool AdminRepo::updateApplicationReviewFields(const string &email,
                                             const string &status,
                                             int goodStanding,
                                             const string &updatedAt)
{
    if (!db)
        return false;
    const char *sql =
        "UPDATE applications SET status = ?, good_standing = ?, updated_at = ? "
        "WHERE student_email = ?";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bindTextOrNull(stmt, 1, status);
    if (goodStanding < 0)
        sqlite3_bind_null(stmt, 2);
    else
        sqlite3_bind_int(stmt, 2, goodStanding);
    bindTextOrNull(stmt, 3, updatedAt);
    bindTextOrNull(stmt, 4, email);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db) > 0);
    sqlite3_finalize(stmt);
    return ok;
}

Vector<AdminReviewRecord> AdminRepo::loadReviews()
{
    Vector<AdminReviewRecord> result;
    if (!db)
        return result;

    const char *sql =
        "SELECT student_email, statement_rating, has_statement_rating, notes, is_demo, updated_at "
        "FROM admin_application_reviews";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        AdminReviewRecord r;
        r.studentEmail = colText(stmt, 0);
        r.statementRating = colDouble(stmt, 1, 0.0);
        r.hasStatementRating = colInt(stmt, 2) == 1 && !colIsNull(stmt, 1);
        r.notes = colText(stmt, 3);
        r.isDemo = colInt(stmt, 4) == 1;
        r.updatedAt = colText(stmt, 5);
        result.push_back(r);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool AdminRepo::saveReview(const AdminReviewRecord &r)
{
    if (!db)
        return false;
    const char *sql =
        "INSERT OR REPLACE INTO admin_application_reviews "
        "(student_email, statement_rating, has_statement_rating, notes, is_demo, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bindTextOrNull(stmt, 1, r.studentEmail);
    if (r.hasStatementRating)
        sqlite3_bind_double(stmt, 2, r.statementRating);
    else
        sqlite3_bind_null(stmt, 2);
    sqlite3_bind_int(stmt, 3, r.hasStatementRating ? 1 : 0);
    bindTextOrNull(stmt, 4, r.notes);
    sqlite3_bind_int(stmt, 5, r.isDemo ? 1 : 0);
    bindTextOrNull(stmt, 6, r.updatedAt);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

Vector<AdminSavedRunSummary> AdminRepo::loadSavedRuns()
{
    Vector<AdminSavedRunSummary> result;
    if (!db)
        return result;

    const char *sql =
        "SELECT id, run_code, cycle, title, mode, allow_unranked, "
        "w_pref, w_gpa, w_statement, w_fairness, default_seats, status, "
        "placed_count, eligible_count, hk_max, first_choice_count, second_choice_count, "
        "third_choice_count, unranked_count, avg_score, created_by, created_at "
        "FROM admin_match_runs ORDER BY id DESC";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        AdminSavedRunSummary r;
        r.id = colInt(stmt, 0);
        r.runCode = colText(stmt, 1);
        r.cycle = colText(stmt, 2);
        r.title = colText(stmt, 3);
        r.mode = colText(stmt, 4);
        r.allowUnranked = colInt(stmt, 5) == 1;
        r.weights.preference = colDouble(stmt, 6);
        r.weights.gpa = colDouble(stmt, 7);
        r.weights.statement = colDouble(stmt, 8);
        r.weights.fairness = colDouble(stmt, 9);
        r.defaultSeats = colInt(stmt, 10, 2);
        r.status = colText(stmt, 11);
        r.placedCount = colInt(stmt, 12);
        r.eligibleCount = colInt(stmt, 13);
        r.hkMax = colInt(stmt, 14);
        r.firstChoiceCount = colInt(stmt, 15);
        r.secondChoiceCount = colInt(stmt, 16);
        r.thirdChoiceCount = colInt(stmt, 17);
        r.unrankedCount = colInt(stmt, 18);
        r.avgScore = colDouble(stmt, 19);
        r.createdBy = colText(stmt, 20);
        r.createdAt = colText(stmt, 21);
        r.firstChoicePct = (r.placedCount > 0)
                               ? (100.0 * static_cast<double>(r.firstChoiceCount) / r.placedCount)
                               : 0.0;
        result.push_back(r);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool AdminRepo::loadSavedRunDetail(int runId, AdminSavedRunDetail &out)
{
    if (!db)
        return false;

    const char *runSql =
        "SELECT id, run_code, cycle, title, mode, allow_unranked, "
        "w_pref, w_gpa, w_statement, w_fairness, default_seats, status, "
        "placed_count, eligible_count, hk_max, first_choice_count, second_choice_count, "
        "third_choice_count, unranked_count, avg_score, created_by, created_at "
        "FROM admin_match_runs WHERE id = ?";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, runSql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(stmt, 1, runId);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return false;
    }

    AdminSavedRunDetail detail;
    detail.summary.id = colInt(stmt, 0);
    detail.summary.runCode = colText(stmt, 1);
    detail.summary.cycle = colText(stmt, 2);
    detail.summary.title = colText(stmt, 3);
    detail.summary.mode = colText(stmt, 4);
    detail.summary.allowUnranked = colInt(stmt, 5) == 1;
    detail.summary.weights.preference = colDouble(stmt, 6);
    detail.summary.weights.gpa = colDouble(stmt, 7);
    detail.summary.weights.statement = colDouble(stmt, 8);
    detail.summary.weights.fairness = colDouble(stmt, 9);
    detail.summary.defaultSeats = colInt(stmt, 10, 2);
    detail.summary.status = colText(stmt, 11);
    detail.summary.placedCount = colInt(stmt, 12);
    detail.summary.eligibleCount = colInt(stmt, 13);
    detail.summary.hkMax = colInt(stmt, 14);
    detail.summary.firstChoiceCount = colInt(stmt, 15);
    detail.summary.secondChoiceCount = colInt(stmt, 16);
    detail.summary.thirdChoiceCount = colInt(stmt, 17);
    detail.summary.unrankedCount = colInt(stmt, 18);
    detail.summary.avgScore = colDouble(stmt, 19);
    detail.summary.createdBy = colText(stmt, 20);
    detail.summary.createdAt = colText(stmt, 21);
    detail.summary.firstChoicePct = (detail.summary.placedCount > 0)
                                      ? (100.0 * static_cast<double>(detail.summary.firstChoiceCount) / detail.summary.placedCount)
                                      : 0.0;
    sqlite3_finalize(stmt);

    // Load assignments
    const char *assignSql =
        "SELECT student_email, student_name, student_auc_id, student_major, student_gpa, "
        "university_slug, university_name, seat_label, pref_rank, score "
        "FROM admin_match_assignments WHERE run_id = ? ORDER BY id ASC";
    if (sqlite3_prepare_v2(db, assignSql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, runId);
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            AdminPlacementRow p;
            p.studentEmail = colText(stmt, 0);
            p.studentName = colText(stmt, 1);
            p.studentAucId = colText(stmt, 2);
            p.studentMajor = colText(stmt, 3);
            p.studentGpa = colDouble(stmt, 4);
            p.universitySlug = colText(stmt, 5);
            p.universityName = colText(stmt, 6);
            p.seatLabel = colText(stmt, 7);
            p.prefRank = colInt(stmt, 8);
            p.rankLabel = (p.prefRank == 1)   ? "#1 Choice"
                          : (p.prefRank == 2) ? "#2 Choice"
                          : (p.prefRank == 3) ? "#3 Choice"
                                              : "Unranked Eligible";
            p.score = colDouble(stmt, 9);
            detail.placements.push_back(p);
        }
        sqlite3_finalize(stmt);
    }

    // Load unplaced rows
    const char *unplacedSql =
        "SELECT student_email, student_name, student_gpa, prefs_summary, bottleneck, reason "
        "FROM admin_match_unplaced WHERE run_id = ? ORDER BY id ASC";
    if (sqlite3_prepare_v2(db, unplacedSql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, runId);
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            AdminUnplacedRow u;
            u.studentEmail = colText(stmt, 0);
            u.studentName = colText(stmt, 1);
            u.studentGpa = colDouble(stmt, 2);
            u.prefsSummary = colText(stmt, 3);
            u.bottleneck = colText(stmt, 4);
            u.reason = colText(stmt, 5);
            detail.unplaced.push_back(u);
        }
        sqlite3_finalize(stmt);
    }

    out = detail;
    return true;
}

bool AdminRepo::insertSavedRun(AdminSavedRunDetail &run)
{
    if (!db)
        return false;
    if (!execSql(db, "BEGIN TRANSACTION;"))
        return false;

    const char *insertRunSql =
        "INSERT INTO admin_match_runs ("
        "run_code, cycle, title, mode, allow_unranked, w_pref, w_gpa, w_statement, w_fairness, "
        "default_seats, status, placed_count, eligible_count, hk_max, first_choice_count, "
        "second_choice_count, third_choice_count, unranked_count, avg_score, created_by, created_at"
        ") VALUES (?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?,?)";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, insertRunSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        execSql(db, "ROLLBACK;");
        return false;
    }

    const AdminSavedRunSummary &s = run.summary;
    bindTextOrNull(stmt, 1, s.runCode);
    bindTextOrNull(stmt, 2, s.cycle);
    bindTextOrNull(stmt, 3, s.title);
    bindTextOrNull(stmt, 4, s.mode);
    sqlite3_bind_int(stmt, 5, s.allowUnranked ? 1 : 0);
    sqlite3_bind_double(stmt, 6, s.weights.preference);
    sqlite3_bind_double(stmt, 7, s.weights.gpa);
    sqlite3_bind_double(stmt, 8, s.weights.statement);
    sqlite3_bind_double(stmt, 9, s.weights.fairness);
    sqlite3_bind_int(stmt, 10, s.defaultSeats);
    bindTextOrNull(stmt, 11, s.status);
    sqlite3_bind_int(stmt, 12, s.placedCount);
    sqlite3_bind_int(stmt, 13, s.eligibleCount);
    sqlite3_bind_int(stmt, 14, s.hkMax);
    sqlite3_bind_int(stmt, 15, s.firstChoiceCount);
    sqlite3_bind_int(stmt, 16, s.secondChoiceCount);
    sqlite3_bind_int(stmt, 17, s.thirdChoiceCount);
    sqlite3_bind_int(stmt, 18, s.unrankedCount);
    sqlite3_bind_double(stmt, 19, s.avgScore);
    bindTextOrNull(stmt, 20, s.createdBy);
    bindTextOrNull(stmt, 21, s.createdAt);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (!ok)
    {
        execSql(db, "ROLLBACK;");
        return false;
    }

    int runId = static_cast<int>(sqlite3_last_insert_rowid(db));
    run.summary.id = runId;

    const char *assignSql =
        "INSERT INTO admin_match_assignments ("
        "run_id, student_email, student_name, student_auc_id, student_major, student_gpa, "
        "university_slug, university_name, seat_label, pref_rank, score"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db, assignSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        execSql(db, "ROLLBACK;");
        return false;
    }

    for (int i = 0; i < run.placements.size(); ++i)
    {
        const AdminPlacementRow &p = run.placements[i];
        sqlite3_bind_int(stmt, 1, runId);
        bindTextOrNull(stmt, 2, p.studentEmail);
        bindTextOrNull(stmt, 3, p.studentName);
        bindTextOrNull(stmt, 4, p.studentAucId);
        bindTextOrNull(stmt, 5, p.studentMajor);
        sqlite3_bind_double(stmt, 6, p.studentGpa);
        bindTextOrNull(stmt, 7, p.universitySlug);
        bindTextOrNull(stmt, 8, p.universityName);
        bindTextOrNull(stmt, 9, p.seatLabel);
        sqlite3_bind_int(stmt, 10, p.prefRank);
        sqlite3_bind_double(stmt, 11, p.score);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            execSql(db, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    const char *unplacedSql =
        "INSERT INTO admin_match_unplaced ("
        "run_id, student_email, student_name, student_gpa, prefs_summary, bottleneck, reason"
        ") VALUES (?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db, unplacedSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        execSql(db, "ROLLBACK;");
        return false;
    }

    for (int i = 0; i < run.unplaced.size(); ++i)
    {
        const AdminUnplacedRow &u = run.unplaced[i];
        sqlite3_bind_int(stmt, 1, runId);
        bindTextOrNull(stmt, 2, u.studentEmail);
        bindTextOrNull(stmt, 3, u.studentName);
        sqlite3_bind_double(stmt, 4, u.studentGpa);
        bindTextOrNull(stmt, 5, u.prefsSummary);
        bindTextOrNull(stmt, 6, u.bottleneck);
        bindTextOrNull(stmt, 7, u.reason);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            execSql(db, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    if (!execSql(db, "COMMIT;"))
    {
        execSql(db, "ROLLBACK;");
        return false;
    }
    return true;
}

bool AdminRepo::archivePublishedRuns()
{
    if (!db)
        return false;
    return execSql(db, "UPDATE admin_match_runs SET status = 'Archived' WHERE status = 'Published';");
}

bool AdminRepo::updateRunStatus(int runId, const string &status)
{
    if (!db)
        return false;
    const char *sql = "UPDATE admin_match_runs SET status = ? WHERE id = ?";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    bindTextOrNull(stmt, 1, status);
    sqlite3_bind_int(stmt, 2, runId);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db) > 0);
    sqlite3_finalize(stmt);
    return ok;
}

bool AdminRepo::deleteSavedRun(int runId)
{
    if (!db || runId <= 0)
        return false;
    if (!execSql(db, "BEGIN TRANSACTION;"))
        return false;

    const char *delSqls[3] = {
        "DELETE FROM admin_match_assignments WHERE run_id = ?",
        "DELETE FROM admin_match_unplaced WHERE run_id = ?",
        "DELETE FROM admin_match_runs WHERE id = ?"
    };

    int deletedRuns = 0;
    for (int k = 0; k < 3; ++k)
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(db, delSqls[k], -1, &stmt, nullptr) != SQLITE_OK)
        {
            execSql(db, "ROLLBACK;");
            return false;
        }
        sqlite3_bind_int(stmt, 1, runId);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            execSql(db, "ROLLBACK;");
            return false;
        }
        if (k == 2)
            deletedRuns = sqlite3_changes(db);
        sqlite3_finalize(stmt);
    }

    if (deletedRuns <= 0 || !execSql(db, "COMMIT;"))
    {
        execSql(db, "ROLLBACK;");
        return false;
    }
    return true;
}

bool AdminRepo::seedDemoBatch(const Vector<StudentRecord> &students,
                              const Vector<Application> &applications,
                              const Vector<ApplicationDocument> &documents,
                              const Vector<LetterRequest> &letters,
                              const Vector<AdminReviewRecord> &reviews)
{
    if (!db)
        return false;
    if (!execSql(db, "BEGIN TRANSACTION;"))
        return false;

    sqlite3_stmt *stmt = nullptr;

    // 1. Insert demo students
    const char *studSql =
        "INSERT OR REPLACE INTO students ("
        "email, name, auc_id, phone, level, major, gpa, has_gpa, credits, has_credits, "
        "major_declared, rhet110, rhet120, final_semester, academic_probation, disciplinary_probation, "
        "english_complete, first_semester_no_gpa, eligibility_saved, budget, has_budget, updated_at"
        ") VALUES (?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, ?,?)";
    if (sqlite3_prepare_v2(db, studSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        execSql(db, "ROLLBACK;");
        return false;
    }
    for (int i = 0; i < students.size(); ++i)
    {
        const StudentRecord &r = students[i];
        bindTextOrNull(stmt, 1, r.email);
        bindTextOrNull(stmt, 2, r.name);
        bindTextOrNull(stmt, 3, r.aucId);
        bindTextOrNull(stmt, 4, r.phone);
        bindTextOrNull(stmt, 5, r.level);
        bindTextOrNull(stmt, 6, r.major);
        if (r.hasGpa)
            sqlite3_bind_double(stmt, 7, r.gpa);
        else
            sqlite3_bind_null(stmt, 7);
        sqlite3_bind_int(stmt, 8, r.hasGpa ? 1 : 0);
        if (r.hasCredits)
            sqlite3_bind_int(stmt, 9, r.credits);
        else
            sqlite3_bind_null(stmt, 9);
        sqlite3_bind_int(stmt, 10, r.hasCredits ? 1 : 0);
        sqlite3_bind_int(stmt, 11, r.majorDeclared ? 1 : 0);
        sqlite3_bind_int(stmt, 12, r.rhet110 ? 1 : 0);
        sqlite3_bind_int(stmt, 13, r.rhet120 ? 1 : 0);
        sqlite3_bind_int(stmt, 14, r.finalSemester ? 1 : 0);
        sqlite3_bind_int(stmt, 15, r.academicProbation ? 1 : 0);
        sqlite3_bind_int(stmt, 16, r.disciplinaryProbation ? 1 : 0);
        sqlite3_bind_int(stmt, 17, r.englishComplete ? 1 : 0);
        sqlite3_bind_int(stmt, 18, r.firstSemesterNoGpa ? 1 : 0);
        sqlite3_bind_int(stmt, 19, r.eligibilitySaved ? 1 : 0);
        sqlite3_bind_int(stmt, 20, r.budget);
        sqlite3_bind_int(stmt, 21, r.hasBudget ? 1 : 0);
        bindTextOrNull(stmt, 22, r.updatedAt);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            execSql(db, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    // 2. Insert demo applications
    const char *appSql =
        "INSERT OR REPLACE INTO applications ("
        "student_email, first_name, surname, date_of_birth, nationality, "
        "second_nationality, gender, phone, auc_id, gpa, level, major, rhet1020, "
        "expected_graduation, accommodation, scholarship, program_type, "
        "pref1, pref2, pref3, "
        "guardian_name, guardian_relation, guardian_phone, guardian_email, guardian_language, "
        "q_preparedness, q_goals, travel_experience, q_adaptability, q_teamwork, "
        "q_reflection, q_finance, budget, prior, declaration, status, good_standing, "
        "submitted_at, created_at, updated_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, "
        "?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db, appSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        execSql(db, "ROLLBACK;");
        return false;
    }
    for (int i = 0; i < applications.size(); ++i)
    {
        const Application &a = applications[i];
        bindTextOrNull(stmt, 1, a.studentEmail);
        bindTextOrNull(stmt, 2, a.firstName);
        bindTextOrNull(stmt, 3, a.surname);
        bindTextOrNull(stmt, 4, a.dateOfBirth);
        bindTextOrNull(stmt, 5, a.nationality);
        bindTextOrNull(stmt, 6, a.secondNationality);
        bindTextOrNull(stmt, 7, a.gender);
        bindTextOrNull(stmt, 8, a.phone);
        bindTextOrNull(stmt, 9, a.aucId);
        sqlite3_bind_double(stmt, 10, a.gpa);
        bindTextOrNull(stmt, 11, a.level);
        bindTextOrNull(stmt, 12, a.major);
        bindTextOrNull(stmt, 13, a.rhet1020);
        bindTextOrNull(stmt, 14, a.expectedGraduation);
        sqlite3_bind_int(stmt, 15, a.accommodation ? 1 : 0);
        bindTextOrNull(stmt, 16, a.scholarship);
        bindTextOrNull(stmt, 17, a.programType);
        bindTextOrNull(stmt, 18, a.pref[0]);
        bindTextOrNull(stmt, 19, a.pref[1]);
        bindTextOrNull(stmt, 20, a.pref[2]);
        bindTextOrNull(stmt, 21, a.guardianName);
        bindTextOrNull(stmt, 22, a.guardianRelation);
        bindTextOrNull(stmt, 23, a.guardianPhone);
        bindTextOrNull(stmt, 24, a.guardianEmail);
        bindTextOrNull(stmt, 25, a.guardianLanguage);
        bindTextOrNull(stmt, 26, a.qPreparedness);
        bindTextOrNull(stmt, 27, a.qGoals);
        bindTextOrNull(stmt, 28, a.travelExperience);
        bindTextOrNull(stmt, 29, a.qAdaptability);
        bindTextOrNull(stmt, 30, a.qTeamwork);
        bindTextOrNull(stmt, 31, a.qReflection);
        bindTextOrNull(stmt, 32, a.qFinance);
        sqlite3_bind_int(stmt, 33, a.budget);
        sqlite3_bind_int(stmt, 34, a.prior ? 1 : 0);
        sqlite3_bind_int(stmt, 35, a.declaration ? 1 : 0);
        bindTextOrNull(stmt, 36, a.status);
        if (a.goodStanding < 0)
            sqlite3_bind_null(stmt, 37);
        else
            sqlite3_bind_int(stmt, 37, a.goodStanding);
        bindTextOrNull(stmt, 38, a.submittedAt);
        bindTextOrNull(stmt, 39, a.createdAt);
        bindTextOrNull(stmt, 40, a.updatedAt);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            execSql(db, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    // 3. Insert demo documents
    const char *docSql =
        "INSERT OR REPLACE INTO application_documents (student_email, doc_type, file_path, uploaded_at) "
        "VALUES (?,?,?,?)";
    if (sqlite3_prepare_v2(db, docSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        execSql(db, "ROLLBACK;");
        return false;
    }
    for (int i = 0; i < documents.size(); ++i)
    {
        const ApplicationDocument &d = documents[i];
        bindTextOrNull(stmt, 1, d.studentEmail);
        bindTextOrNull(stmt, 2, d.docType);
        bindTextOrNull(stmt, 3, d.filePath);
        bindTextOrNull(stmt, 4, d.uploadedAt);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            execSql(db, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    // 4. Insert demo recommendation letters
    const char *letterSql =
        "INSERT OR IGNORE INTO letters (student_email, doctor_name, doctor_email, status, letter_text, requested_at, answered_at) "
        "VALUES (?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db, letterSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        execSql(db, "ROLLBACK;");
        return false;
    }
    for (int i = 0; i < letters.size(); ++i)
    {
        const LetterRequest &l = letters[i];
        bindTextOrNull(stmt, 1, l.studentEmail);
        bindTextOrNull(stmt, 2, l.doctorName);
        bindTextOrNull(stmt, 3, l.doctorEmail);
        bindTextOrNull(stmt, 4, l.status);
        bindTextOrNull(stmt, 5, l.letterText);
        bindTextOrNull(stmt, 6, l.requestedAt);
        bindTextOrNull(stmt, 7, l.answeredAt);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            execSql(db, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    // 5. Insert admin reviews marked is_demo = 1
    const char *revSql =
        "INSERT OR REPLACE INTO admin_application_reviews "
        "(student_email, statement_rating, has_statement_rating, notes, is_demo, updated_at) "
        "VALUES (?,?,?,?,1,?)";
    if (sqlite3_prepare_v2(db, revSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        execSql(db, "ROLLBACK;");
        return false;
    }
    for (int i = 0; i < reviews.size(); ++i)
    {
        const AdminReviewRecord &r = reviews[i];
        bindTextOrNull(stmt, 1, r.studentEmail);
        if (r.hasStatementRating)
            sqlite3_bind_double(stmt, 2, r.statementRating);
        else
            sqlite3_bind_null(stmt, 2);
        sqlite3_bind_int(stmt, 3, r.hasStatementRating ? 1 : 0);
        bindTextOrNull(stmt, 4, r.notes);
        bindTextOrNull(stmt, 5, r.updatedAt);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            execSql(db, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    if (!execSql(db, "COMMIT;"))
    {
        execSql(db, "ROLLBACK;");
        return false;
    }
    return true;
}

int AdminRepo::clearDemoBatch()
{
    if (!db)
        return -1;
    if (!execSql(db, "BEGIN TRANSACTION;"))
        return -1;

    execSql(db, "DELETE FROM letters WHERE student_email IN (SELECT student_email FROM admin_application_reviews WHERE is_demo = 1);");
    execSql(db, "DELETE FROM application_documents WHERE student_email IN (SELECT student_email FROM admin_application_reviews WHERE is_demo = 1);");
    execSql(db, "DELETE FROM applications WHERE student_email IN (SELECT student_email FROM admin_application_reviews WHERE is_demo = 1);");
    execSql(db, "DELETE FROM students WHERE email IN (SELECT student_email FROM admin_application_reviews WHERE is_demo = 1);");
    // Optional discovery tables if present
    execSql(db, "DELETE FROM saved_universities WHERE student_email IN (SELECT student_email FROM admin_application_reviews WHERE is_demo = 1);");
    execSql(db, "DELETE FROM notifications WHERE student_email IN (SELECT student_email FROM admin_application_reviews WHERE is_demo = 1);");

    if (!execSql(db, "DELETE FROM admin_application_reviews WHERE is_demo = 1;"))
    {
        execSql(db, "ROLLBACK;");
        return -1;
    }
    int removed = sqlite3_changes(db);

    if (!execSql(db, "COMMIT;"))
    {
        execSql(db, "ROLLBACK;");
        return -1;
    }
    return removed;
}
