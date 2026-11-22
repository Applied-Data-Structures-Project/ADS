#include "services/admin_service.hpp"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <sstream>

#include "services/admin_allocator.hpp"
#include "services/admin_scoring.hpp"
#include "services/admin_seats.hpp"
#include "services/student_service.hpp"

using namespace std;

namespace
{

string toLower(const string &s)
{
    string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(tolower(static_cast<unsigned char>(out[i])));
    return out;
}

string toUpper(const string &s)
{
    string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(toupper(static_cast<unsigned char>(out[i])));
    return out;
}

bool containsIgnoreCase(const string &haystack, const string &needle)
{
    if (needle.empty())
        return true;
    return toLower(haystack).find(toLower(needle)) != string::npos;
}

bool vecContains(const Vector<string> &v, const string &target)
{
    for (int i = 0; i < v.size(); ++i)
    {
        if (v[i] == target)
            return true;
    }
    return false;
}

void pushUnique(Vector<string> &v, const string &val)
{
    if (!val.empty() && !vecContains(v, val))
        v.push_back(val);
}

// Extracts compact major code from formats like "Computer Science (CSCE)" or "Economics".
string extractMajorCode(const string &rawMajor)
{
    size_t open = rawMajor.rfind('(');
    size_t close = rawMajor.rfind(')');
    if (open != string::npos && close != string::npos && close > open + 1)
    {
        string inside = toUpper(rawMajor.substr(open + 1, close - open - 1));
        if (inside.size() >= 2 && inside.size() <= 8)
            return inside;
    }

    string lower = toLower(rawMajor);
    if (lower.find("computer science") != string::npos || lower == "csce" || lower == "cs")
        return "CSCE";
    if (lower.find("computer eng") != string::npos || lower == "ceng")
        return "CENG";
    if (lower.find("mechanical") != string::npos || lower == "meng")
        return "MENG";
    if (lower.find("industrial") != string::npos || lower == "ieng")
        return "IENG";
    if (lower.find("electronics") != string::npos || lower.find("electrical") != string::npos || lower == "eeng")
        return "EENG";
    if (lower.find("economics") != string::npos || lower == "econ")
        return "ECON";
    if (lower.find("finance") != string::npos || lower == "finc")
        return "FINC";
    if (lower.find("marketing") != string::npos || lower == "mktg")
        return "MKTG";
    if (lower.find("accounting") != string::npos || lower == "acct")
        return "ACCT";
    if (lower.find("business") != string::npos || lower == "bus" || lower == "mgmt")
        return "BUS";
    if (lower.find("political") != string::npos || lower == "pols")
        return "POLS";
    if (lower.find("architecture") != string::npos || lower == "arch")
        return "ARCH";
    if (lower.find("journalism") != string::npos || lower == "jrmc")
        return "JRMC";
    if (lower.find("integrated marketing") != string::npos || lower == "imc")
        return "IMC";
    if (lower.find("communication") != string::npos || lower == "mcom")
        return "MCOM";
    if (lower.find("psychology") != string::npos || lower == "psyc")
        return "PSYC";
    if (lower.find("data science") != string::npos || lower == "data")
        return "DATA";
    if (lower.find("biology") != string::npos || lower == "biol")
        return "BIOL";
    if (lower.find("law") != string::npos)
        return "LAW";
    if (lower.find("philosophy") != string::npos || lower == "phil")
        return "PHIL";
    if (lower.find("history") != string::npos || lower == "hist")
        return "HIST";
    return toUpper(rawMajor);
}

// Expands university-level discipline labels from `eligible_majors_json` into accepted AUC major codes
// while keeping the original labels so untouched AdminFitScorer::passesMajor matches accurately.
Vector<string> expandAcceptedMajors(const Vector<string> &rawMajors)
{
    Vector<string> out;
    if (rawMajors.size() == 0)
        return out;

    for (int i = 0; i < rawMajors.size(); ++i)
    {
        const string &entry = rawMajors[i];
        pushUnique(out, entry);
        string l = toLower(entry);
        if (l == "all")
        {
            pushUnique(out, "ALL");
            return out;
        }
        if (l.find("business") != string::npos || l.find("management") != string::npos)
        {
            pushUnique(out, "BUS");
            pushUnique(out, "FINC");
            pushUnique(out, "MKTG");
            pushUnique(out, "ACCT");
            pushUnique(out, "MGMT");
        }
        if (l.find("economics") != string::npos)
        {
            pushUnique(out, "ECON");
        }
        if (l.find("political") != string::npos)
        {
            pushUnique(out, "POLS");
        }
        if (l.find("journalism") != string::npos || l.find("communication") != string::npos ||
            l.find("media") != string::npos)
        {
            pushUnique(out, "JRMC");
            pushUnique(out, "IMC");
            pushUnique(out, "MCOM");
        }
        if (l.find("industrial engineering") != string::npos)
        {
            pushUnique(out, "IENG");
            pushUnique(out, "MENG");
        }
        if (l.find("engineering") != string::npos)
        {
            pushUnique(out, "CSCE");
            pushUnique(out, "CENG");
            pushUnique(out, "MENG");
            pushUnique(out, "IENG");
            pushUnique(out, "EENG");
        }
        if (l.find("computer") != string::npos)
        {
            pushUnique(out, "CSCE");
            pushUnique(out, "CENG");
            pushUnique(out, "DATA");
        }
        if (l.find("humanities") != string::npos)
        {
            pushUnique(out, "PHIL");
            pushUnique(out, "HIST");
            pushUnique(out, "ANTH");
            pushUnique(out, "PSYC");
            pushUnique(out, "POLS");
        }
        if (l.find("social science") != string::npos)
        {
            pushUnique(out, "PSYC");
            pushUnique(out, "POLS");
            pushUnique(out, "ECON");
            pushUnique(out, "ANTH");
            pushUnique(out, "SOC");
        }
        if (l.find("art") != string::npos || l.find("design") != string::npos)
        {
            pushUnique(out, "ARCH");
            pushUnique(out, "ART");
            pushUnique(out, "GD");
        }
        if (l.find("law") != string::npos)
        {
            pushUnique(out, "LAW");
            pushUnique(out, "POLS");
        }
    }
    return out;
}

string extractTermFromCycle(const string &cycle)
{
    if (containsIgnoreCase(cycle, "spring"))
        return "Spring";
    return "Fall";
}

string deriveStanding(const string &level, int credits, bool hasCredits, const string &expectedGrad)
{
    if (level == "GR")
        return "Graduate";
    if (hasCredits)
    {
        if (credits >= 90)
            return "Senior";
        if (credits >= 60)
            return "Junior";
        return "Sophomore";
    }
    if (expectedGrad.find("2027") != string::npos)
        return "Senior";
    if (expectedGrad.find("2028") != string::npos)
        return "Junior";
    return "Junior";
}

bool validAdminStatus(const string &s)
{
    return s == "Draft" || s == "Submitted" || s == "In Review" ||
           s == "Verified" || s == "Ineligible";
}

string modeLabel(const string &mode)
{
    if (mode == "best_fit" || mode == "highest_score" || mode == "most_placed" || mode == "shortlists")
        return mode;
    return "best_fit";
}

class DemoRng
{
public:
    explicit DemoRng(unsigned long long seed) : state(seed) {}
    unsigned int next()
    {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<unsigned int>(state >> 33);
    }
    int range(int lo, int hi)
    {
        return lo + static_cast<int>(next() % static_cast<unsigned int>(hi - lo + 1));
    }
    double unit()
    {
        return static_cast<double>(next()) / 2147483648.0;
    }

private:
    unsigned long long state;
};

} // namespace

AdminService::AdminService(AdminRepo &adminRepo,
                           ApplicationService &appService,
                           DiscoveryService &discoveryService)
    : adminRepo(adminRepo),
      appService(appService),
      discoveryService(discoveryService)
{
}

void AdminService::loadAll()
{
    partners = adminRepo.loadPartners();
    partnerIndexBySlug = HashMap<string, int>(partners.size() * 2 + 16);
    for (int i = 0; i < partners.size(); ++i)
        partnerIndexBySlug.insert(partners[i].slug, i);

    Vector<AdminSeatCount> overrides = adminRepo.loadSeatOverrides();
    seatOverrides = HashMap<string, int>(overrides.size() * 2 + 16);
    for (int i = 0; i < overrides.size(); ++i)
        seatOverrides.insert(overrides[i].slug, overrides[i].seats);

    Vector<AdminReviewRecord> revList = adminRepo.loadReviews();
    reviews = HashMap<string, AdminReviewRecord>(revList.size() * 2 + 16);
    for (int i = 0; i < revList.size(); ++i)
        reviews.insert(revList[i].studentEmail, revList[i]);

    savedRuns = adminRepo.loadSavedRuns();
}

Vector<AdminApplicationRow> AdminService::buildAllApplicationRows() const
{
    Vector<Application> apps = adminRepo.loadApplications();
    Vector<StudentRecord> studentRecs = adminRepo.loadStudents();
    Vector<ApplicationDocument> docs = adminRepo.loadDocuments();
    Vector<LetterRequest> letters = adminRepo.loadLetters();

    HashMap<string, StudentRecord> studentByEmail(studentRecs.size() * 2 + 16);
    for (int i = 0; i < studentRecs.size(); ++i)
        studentByEmail.insert(studentRecs[i].email, studentRecs[i]);

    HashMap<string, Vector<string>> docsByEmail(apps.size() * 2 + 16);
    for (int i = 0; i < docs.size(); ++i)
    {
        Vector<string> *list = docsByEmail.get(docs[i].studentEmail);
        if (list == nullptr)
        {
            Vector<string> fresh;
            fresh.push_back(docs[i].docType);
            docsByEmail.insert(docs[i].studentEmail, fresh);
        }
        else
        {
            pushUnique(*list, docs[i].docType);
        }
    }

    HashMap<string, int> recvLetters(apps.size() * 2 + 16);
    HashMap<string, int> pendLetters(apps.size() * 2 + 16);
    HashMap<string, int> declLetters(apps.size() * 2 + 16);
    for (int i = 0; i < letters.size(); ++i)
    {
        const LetterRequest &l = letters[i];
        if (l.status == "Received")
            recvLetters[l.studentEmail] = recvLetters.search(l.studentEmail) ? recvLetters.at(l.studentEmail) + 1 : 1;
        else if (l.status == "Pending")
            pendLetters[l.studentEmail] = pendLetters.search(l.studentEmail) ? pendLetters.at(l.studentEmail) + 1 : 1;
        else if (l.status == "Declined")
            declLetters[l.studentEmail] = declLetters.search(l.studentEmail) ? declLetters.at(l.studentEmail) + 1 : 1;
    }

    Vector<AdminApplicationRow> rows;
    rows.reserve(apps.size());

    for (int i = 0; i < apps.size(); ++i)
    {
        const Application &a = apps[i];
        const StudentRecord *s = studentByEmail.get(a.studentEmail);
        const AdminReviewRecord *rev = reviews.get(a.studentEmail);

        AdminApplicationRow row;
        row.studentEmail = a.studentEmail;
        row.firstName = a.firstName;
        row.surname = a.surname;
        row.fullName = (!a.firstName.empty() || !a.surname.empty())
                           ? (a.firstName + (a.firstName.empty() || a.surname.empty() ? "" : " ") + a.surname)
                           : ((s && !s->name.empty()) ? s->name : a.studentEmail);
        row.aucId = !a.aucId.empty() ? a.aucId : (s ? s->aucId : "");
        row.level = !a.level.empty() ? a.level : (s && !s->level.empty() ? s->level : "UG");
        row.major = !a.major.empty() ? a.major : (s ? s->major : "");
        row.gpa = (a.gpa > 0.0) ? a.gpa : ((s && s->hasGpa) ? s->gpa : 0.0);

        if (s != nullptr)
        {
            row.hasCredits = s->hasCredits;
            row.credits = s->credits;
            row.majorDeclared = s->majorDeclared;
            row.rhet110 = s->rhet110;
            row.rhet120 = s->rhet120;
            row.finalSemester = s->finalSemester;
            row.academicProbation = s->academicProbation;
            row.disciplinaryProbation = s->disciplinaryProbation;
            row.englishComplete = s->englishComplete;
            row.firstSemesterNoGpa = s->firstSemesterNoGpa;
            row.eligibilitySaved = s->eligibilitySaved;
        }
        else
        {
            row.hasCredits = true;
            row.credits = (row.level == "GR") ? 18 : 66;
            row.majorDeclared = true;
            row.rhet110 = true;
            row.rhet120 = true;
            row.englishComplete = true;
        }

        row.standing = deriveStanding(row.level, row.credits, row.hasCredits, a.expectedGraduation);
        row.rhet1020 = a.rhet1020;
        row.expectedGraduation = a.expectedGraduation;
        row.nationality = a.nationality;
        row.secondNationality = a.secondNationality;
        row.gender = a.gender;
        row.phone = !a.phone.empty() ? a.phone : (s ? s->phone : "");
        row.dateOfBirth = a.dateOfBirth;
        row.programType = a.programType;
        row.scholarship = a.scholarship;
        row.accommodation = a.accommodation;
        row.budget = (a.budget > 0) ? a.budget : ((s && s->hasBudget) ? s->budget : 0);
        row.prior = a.prior || (a.travelExperience == "Yes, Study Abroad");
        row.travelExperience = a.travelExperience;
        row.status = a.status.empty() ? "Draft" : a.status;
        row.goodStanding = a.goodStanding;
        row.declaration = a.declaration;
        row.submittedAt = a.submittedAt;
        row.updatedAt = a.updatedAt;

        for (int p = 0; p < 3; ++p)
        {
            row.prefSlugs[p] = a.pref[p];
            if (!a.pref[p].empty())
            {
                const int *uIdx = partnerIndexBySlug.get(a.pref[p]);
                row.prefNames[p] = (uIdx != nullptr) ? partners[*uIdx].officialName : a.pref[p];
            }
        }

        const Vector<string> *docList = docsByEmail.get(a.studentEmail);
        if (docList != nullptr)
            row.uploadedDocs = *docList;

        row.letterReceivedCount = recvLetters.search(a.studentEmail) ? recvLetters.at(a.studentEmail) : 0;
        row.letterPendingCount = pendLetters.search(a.studentEmail) ? pendLetters.at(a.studentEmail) : 0;
        row.letterDeclinedCount = declLetters.search(a.studentEmail) ? declLetters.at(a.studentEmail) : 0;

        row.guardianName = a.guardianName;
        row.guardianRelation = a.guardianRelation;
        row.guardianPhone = a.guardianPhone;
        row.guardianEmail = a.guardianEmail;
        row.guardianLanguage = a.guardianLanguage;
        row.qPreparedness = a.qPreparedness;
        row.qGoals = a.qGoals;
        row.qAdaptability = a.qAdaptability;
        row.qTeamwork = a.qTeamwork;
        row.qReflection = a.qReflection;
        row.qFinance = a.qFinance;

        if (rev != nullptr)
        {
            row.hasStatementRating = rev->hasStatementRating;
            row.statementRating = rev->statementRating;
            row.adminNotes = rev->notes;
            row.isDemo = rev->isDemo;
        }

        if (s != nullptr && s->eligibilitySaved)
            row.eligibilityReason = StudentService::checkEligibilityReason(*s);
        else if (row.level == "UG" && row.gpa < 2.8)
            row.eligibilityReason = "GPA is below the 2.8 minimum for UG students.";
        else if (row.level == "GR" && row.gpa < 3.2)
            row.eligibilityReason = "GPA is below the 3.2 minimum for GR students.";

        row.flagged = (row.status == "Ineligible" || !row.eligibilityReason.empty() ||
                       row.status == "Submitted" || row.status == "In Review");
        rows.push_back(row);
    }

    return rows;
}

void AdminService::computeDemandMaps(const Vector<AdminApplicationRow> &rows,
                                     HashMap<string, int> &firstChoiceOut,
                                     HashMap<string, int> &totalDemandOut) const
{
    firstChoiceOut = HashMap<string, int>(partners.size() * 2 + 16);
    totalDemandOut = HashMap<string, int>(partners.size() * 2 + 16);

    for (int i = 0; i < rows.size(); ++i)
    {
        for (int p = 0; p < 3; ++p)
        {
            const string &slug = rows[i].prefSlugs[p];
            if (slug.empty())
                continue;
            if (p == 0)
                firstChoiceOut[slug] = firstChoiceOut.search(slug) ? firstChoiceOut.at(slug) + 1 : 1;
            totalDemandOut[slug] = totalDemandOut.search(slug) ? totalDemandOut.at(slug) + 1 : 1;
        }
    }
}

Vector<AdminUniversity> AdminService::buildAdminUniversities() const
{
    Vector<AdminUniversity> out;
    out.reserve(partners.size());
    for (int i = 0; i < partners.size(); ++i)
    {
        const AdminPartnerDetail &p = partners[i];
        AdminUniversity u;
        u.slug = p.slug;
        u.name = p.officialName;
        u.minGpaKnown = p.minGpaKnown;
        u.minGpa = p.minGpa;
        u.acceptedMajors = expandAcceptedMajors(p.eligibleMajors);

        for (int t = 0; t < p.availableTerms.size(); ++t)
        {
            const string &term = p.availableTerms[t];
            if (term == "Academic Year")
            {
                pushUnique(u.semesters, "Fall");
                pushUnique(u.semesters, "Spring");
            }
            else
            {
                pushUnique(u.semesters, term);
            }
        }
        out.push_back(u);
    }
    return out;
}

Vector<AdminSeatCount> AdminService::buildSeatCounts() const
{
    Vector<AdminSeatCount> out;
    for (int i = 0; i < partners.size(); ++i)
    {
        const AdminPartnerDetail &p = partners[i];
        if (seatOverrides.search(p.slug))
        {
            AdminSeatCount c;
            c.slug = p.slug;
            c.seats = seatOverrides.at(p.slug);
            out.push_back(c);
        }
        else if (p.seatQuotaSeeded >= 0)
        {
            AdminSeatCount c;
            c.slug = p.slug;
            c.seats = p.seatQuotaSeeded;
            out.push_back(c);
        }
    }
    return out;
}

Vector<AdminStudent> AdminService::buildAdminStudents(const Vector<AdminApplicationRow> &rows,
                                                        const string &term,
                                                        Vector<int> &sourceRowIndicesOut) const
{
    Vector<AdminStudent> out;
    sourceRowIndicesOut = Vector<int>();
    for (int i = 0; i < rows.size(); ++i)
    {
        const AdminApplicationRow &r = rows[i];
        if (r.status == "Draft")
            continue; // Unsubmitted drafts do not enter seat matching
        AdminStudent s;
        s.id = out.size(); // Exact index into returned AdminStudent vector & sourceRowIndicesOut
        s.name = r.fullName;
        s.gpa = r.gpa;
        s.majorCode = extractMajorCode(r.major);
        s.semester = term;
        for (int p = 0; p < 3; ++p)
            s.prefs[p] = r.prefSlugs[p];
        s.hasStatementRating = r.hasStatementRating;
        s.statementRating = r.statementRating;
        s.studiedAbroadBefore = r.prior;
        // Reflect full student eligibility intake flag or audit Ineligible into status check
        s.status = (!r.eligibilityReason.empty() && r.eligibilitySaved) ? "Ineligible" : r.status;
        out.push_back(s);
        sourceRowIndicesOut.push_back(i);
    }
    return out;
}

AdminOverviewStats AdminService::getOverviewStats(const string &cycle) const
{
    Vector<AdminApplicationRow> rows = buildAllApplicationRows();
    AdminOverviewStats stats;
    stats.totalApplicants = rows.size();
    stats.totalPartners = partners.size();
    stats.reviewQueueWaiting = 0;

    for (int i = 0; i < rows.size(); ++i)
    {
        const AdminApplicationRow &r = rows[i];
        if (r.status == "Submitted")
            stats.reviewQueueWaiting++;
        if (r.isDemo)
            stats.demoApplicantCount++;
        if (!r.prior)
            stats.firstTimeAbroadCount++;
        if (r.gpa < 2.8)
            stats.lowGpaFlaggedCount++;
        if (r.flagged)
            stats.flaggedCount++;
        if (r.status != "Draft" && r.status != "Ineligible" && r.gpa >= 2.8 &&
            (r.eligibilityReason.empty() || !r.eligibilitySaved))
            stats.eligibleCount++;
    }

    for (int i = 0; i < partners.size(); ++i)
    {
        const AdminPartnerDetail &p = partners[i];
        if (seatOverrides.search(p.slug))
        {
            stats.totalConfiguredSeats += seatOverrides.at(p.slug);
            stats.customQuotaCount++;
        }
        else if (p.seatQuotaSeeded >= 0)
        {
            stats.totalConfiguredSeats += p.seatQuotaSeeded;
            stats.customQuotaCount++;
        }
        else
        {
            stats.totalConfiguredSeats += DEFAULT_SEATS_PER_UNIVERSITY;
        }
    }

    // Fast Hopcroft-Karp upper bound check on active cohort
    Vector<int> sourceIdx;
    Vector<AdminStudent> adminStudents = buildAdminStudents(rows, extractTermFromCycle(cycle), sourceIdx);
    if (adminStudents.size() > 0 && partners.size() > 0)
    {
        Vector<AdminUniversity> adminUnis = buildAdminUniversities();
        Vector<AdminSeatCount> seatCounts = buildSeatCounts();
        ScoringWeights defaultW;
        defaultW.preference = 0.65;
        defaultW.gpa = 0.45;
        defaultW.statement = 0.35;
        defaultW.fairness = 0.20;
        AdminAllocator alloc(adminStudents, adminUnis, seatCounts, true, defaultW, DEFAULT_SEATS_PER_UNIVERSITY);
        AdminMatchResult hk = alloc.runMostPlaced();
        if (hk.valid)
        {
            int placed = 0;
            for (int u = 0; u < hk.byUniversity.size(); ++u)
                placed += hk.byUniversity[u].placed.size();
            stats.hkMaxFeasible = placed;
        }
    }

    return stats;
}

Vector<AdminPartnerDetail> AdminService::listPartners(const AdminPartnerFilter &filter,
                                                      AdminPartnerStats &statsOut) const
{
    Vector<AdminApplicationRow> rows = buildAllApplicationRows();
    HashMap<string, int> firstChoice;
    HashMap<string, int> totalDemand;
    computeDemandMaps(rows, firstChoice, totalDemand);

    statsOut = AdminPartnerStats();
    statsOut.totalPartners = partners.size();

    Vector<AdminPartnerDetail> filtered;
    for (int i = 0; i < partners.size(); ++i)
    {
        AdminPartnerDetail p = partners[i];
        if (seatOverrides.search(p.slug))
        {
            p.seats = seatOverrides.at(p.slug);
            p.seatsAssumed = false;
        }
        else if (p.seatQuotaSeeded >= 0)
        {
            p.seats = p.seatQuotaSeeded;
            p.seatsAssumed = false;
        }
        else
        {
            p.seats = DEFAULT_SEATS_PER_UNIVERSITY;
            p.seatsAssumed = true;
        }

        p.firstChoiceDemand = firstChoice.search(p.slug) ? firstChoice.at(p.slug) : 0;
        p.totalDemand = totalDemand.search(p.slug) ? totalDemand.at(p.slug) : 0;

        if (p.agreementStatus == "active")
            statsOut.activePartners++;
        else if (p.agreementStatus == "check_slots")
            statsOut.checkSlotsPartners++;
        else if (p.agreementStatus == "letter_of_intent")
            statsOut.loiPartners++;

        statsOut.totalConfiguredSeats += p.seats;
        if (!p.seatsAssumed)
            statsOut.customQuotaCount++;
        if (p.seats > 0 && p.totalDemand >= p.seats * 2)
            statsOut.highDemandCount++;
        if (p.factSheetStatus == "matched")
            statsOut.factSheetMatchedCount++;
        if (p.minGpaKnown)
            statsOut.knownMinGpaCount++;

        // Apply filters in C++
        if (!filter.search.empty())
        {
            bool hit = containsIgnoreCase(p.officialName, filter.search) ||
                       containsIgnoreCase(p.shortName, filter.search) ||
                       containsIgnoreCase(p.city, filter.search) ||
                       containsIgnoreCase(p.country, filter.search) ||
                       containsIgnoreCase(p.slug, filter.search);
            if (!hit)
                continue;
        }
        if (filter.region != "all" && !filter.region.empty() && p.region != filter.region)
            continue;
        if (filter.agreementStatus != "all" && !filter.agreementStatus.empty() &&
            p.agreementStatus != filter.agreementStatus)
            continue;
        if (filter.level != "all" && !filter.level.empty() && !vecContains(p.studentLevels, filter.level))
            continue;
        if (filter.term != "all" && !filter.term.empty())
        {
            bool offersTerm = vecContains(p.availableTerms, filter.term);
            if (!offersTerm && (filter.term == "Fall" || filter.term == "Spring"))
                offersTerm = vecContains(p.availableTerms, "Academic Year");
            if (!offersTerm)
                continue;
        }
        if (filter.quotaState == "custom" && p.seatsAssumed)
            continue;
        if (filter.quotaState == "default" && !p.seatsAssumed)
            continue;
        if (filter.quotaState == "demanded" && p.totalDemand == 0)
            continue;

        filtered.push_back(p);
    }

    return filtered;
}

bool AdminService::getPartnerDetail(const string &slug, AdminPartnerDetail &out) const
{
    const int *idx = partnerIndexBySlug.get(slug);
    if (idx == nullptr)
        return false;

    Vector<AdminApplicationRow> rows = buildAllApplicationRows();
    HashMap<string, int> firstChoice;
    HashMap<string, int> totalDemand;
    computeDemandMaps(rows, firstChoice, totalDemand);

    AdminPartnerDetail p = partners[*idx];
    if (seatOverrides.search(p.slug))
    {
        p.seats = seatOverrides.at(p.slug);
        p.seatsAssumed = false;
    }
    else if (p.seatQuotaSeeded >= 0)
    {
        p.seats = p.seatQuotaSeeded;
        p.seatsAssumed = false;
    }
    else
    {
        p.seats = DEFAULT_SEATS_PER_UNIVERSITY;
        p.seatsAssumed = true;
    }
    p.firstChoiceDemand = firstChoice.search(p.slug) ? firstChoice.at(p.slug) : 0;
    p.totalDemand = totalDemand.search(p.slug) ? totalDemand.at(p.slug) : 0;
    out = p;
    return true;
}

bool AdminService::setPartnerSeats(const string &slug,
                                   int seats,
                                   const string &now,
                                   AdminPartnerDetail &out,
                                   string &error)
{
    if (!partnerIndexBySlug.search(slug))
    {
        error = "Partner university not found.";
        return false;
    }
    if (seats < 0 || seats > 100)
    {
        error = "Seat quota must be between 0 and 100.";
        return false;
    }
    if (!adminRepo.saveSeatOverride(slug, seats, now))
    {
        error = "Could not persist seat quota override.";
        return false;
    }
    seatOverrides.insert(slug, seats);
    return getPartnerDetail(slug, out);
}

Vector<AdminApplicationRow> AdminService::listApplications(const AdminApplicationFilter &filter,
                                                           int &totalCountOut) const
{
    Vector<AdminApplicationRow> all = buildAllApplicationRows();
    totalCountOut = all.size();

    Vector<AdminApplicationRow> filtered;
    for (int i = 0; i < all.size(); ++i)
    {
        const AdminApplicationRow &s = all[i];
        if (filter.cohort == "eligible" && (s.status == "Ineligible" || s.status == "Draft" || s.gpa < 2.8))
            continue;
        if (filter.cohort == "flagged" && !s.flagged)
            continue;
        if (filter.standing != "all" && !filter.standing.empty() && s.standing != filter.standing)
            continue;
        if (filter.level != "all" && !filter.level.empty() && s.level != filter.level)
            continue;
        if (s.gpa < filter.gpaMin || s.gpa > filter.gpaMax)
            continue;
        if (filter.uniSlug != "all" && !filter.uniSlug.empty())
        {
            if (s.prefSlugs[0] != filter.uniSlug && s.prefSlugs[1] != filter.uniSlug &&
                s.prefSlugs[2] != filter.uniSlug)
                continue;
        }
        if (filter.prior == "first" && s.prior)
            continue;
        if (filter.prior == "prior" && !s.prior)
            continue;
        if (!filter.major.empty() && !containsIgnoreCase(s.major, filter.major))
            continue;
        if (filter.status != "all" && !filter.status.empty() && s.status != filter.status)
            continue;
        if (!filter.search.empty())
        {
            bool hit = containsIgnoreCase(s.fullName, filter.search) ||
                       containsIgnoreCase(s.studentEmail, filter.search) ||
                       containsIgnoreCase(s.aucId, filter.search) ||
                       containsIgnoreCase(s.major, filter.search) ||
                       containsIgnoreCase(s.prefNames[0], filter.search) ||
                       containsIgnoreCase(s.prefNames[1], filter.search) ||
                       containsIgnoreCase(s.prefNames[2], filter.search);
            if (!hit)
                continue;
        }
        filtered.push_back(s);
    }
    return filtered;
}

bool AdminService::getApplicationDetail(const string &email, AdminApplicationRow &out) const
{
    Vector<AdminApplicationRow> all = buildAllApplicationRows();
    string target = toLower(email);
    for (int i = 0; i < all.size(); ++i)
    {
        if (toLower(all[i].studentEmail) == target)
        {
            out = all[i];
            return true;
        }
    }
    return false;
}

bool AdminService::updateApplicationReview(const string &email,
                                           const AdminReviewUpdate &update,
                                           const string &now,
                                           AdminApplicationRow &out,
                                           string &error)
{
    AdminApplicationRow current;
    if (!getApplicationDetail(email, current))
    {
        error = "Application not found.";
        return false;
    }

    string nextStatus = update.status.empty() ? current.status : update.status;
    if (!validAdminStatus(nextStatus))
    {
        error = "Invalid application status.";
        return false;
    }

    int nextStanding = update.hasGoodStanding ? update.goodStanding : current.goodStanding;

    AdminReviewRecord rev;
    if (const AdminReviewRecord *existingRev = reviews.get(current.studentEmail))
        rev = *existingRev;
    rev.studentEmail = current.studentEmail;
    if (update.updateStatementRating)
    {
        if (update.hasStatementRating && (update.statementRating < 0.0 || update.statementRating > 10.0))
        {
            error = "Statement rating must be between 0.0 and 10.0.";
            return false;
        }
        rev.hasStatementRating = update.hasStatementRating;
        rev.statementRating = update.hasStatementRating ? update.statementRating : 0.0;
    }
    if (update.updateNotes)
        rev.notes = update.notes;
    rev.updatedAt = now;

    if (!adminRepo.saveReview(rev))
    {
        error = "Could not save review annotations.";
        return false;
    }
    reviews.insert(rev.studentEmail, rev);

    if (nextStatus != current.status || nextStanding != current.goodStanding)
    {
        if (!adminRepo.updateApplicationReviewFields(current.studentEmail, nextStatus, nextStanding, now))
        {
            error = "Could not update application status.";
            return false;
        }
        // Sync live in-memory student ApplicationService entry without calling loadAll()
        if (Application *live = appService.find(current.studentEmail))
        {
            live->status = nextStatus;
            live->goodStanding = nextStanding;
            live->updatedAt = now;
        }
        if (nextStatus != "Submitted" && appService.nextForReview() == current.studentEmail)
            appService.finishReview();

        if (!rev.isDemo && nextStatus != current.status &&
            (nextStatus == "Verified" || nextStatus == "Ineligible"))
        {
            string ignored;
            string msg = (nextStatus == "Verified")
                             ? "IPSO verified your exchange application for matching."
                             : "IPSO flagged your application as ineligible. Check with studyabroad@aucegypt.edu.";
            discoveryService.notify(current.studentEmail, msg, now, ignored);
        }
    }

    return getApplicationDetail(current.studentEmail, out);
}

AdminRunDiff AdminService::computeDiff(const Vector<AdminPlacementRow> &currentPlacements,
                                       int currentFirstChoices,
                                       int baselineRunId) const
{
    AdminRunDiff diff;
    int targetId = baselineRunId;
    if (targetId <= 0 && savedRuns.size() > 0)
        targetId = savedRuns[0].id;
    if (targetId <= 0)
        return diff;

    AdminSavedRunDetail baseline;
    if (!adminRepo.loadSavedRunDetail(targetId, baseline))
        return diff;

    diff.hasBaseline = true;
    diff.baselineRunId = baseline.summary.id;
    diff.baselineRunCode = baseline.summary.runCode;
    diff.baselineTitle = baseline.summary.title;
    diff.placedDelta = currentPlacements.size() - baseline.summary.placedCount;
    diff.firstChoiceDelta = currentFirstChoices - baseline.summary.firstChoiceCount;

    HashMap<string, AdminPlacementRow> oldMap(baseline.placements.size() * 2 + 16);
    for (int i = 0; i < baseline.placements.size(); ++i)
        oldMap.insert(baseline.placements[i].studentEmail, baseline.placements[i]);

    HashMap<string, AdminPlacementRow> newMap(currentPlacements.size() * 2 + 16);
    for (int i = 0; i < currentPlacements.size(); ++i)
        newMap.insert(currentPlacements[i].studentEmail, currentPlacements[i]);

    for (int i = 0; i < currentPlacements.size(); ++i)
    {
        const AdminPlacementRow &cur = currentPlacements[i];
        const AdminPlacementRow *prev = oldMap.get(cur.studentEmail);
        if (prev == nullptr)
        {
            diff.gainedCount++;
            AdminRunDiffStudent c;
            c.studentEmail = cur.studentEmail;
            c.studentName = cur.studentName;
            c.changeType = "gained";
            c.toUniSlug = cur.universitySlug;
            c.toUniName = cur.universityName;
            c.scoreDelta = cur.score;
            diff.changes.push_back(c);
        }
        else if (prev->universitySlug != cur.universitySlug)
        {
            diff.shiftedCount++;
            AdminRunDiffStudent c;
            c.studentEmail = cur.studentEmail;
            c.studentName = cur.studentName;
            c.changeType = "shifted";
            c.fromUniSlug = prev->universitySlug;
            c.fromUniName = prev->universityName;
            c.toUniSlug = cur.universitySlug;
            c.toUniName = cur.universityName;
            c.scoreDelta = cur.score - prev->score;
            diff.changes.push_back(c);
        }
    }

    for (int i = 0; i < baseline.placements.size(); ++i)
    {
        const AdminPlacementRow &prev = baseline.placements[i];
        if (!newMap.search(prev.studentEmail))
        {
            diff.lostCount++;
            AdminRunDiffStudent c;
            c.studentEmail = prev.studentEmail;
            c.studentName = prev.studentName;
            c.changeType = "lost";
            c.fromUniSlug = prev.universitySlug;
            c.fromUniName = prev.universityName;
            c.scoreDelta = -prev.score;
            diff.changes.push_back(c);
        }
    }

    return diff;
}

AdminSimulationResult AdminService::simulate(const AdminSimulationRequest &request) const
{
    AdminSimulationResult res;
    res.mode = modeLabel(request.mode);
    res.cycle = request.cycle;
    res.allowUnranked = request.allowUnranked;
    res.weights = request.weights;

    // Option 1 (most_placed) ignores weights mathematically, so supply valid fallback weights if all zero.
    ScoringWeights effWeights = request.weights;
    if (!AdminFitScorer::weightsAreValid(effWeights))
    {
        if (res.mode == "most_placed")
        {
            effWeights.preference = 1.0;
            effWeights.gpa = 0.0;
            effWeights.statement = 0.0;
            effWeights.fairness = 0.0;
        }
        else
        {
            res.error = "Weight sliders must be non-negative and cannot all be zero.";
            return res;
        }
    }

    Vector<AdminApplicationRow> allRows = buildAllApplicationRows();
    HashMap<string, int> firstDemand;
    HashMap<string, int> totalDemand;
    computeDemandMaps(allRows, firstDemand, totalDemand);

    Vector<int> sourceIdx;
    string term = extractTermFromCycle(request.cycle);
    Vector<AdminStudent> adminStudents = buildAdminStudents(allRows, term, sourceIdx);
    if (adminStudents.size() == 0)
    {
        res.error = "No submitted or active applicants found for matching.";
        return res;
    }

    Vector<AdminUniversity> adminUnis = buildAdminUniversities();
    Vector<AdminSeatCount> seatCounts = buildSeatCounts();
    int defSeats = request.defaultSeats >= 0 ? request.defaultSeats : DEFAULT_SEATS_PER_UNIVERSITY;

    // Count eligibility cohort (not Ineligible and >= 2.8)
    for (int s = 0; s < adminStudents.size(); ++s)
    {
        const AdminApplicationRow &src = allRows[sourceIdx[s]];
        if (src.status != "Ineligible" && src.gpa >= 2.8 &&
            (src.eligibilityReason.empty() || !src.eligibilitySaved))
            res.eligibleCount++;
    }

    AdminAllocator allocator(adminStudents, adminUnis, seatCounts, request.allowUnranked, effWeights, defSeats);
    if (!allocator.valid())
    {
        res.error = "Allocator configuration is invalid (check seat counts and weights).";
        return res;
    }

    auto tStart = std::chrono::steady_clock::now();

    // Compute Hopcroft-Karp upper bound for comparison KPI
    AdminMatchResult hkRes = allocator.runMostPlaced();
    if (hkRes.valid)
    {
        int hkPlaced = 0;
        for (int u = 0; u < hkRes.byUniversity.size(); ++u)
            hkPlaced += hkRes.byUniversity[u].placed.size();
        res.hkMax = hkPlaced;
    }

    if (res.mode == "shortlists")
    {
        int limit = request.shortlistLimit > 0 ? request.shortlistLimit : 10;
        AdminShortlistResult sl = allocator.runTopCandidates(limit);
        auto tEnd = std::chrono::steady_clock::now();
        res.solveTimeMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        if (!sl.valid)
        {
            res.error = "Failed to generate candidate shortlists.";
            return res;
        }

        for (int u = 0; u < sl.byUniversity.size(); ++u)
        {
            const AdminUniversityShortlist &sec = sl.byUniversity[u];
            if (sec.eligibleCount == 0 && !totalDemand.search(sec.slug))
                continue; // Keep shortlist table scannable across partners with applicants/eligibility
            AdminShortlistRow outRow;
            outRow.slug = sec.slug;
            outRow.name = sec.name;
            outRow.country = partners[u].country;
            outRow.totalSeats = sec.totalSeats;
            outRow.seatsAssumed = sec.seatsAssumed;
            outRow.eligibleCount = sec.eligibleCount;

            for (int c = 0; c < sec.candidates.size(); ++c)
            {
                const AdminCandidate &cand = sec.candidates[c];
                const AdminApplicationRow &src = allRows[sourceIdx[cand.student.id]];
                AdminShortlistCandidateRow cr;
                cr.rank = cand.rank;
                cr.studentEmail = src.studentEmail;
                cr.studentName = src.fullName;
                cr.studentAucId = src.aucId;
                cr.studentMajor = src.major;
                cr.studentGpa = src.gpa;
                cr.score = cand.score;
                cr.minGpaUnknown = cand.minGpaUnknown;
                cr.statementMissing = cand.statementMissing;
                outRow.candidates.push_back(cr);
            }
            res.shortlists.push_back(outRow);
        }

        // Insertion sort shortlists by eligibleCount desc then totalSeats desc
        for (int i = 1; i < res.shortlists.size(); ++i)
        {
            AdminShortlistRow key = res.shortlists[i];
            int j = i - 1;
            while (j >= 0 && res.shortlists[j].eligibleCount < key.eligibleCount)
            {
                res.shortlists[j + 1] = res.shortlists[j];
                j--;
            }
            res.shortlists[j + 1] = key;
        }

        res.valid = true;
        return res;
    }

    // Allocation modes: best_fit | highest_score | most_placed
    AdminMatchResult match = (res.mode == "most_placed")      ? hkRes
                             : (res.mode == "highest_score") ? allocator.runHighestScore()
                                                             : allocator.runBestFit();
    auto tEnd = std::chrono::steady_clock::now();
    res.solveTimeMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    if (!match.valid)
    {
        res.error = "Matching engine returned an invalid allocation.";
        return res;
    }

    double sumScores = 0.0;
    for (int u = 0; u < match.byUniversity.size(); ++u)
    {
        const AdminUniversityResult &sec = match.byUniversity[u];
        int demand = totalDemand.search(sec.slug) ? totalDemand.at(sec.slug) : 0;

        AdminUniversityFillRow fillRow;
        fillRow.slug = sec.slug;
        fillRow.name = sec.name;
        fillRow.country = partners[u].country;
        fillRow.region = partners[u].region;
        fillRow.totalSeats = sec.totalSeats;
        fillRow.seatsAssumed = sec.seatsAssumed;
        fillRow.filledSeats = sec.placed.size();
        fillRow.leftSeats = sec.totalSeats - sec.placed.size();
        fillRow.demandCount = demand;
        fillRow.demandRatio = (sec.totalSeats > 0) ? (static_cast<double>(demand) / sec.totalSeats)
                                                 : static_cast<double>(demand);

        double minPlacedScore = 2.0;
        for (int i = 0; i < sec.placed.size(); ++i)
        {
            const AdminPlacedStudent &ps = sec.placed[i];
            const AdminApplicationRow &src = allRows[sourceIdx[ps.student.id]];

            AdminPlacementRow pr;
            pr.studentEmail = src.studentEmail;
            pr.studentName = src.fullName;
            pr.studentAucId = src.aucId;
            pr.studentMajor = src.major;
            pr.studentGpa = src.gpa;
            pr.universitySlug = sec.slug;
            pr.universityName = sec.name;
            pr.seatLabel = ps.seatLabel;
            pr.score = ps.score;

            int prefRank = 0;
            for (int rIdx = 0; rIdx < 3; ++rIdx)
            {
                if (src.prefSlugs[rIdx] == sec.slug)
                {
                    prefRank = rIdx + 1;
                    break;
                }
            }
            pr.prefRank = prefRank;
            pr.rankLabel = (prefRank == 1)   ? "#1 Choice"
                           : (prefRank == 2) ? "#2 Choice"
                           : (prefRank == 3) ? "#3 Choice"
                                             : "Unranked Eligible";
            if (prefRank == 1)
                res.firstChoiceCount++;
            else if (prefRank == 2)
                res.secondChoiceCount++;
            else if (prefRank == 3)
                res.thirdChoiceCount++;
            else
                res.unrankedCount++;

            sumScores += ps.score;
            if (ps.score < minPlacedScore)
                minPlacedScore = ps.score;

            res.placements.push_back(pr);
        }

        if (sec.placed.size() > 0)
        {
            fillRow.hasCutoff = true;
            fillRow.cutoffScore = minPlacedScore;
        }
        if (demand > 0 || sec.placed.size() > 0)
            res.universityFills.push_back(fillRow);
    }

    // Sort placements by score DESC
    for (int i = 1; i < res.placements.size(); ++i)
    {
        AdminPlacementRow key = res.placements[i];
        int j = i - 1;
        while (j >= 0 && res.placements[j].score < key.score)
        {
            res.placements[j + 1] = res.placements[j];
            j--;
        }
        res.placements[j + 1] = key;
    }

    // Sort university fill rows by demandRatio DESC then demandCount DESC
    for (int i = 1; i < res.universityFills.size(); ++i)
    {
        AdminUniversityFillRow key = res.universityFills[i];
        int j = i - 1;
        while (j >= 0 && (res.universityFills[j].demandCount < key.demandCount ||
                          (res.universityFills[j].demandCount == key.demandCount &&
                           res.universityFills[j].filledSeats < key.filledSeats)))
        {
            res.universityFills[j + 1] = res.universityFills[j];
            j--;
        }
        res.universityFills[j + 1] = key;
    }

    // Build exact per-student unplaced explanations directly from untouched AdminFitScorer
    if (match.unplaced.size() > 0)
    {
        AdminFitScorer diagScorer(adminStudents, adminUnis, request.allowUnranked, effWeights);
        const Vector<PairResult> &pairs = diagScorer.pairs();
        int uCount = adminUnis.size();

        for (int i = 0; i < match.unplaced.size(); ++i)
        {
            int sIdx = match.unplaced[i].id;
            const AdminApplicationRow &src = allRows[sourceIdx[sIdx]];

            AdminUnplacedRow ur;
            ur.studentEmail = src.studentEmail;
            ur.studentName = src.fullName;
            ur.studentAucId = src.aucId;
            ur.studentMajor = src.major;
            ur.studentGpa = src.gpa;

            string prefsJoined;
            for (int p = 0; p < 3; ++p)
            {
                if (!src.prefNames[p].empty())
                {
                    if (!prefsJoined.empty())
                        prefsJoined += " · ";
                    prefsJoined += src.prefNames[p];
                }
            }
            ur.prefsSummary = prefsJoined.empty() ? "No preferences selected" : prefsJoined;

            // Check eligible options vs failure reasons on their ranked choices
            int eligibleUnis = 0;
            PairReason primaryFailure = PairReason::NotRanked;
            for (int p = 0; p < 3; ++p)
            {
                if (src.prefSlugs[p].empty())
                    continue;
                const int *uPtr = partnerIndexBySlug.get(src.prefSlugs[p]);
                if (uPtr != nullptr)
                {
                    const PairResult &pr = pairs[sIdx * uCount + *uPtr];
                    if (pr.eligible())
                        eligibleUnis++;
                    else if (primaryFailure == PairReason::NotRanked)
                        primaryFailure = pr.reason;
                }
            }
            if (request.allowUnranked && eligibleUnis == 0)
            {
                for (int u = 0; u < uCount; ++u)
                {
                    if (pairs[sIdx * uCount + u].eligible())
                    {
                        eligibleUnis++;
                        break;
                    }
                    else if (primaryFailure == PairReason::NotRanked)
                    {
                        primaryFailure = pairs[sIdx * uCount + u].reason;
                    }
                }
            }

            if (eligibleUnis > 0)
            {
                ur.bottleneck = "Capacity Contention";
                ur.reason = "All eligible seat slots filled by higher-scoring applicants.";
            }
            else if (src.prefSlugs[0].empty() && src.prefSlugs[1].empty() && src.prefSlugs[2].empty() && !request.allowUnranked)
            {
                ur.bottleneck = "No Preferences / Seats";
                ur.reason = "No ranked university choices submitted and unranked fallback is off.";
            }
            else
            {
                ur.bottleneck = "Eligibility / Policy";
                ur.reason = reasonText(primaryFailure);
            }
            res.unplaced.push_back(ur);
        }
    }

    res.placedCount = res.placements.size();
    res.unplacedCount = res.unplaced.size();
    res.firstChoicePct = (res.placedCount > 0)
                             ? (100.0 * static_cast<double>(res.firstChoiceCount) / res.placedCount)
                             : 0.0;
    res.avgScore = (res.placedCount > 0) ? (sumScores / res.placedCount) : 0.0;
    res.diff = computeDiff(res.placements, res.firstChoiceCount, request.compareRunId);
    res.valid = true;
    return res;
}

Vector<AdminSavedRunSummary> AdminService::listSavedRuns() const
{
    return savedRuns;
}

Vector<AdminUniversityFillRow> AdminService::buildUniversityFillsFromPlacements(
    const Vector<AdminPlacementRow> &placements,
    int defaultSeats) const
{
    Vector<AdminUniversityFillRow> fills;
    Vector<AdminApplicationRow> allRows = buildAllApplicationRows();
    HashMap<string, int> firstChoiceDemand;
    HashMap<string, int> totalDemand;
    computeDemandMaps(allRows, firstChoiceDemand, totalDemand);

    int cap = partners.size() * 2 + 16;
    HashMap<string, int> filledBySlug(cap);
    HashMap<string, double> minScoreBySlug(cap);

    for (int i = 0; i < placements.size(); ++i)
    {
        const AdminPlacementRow &p = placements[i];
        int prevCount = filledBySlug.search(p.universitySlug) ? filledBySlug.at(p.universitySlug) : 0;
        filledBySlug.insert(p.universitySlug, prevCount + 1);

        const double *prevMin = minScoreBySlug.get(p.universitySlug);
        if (prevMin == nullptr || p.score < *prevMin)
            minScoreBySlug.insert(p.universitySlug, p.score);
    }

    int fallbackSeats = defaultSeats >= 0 ? defaultSeats : DEFAULT_SEATS_PER_UNIVERSITY;
    for (int u = 0; u < partners.size(); ++u)
    {
        const AdminPartnerDetail &p = partners[u];
        int seats = fallbackSeats;
        bool assumed = true;
        if (seatOverrides.search(p.slug))
        {
            seats = seatOverrides.at(p.slug);
            assumed = false;
        }
        else if (p.seatQuotaSeeded >= 0)
        {
            seats = p.seatQuotaSeeded;
            assumed = false;
        }

        int filledSeats = filledBySlug.search(p.slug) ? filledBySlug.at(p.slug) : 0;
        int demand = totalDemand.search(p.slug) ? totalDemand.at(p.slug) : 0;
        if (demand == 0 && filledSeats == 0)
            continue;

        AdminUniversityFillRow fillRow;
        fillRow.slug = p.slug;
        fillRow.name = p.officialName;
        fillRow.country = p.country;
        fillRow.region = p.region;
        fillRow.totalSeats = seats;
        fillRow.seatsAssumed = assumed;
        fillRow.filledSeats = filledSeats;
        fillRow.leftSeats = (seats > filledSeats) ? (seats - filledSeats) : 0;
        fillRow.demandCount = demand;
        fillRow.demandRatio = (seats > 0) ? (static_cast<double>(demand) / seats)
                                          : static_cast<double>(demand);

        const double *minScore = minScoreBySlug.get(p.slug);
        if (filledSeats > 0 && minScore != nullptr)
        {
            fillRow.hasCutoff = true;
            fillRow.cutoffScore = *minScore;
        }
        fills.push_back(fillRow);
    }

    // Sort university fill rows identically to simulate(): demandCount DESC then filledSeats DESC
    for (int i = 1; i < fills.size(); ++i)
    {
        AdminUniversityFillRow key = fills[i];
        int j = i - 1;
        while (j >= 0 && (fills[j].demandCount < key.demandCount ||
                          (fills[j].demandCount == key.demandCount &&
                           fills[j].filledSeats < key.filledSeats)))
        {
            fills[j + 1] = fills[j];
            j--;
        }
        fills[j + 1] = key;
    }

    return fills;
}

bool AdminService::getSavedRunDetail(int runId, AdminSavedRunDetail &out) const
{
    if (!adminRepo.loadSavedRunDetail(runId, out))
        return false;
    out.universityFills = buildUniversityFillsFromPlacements(out.placements, out.summary.defaultSeats);
    return true;
}

string AdminService::nextRunCode() const
{
    int maxNum = 0;
    for (int i = 0; i < savedRuns.size(); ++i)
    {
        if (savedRuns[i].id > maxNum)
            maxNum = savedRuns[i].id;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "RUN-%03d", maxNum + 1);
    return buf;
}

bool AdminService::saveRun(const string &title,
                           const AdminSimulationRequest &request,
                           const string &createdBy,
                           const string &now,
                           AdminSavedRunDetail &out,
                           string &error)
{
    AdminSimulationRequest simReq = request;
    if (simReq.mode == "shortlists")
    {
        error = "Shortlists is an inspection mode and cannot be saved as a placement run.";
        return false;
    }

    AdminSimulationResult sim = simulate(simReq);
    if (!sim.valid)
    {
        error = sim.error;
        return false;
    }

    AdminSavedRunDetail detail;
    detail.summary.runCode = nextRunCode();
    detail.summary.cycle = sim.cycle;
    detail.summary.title = title.empty() ? ("Matching Simulation · " + sim.mode) : title;
    detail.summary.mode = sim.mode;
    detail.summary.allowUnranked = sim.allowUnranked;
    detail.summary.weights = sim.weights;
    detail.summary.defaultSeats = request.defaultSeats >= 0 ? request.defaultSeats : DEFAULT_SEATS_PER_UNIVERSITY;
    detail.summary.status = "Draft";
    detail.summary.placedCount = sim.placedCount;
    detail.summary.eligibleCount = sim.eligibleCount;
    detail.summary.hkMax = sim.hkMax;
    detail.summary.firstChoiceCount = sim.firstChoiceCount;
    detail.summary.secondChoiceCount = sim.secondChoiceCount;
    detail.summary.thirdChoiceCount = sim.thirdChoiceCount;
    detail.summary.unrankedCount = sim.unrankedCount;
    detail.summary.firstChoicePct = sim.firstChoicePct;
    detail.summary.avgScore = sim.avgScore;
    detail.summary.createdBy = createdBy.empty() ? "IPSO Coordinator" : createdBy;
    detail.summary.createdAt = now;
    detail.placements = sim.placements;
    detail.unplaced = sim.unplaced;
    detail.universityFills = sim.universityFills;

    if (!adminRepo.insertSavedRun(detail))
    {
        error = "Could not persist matching run.";
        return false;
    }

    savedRuns = adminRepo.loadSavedRuns();
    out = detail;
    return true;
}

bool AdminService::publishRun(int runId,
                              const string &now,
                              AdminSavedRunSummary &out,
                              string &error)
{
    AdminSavedRunDetail detail;
    if (!adminRepo.loadSavedRunDetail(runId, detail))
    {
        error = "Run not found.";
        return false;
    }

    if (!adminRepo.archivePublishedRuns() || !adminRepo.updateRunStatus(runId, "Published"))
    {
        error = "Could not update run status to Published.";
        return false;
    }

    // Notify real (non-demo) placed students of their nomination
    for (int i = 0; i < detail.placements.size(); ++i)
    {
        const AdminPlacementRow &p = detail.placements[i];
        const AdminReviewRecord *rev = reviews.get(p.studentEmail);
        if (rev != nullptr && rev->isDemo)
            continue;
        string ignored;
        discoveryService.notify(
            p.studentEmail,
            "Exchange placement published (" + detail.summary.runCode + "): Nominated for " + p.universityName + ".",
            now, ignored);
    }

    savedRuns = adminRepo.loadSavedRuns();
    for (int i = 0; i < savedRuns.size(); ++i)
    {
        if (savedRuns[i].id == runId)
        {
            out = savedRuns[i];
            return true;
        }
    }
    return false;
}

bool AdminService::deleteRun(int runId, string &error)
{
    if (!adminRepo.deleteSavedRun(runId))
    {
        error = "Saved matching run not found.";
        return false;
    }
    savedRuns = adminRepo.loadSavedRuns();
    return true;
}

bool AdminService::diffRuns(int runIdA, int runIdB, AdminRunDiff &out, string &error) const
{
    AdminSavedRunDetail runA;
    if (!adminRepo.loadSavedRunDetail(runIdA, runA))
    {
        error = "Selected primary run not found.";
        return false;
    }
    AdminSavedRunDetail runB;
    if (!adminRepo.loadSavedRunDetail(runIdB, runB))
    {
        error = "Selected baseline run not found.";
        return false;
    }

    out = computeDiff(runA.placements, runA.summary.firstChoiceCount, runIdB);
    return out.hasBaseline;
}

bool AdminService::seedDemoCohort(const string &now, int &seededCountOut, string &error)
{
    seededCountOut = 0;
    if (partners.size() < 3)
    {
        error = "Need at least 3 partner universities in SQLite to generate a realistic demo cohort.";
        return false;
    }

    // Idempotent cleanup of any prior demo batch
    if (adminRepo.clearDemoBatch() < 0)
    {
        error = "Could not clear previous demo cohort.";
        return false;
    }

    // Separate active partners vs top-demand flagship slugs for realistic contention skew
    Vector<string> activeSlugs;
    for (int i = 0; i < partners.size(); ++i)
    {
        if (partners[i].agreementStatus == "active")
            activeSlugs.push_back(partners[i].slug);
    }
    if (activeSlugs.size() < 3)
    {
        for (int i = 0; i < partners.size(); ++i)
            activeSlugs.push_back(partners[i].slug);
    }

    const char *FLAGSHIP_SLUGS[] = {
        "bocconi-university",
        "aarhus-university",
        "technical-university-of-munich-tum",
        "sciences-po-institute-d-etudes-politiques-de-paris",
        "vrije-universiteit-amsterdam",
        "university-of-edinburgh",
        "king-s-college-london-kcl",
        "copenhagen-business-school",
        "lund-university",
        "akita-international-university",
        "the-university-of-texas-at-austin",
        "university-of-pennsylvania",
        "cornell-university",
        "trinity-college-dublin",
        "koc-university",
        "bogazici-university",
        "edhec-business-school",
        "university-of-copenhagen",
        "freie-universitat-berlin",
        "boston-college"
    };
    Vector<string> flagships;
    for (size_t i = 0; i < sizeof(FLAGSHIP_SLUGS) / sizeof(FLAGSHIP_SLUGS[0]); ++i)
    {
        if (partnerIndexBySlug.search(FLAGSHIP_SLUGS[i]))
            flagships.push_back(FLAGSHIP_SLUGS[i]);
    }
    if (flagships.size() == 0)
    {
        for (int i = 0; i < activeSlugs.size() && i < 20; ++i)
            flagships.push_back(activeSlugs[i]);
    }

    const char *FIRST_MALE[] = {
        "Youssef", "Omar", "Karim", "Tarek", "Ziad", "Aly", "Mostafa", "Seif",
        "Adam", "Khaled", "Hassan", "Rami", "Marwan", "Hossam", "Sherif"
    };
    const char *FIRST_FEMALE[] = {
        "Salma", "Mariam", "Laila", "Nourhan", "Nadine", "Farida", "Hana", "Jana",
        "Malak", "Yara", "Dina", "Rana", "Yasmin", "Maya", "Habiba"
    };
    const char *LAST_NAMES[] = {
        "El-Masry", "Abdelrahman", "Tarek", "Nabil", "El-Sayed", "Mansour", "Sherif", "Farouk",
        "Gamal", "Zaki", "Ibrahim", "Soliman", "Kamel", "Hegazy", "Shoukry",
        "El-Shenawy", "Badr", "El-Gendy", "Hassan", "Morsi", "Darwish", "Khalil", "Sadek", "Ashraf"
    };
    const char *MAJORS[] = {
        "Computer Science (CSCE)",
        "Economics (ECON)",
        "Business Administration (BUS)",
        "Finance (FINC)",
        "Computer Engineering (CENG)",
        "Mechanical Engineering (MENG)",
        "Political Science (POLS)",
        "Architecture (ARCH)",
        "Journalism & Mass Communication (JRMC)",
        "Psychology (PSYC)",
        "Data Science (DATA)",
        "Integrated Marketing Comm. (IMC)",
        "Biology (BIOL)",
        "Law & Legal Studies (LAW)"
    };
    const char *NATIONALITIES[] = {
        "Egyptian", "Egyptian", "Egyptian", "Egyptian", "Jordanian", "Lebanese", "Saudi", "American"
    };
    const char *SCHOLARSHIPS[] = {"None", "None", "None", "Tomorrow's Leaders", "Other"};
    const char *DOCTORS[][2] = {
        {"Dr. Amr Hassan", "amr.hassan@aucegypt.edu"},
        {"Dr. Laila El-Sadek", "laila.sadek@aucegypt.edu"},
        {"Dr. Tarek Ezzat", "tarek.ezzat@aucegypt.edu"},
        {"Dr. Mona Badawi", "mona.badawi@aucegypt.edu"},
        {"Dr. Hisham Darwish", "hisham.darwish@aucegypt.edu"}
    };

    const int DEMO_COUNT = 500;
    DemoRng rng(20261122ULL);

    Vector<StudentRecord> students;
    Vector<Application> apps;
    Vector<ApplicationDocument> docs;
    Vector<LetterRequest> letters;
    Vector<AdminReviewRecord> revBatch;
    students.reserve(DEMO_COUNT);
    apps.reserve(DEMO_COUNT);
    revBatch.reserve(DEMO_COUNT);

    for (int i = 0; i < DEMO_COUNT; ++i)
    {
        bool female = (i % 2 == 1);
        const char *fName = female ? FIRST_FEMALE[rng.range(0, 14)] : FIRST_MALE[rng.range(0, 14)];
        const char *lName = LAST_NAMES[rng.range(0, 23)];
        string fullName = string(fName) + " " + lName;

        char emailBuf[64];
        std::snprintf(emailBuf, sizeof(emailBuf), "demo.student%03d@aucegypt.edu", i + 1);
        string email = emailBuf;

        char idBuf[16];
        std::snprintf(idBuf, sizeof(idBuf), "900210%03d", i + 1);
        string aucId = idBuf;

        bool isGr = (rng.range(1, 100) <= 14);
        string level = isGr ? "GR" : "UG";
        string major = MAJORS[rng.range(0, 13)];

        // Varied GPA distribution: ~25% honors (3.70-4.00), ~63% eligible (2.80-3.69), ~12% below threshold (2.25-2.79)
        int tier = rng.range(1, 100);
        double gpa = 3.25;
        if (tier <= 12)
            gpa = 2.25 + rng.unit() * 0.54;
        else if (tier <= 75)
            gpa = (isGr ? 3.20 : 2.82) + rng.unit() * 0.85;
        else
            gpa = 3.70 + rng.unit() * 0.30;
        // Round to 2 decimal places
        gpa = static_cast<int>(gpa * 100.0 + 0.5) / 100.0;

        int credits = isGr ? rng.range(9, 33) : rng.range(30, 114);
        bool majorDeclared = isGr ? true : (rng.range(1, 100) > 3);
        bool finalSemester = (rng.range(1, 100) <= 4);
        bool acadProbation = (gpa < 2.55) || (rng.range(1, 100) <= 2);
        bool discProbation = (rng.range(1, 100) <= 1);
        bool priorAbroad = (rng.range(1, 100) <= 22);
        int budget = rng.range(6, 24) * 500; // $3,000 .. $12,000

        // Derive realistic audit status aligned with IPSO rules
        bool policyEligible = !acadProbation && !discProbation && !finalSemester &&
                              majorDeclared && (isGr ? (gpa >= 3.2) : (gpa >= 2.8 && credits >= 24));
        int sRoll = rng.range(1, 100);
        string status;
        int goodStanding = 1;
        if (!policyEligible)
        {
            status = (sRoll <= 75) ? "Ineligible" : "In Review";
            goodStanding = acadProbation || discProbation ? 0 : -1;
        }
        else if (sRoll <= 62)
        {
            status = "Verified";
            goodStanding = 1;
        }
        else if (sRoll <= 79)
        {
            status = "Submitted";
            goodStanding = -1;
        }
        else if (sRoll <= 95)
        {
            status = "In Review";
            goodStanding = -1;
        }
        else
        {
            status = "Draft";
            goodStanding = -1;
        }

        // Pick 3 distinct real partner university slugs (45% flagship contention, 55% broad catalog)
        string picks[3];
        string pickNames[3];
        for (int p = 0; p < 3; ++p)
        {
            string chosen;
            for (int tries = 0; tries < 20; ++tries)
            {
                bool useFlagship = (rng.range(1, 100) <= (p == 0 ? 52 : 35));
                chosen = useFlagship
                             ? flagships[rng.range(0, flagships.size() - 1)]
                             : activeSlugs[rng.range(0, activeSlugs.size() - 1)];
                bool dup = false;
                for (int k = 0; k < p; ++k)
                    if (picks[k] == chosen)
                        dup = true;
                if (!dup)
                    break;
            }
            picks[p] = chosen;
            const int *pIdx = partnerIndexBySlug.get(chosen);
            pickNames[p] = (pIdx != nullptr) ? partners[*pIdx].officialName : chosen;
        }

        char phoneBuf[32];
        std::snprintf(phoneBuf, sizeof(phoneBuf), "+20 100 %03d %04d", 100 + (i % 899), 1000 + (i * 7 % 8999));

        StudentRecord sr;
        sr.email = email;
        sr.name = fullName;
        sr.aucId = aucId;
        sr.phone = phoneBuf;
        sr.level = level;
        sr.major = major;
        sr.gpa = gpa;
        sr.hasGpa = true;
        sr.credits = credits;
        sr.hasCredits = true;
        sr.majorDeclared = majorDeclared;
        sr.rhet110 = true;
        sr.rhet120 = (credits >= 24);
        sr.finalSemester = finalSemester;
        sr.academicProbation = acadProbation;
        sr.disciplinaryProbation = discProbation;
        sr.englishComplete = true;
        sr.firstSemesterNoGpa = false;
        sr.eligibilitySaved = true;
        sr.budget = budget;
        sr.hasBudget = true;
        sr.updatedAt = now;
        students.push_back(sr);

        Application app;
        app.studentEmail = email;
        app.firstName = fName;
        app.surname = lName;
        char dobBuf[16];
        std::snprintf(dobBuf, sizeof(dobBuf), "%02d/%02d/%04d", rng.range(1, 12), rng.range(1, 28),
                      isGr ? rng.range(1999, 2002) : rng.range(2004, 2006));
        app.dateOfBirth = dobBuf;
        app.nationality = NATIONALITIES[rng.range(0, 7)];
        app.secondNationality = (rng.range(1, 100) <= 12) ? "British" : "";
        app.gender = female ? "Female" : "Male";
        app.phone = phoneBuf;
        app.aucId = aucId;
        app.gpa = gpa;
        app.level = level;
        app.major = major;
        app.rhet1020 = isGr ? "Not Applicable" : "Yes";
        app.expectedGraduation = (credits >= 90) ? "06/2027" : ((credits >= 60) ? "06/2028" : "06/2029");
        app.accommodation = (rng.range(1, 100) <= 6);
        app.scholarship = SCHOLARSHIPS[rng.range(0, 4)];
        app.programType = (rng.range(1, 100) <= 92) ? "Exchange" : "Direct Enrollment";
        app.pref[0] = picks[0];
        app.pref[1] = picks[1];
        app.pref[2] = picks[2];

        app.guardianName = string("Hany ") + lName;
        app.guardianRelation = (i % 2 == 0) ? "Father" : "Mother";
        app.guardianPhone = "+20 122 410 7733";
        app.guardianEmail = string("guardian.") + aucId + "@gmail.com";
        app.guardianLanguage = "Arabic";

        app.qPreparedness = "Completed core coursework in " + major +
                            " and mapped degree-equivalent electives with my AUC academic advisor for Fall 2027.";
        app.qGoals =
            "My academic focus within " + major +
            " centers on bridging theoretical coursework at AUC with specialized seminars abroad. "
            "Applying to " + pickNames[0] + " as my first choice gives me direct access to upper-division "
            "research electives, laboratory methods, and faculty mentorship that complement my remaining degree requirements. "
            "My second preference, " + pickNames[1] + ", similarly aligns with my planned concentration and credit-transfer "
            "roadmap approved in advance with my department advisor, while " + pickNames[2] + " expands my comparative perspective "
            "across international curricula.\n\n"
            "Over the past semesters at AUC, maintaining a cumulative GPA of " +
            to_string(gpa).substr(0, 4) +
            " while collaborating on group capstones and student initiatives has prepared me for independent study in a new "
            "academic system. I aim to translate insights from courses taken abroad directly into my senior thesis and graduate "
            "or industry plans, bringing back structured comparative research methodologies to the AUC campus community.";
        app.travelExperience = priorAbroad ? "Yes, Study Abroad" : "Yes, Non-Study Abroad";
        app.qAdaptability =
            "Adapting to a new university environment begins with proactive preparation, cultural humility, and structured "
            "day-to-day engagement. Before departure I review host course registration rules, housing norms, and transit "
            "logistics so academic routines stabilize quickly during orientation week. Participating in diverse student clubs "
            "at AUC has taught me how to listen carefully across different communication styles, resolve scheduling friction "
            "calmly in multicultural project teams, and build supportive local networks while representing AUC responsibly abroad.";
        app.qTeamwork = "Led cross-functional lab groups and coordinated peer mentoring schedules cleanly under deadline pressure.";
        app.qReflection = "Broadening technical rigor, cross-cultural teamwork, and independent living maturity.";
        app.qFinance = "Supported by family savings and semester budget planning (~$" + to_string(budget) + ").";
        app.budget = budget;
        app.prior = priorAbroad;
        app.declaration = (status != "Draft");
        app.status = status;
        app.goodStanding = goodStanding;

        char tsBuf[32];
        std::snprintf(tsBuf, sizeof(tsBuf), "2026-11-%02dT%02d:%02d:00",
                      rng.range(5, 22), rng.range(9, 20), rng.range(0, 59));
        app.createdAt = tsBuf;
        app.updatedAt = tsBuf;
        app.submittedAt = (status == "Draft") ? "" : string(tsBuf);
        apps.push_back(app);

        // Documents (all 4 for non-drafts, partial for Draft)
        const char *docTypes[4] = {"courses", "transcript", "cv", "passport"};
        int docCount = (status == "Draft") ? rng.range(1, 3) : 4;
        for (int d = 0; d < docCount; ++d)
        {
            ApplicationDocument doc;
            doc.studentEmail = email;
            doc.docType = docTypes[d];
            doc.filePath = "uploads/demo/" + aucId + "/" + docTypes[d] + ".pdf";
            doc.uploadedAt = tsBuf;
            docs.push_back(doc);
        }

        // Recommendation letter
        int docIdx = rng.range(0, 4);
        LetterRequest lr;
        lr.studentEmail = email;
        lr.doctorName = DOCTORS[docIdx][0];
        lr.doctorEmail = DOCTORS[docIdx][1];
        lr.status = (status == "Verified" || status == "Submitted") ? "Received"
                    : (rng.range(1, 100) <= 60 ? "Received" : "Pending");
        lr.letterText = (lr.status == "Received") ? "Strong academic recommendation for outgoing exchange." : "";
        lr.requestedAt = "2026-11-04";
        lr.answeredAt = (lr.status == "Received") ? "2026-11-10" : "";
        letters.push_back(lr);

        // Admin statement rating (leave ~12% unrated so statementMissing edge case is exercised)
        AdminReviewRecord rev;
        rev.studentEmail = email;
        rev.hasStatementRating = (status != "Draft") && (rng.range(1, 100) <= 88);
        if (rev.hasStatementRating)
        {
            double rawR = 5.4 + rng.unit() * 4.4;
            rev.statementRating = static_cast<int>(rawR * 10.0 + 0.5) / 10.0;
        }
        rev.isDemo = true;
        rev.updatedAt = now;
        revBatch.push_back(rev);
    }

    if (!adminRepo.seedDemoBatch(students, apps, docs, letters, revBatch))
    {
        error = "Database transaction failed while seeding 500 demo applicants.";
        return false;
    }

    // Refresh reviews HashMap
    Vector<AdminReviewRecord> revList = adminRepo.loadReviews();
    reviews = HashMap<string, AdminReviewRecord>(revList.size() * 2 + 16);
    for (int i = 0; i < revList.size(); ++i)
        reviews.insert(revList[i].studentEmail, revList[i]);

    seededCountOut = DEMO_COUNT;
    return true;
}

bool AdminService::clearDemoCohort(int &removedCountOut, string &error)
{
    int removed = adminRepo.clearDemoBatch();
    if (removed < 0)
    {
        error = "Could not clear demo records.";
        return false;
    }
    Vector<AdminReviewRecord> revList = adminRepo.loadReviews();
    reviews = HashMap<string, AdminReviewRecord>(revList.size() * 2 + 16);
    for (int i = 0; i < revList.size(); ++i)
        reviews.insert(revList[i].studentEmail, revList[i]);

    removedCountOut = removed;
    return true;
}
