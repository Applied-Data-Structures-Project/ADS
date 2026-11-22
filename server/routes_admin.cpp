// routes_admin.cpp
// Thin HTTP/JSON transport layer for the IPSO Admin Dashboard (/api/admin/*).
// Responsibilities ONLY:
//   1. verify caller header (`currentUser`)
//   2. parse query params / JSON body
//   3. acquire `ctx.lock`
//   4. delegate to `ctx.admin` (`AdminService`)
//   5. serialize C++ structs to `crow::json::wvalue`
// Zero domain logic, filtering, sorting, or scoring calculations live here.

#include "app_context.hpp"

#include <cctype>
#include <cstdlib>

using namespace std;

namespace
{

string urlDecode(const string &encoded)
{
    string out;
    out.reserve(encoded.size());
    for (size_t i = 0; i < encoded.size(); ++i)
    {
        if (encoded[i] == '%' && i + 2 < encoded.size() &&
            isxdigit(static_cast<unsigned char>(encoded[i + 1])) &&
            isxdigit(static_cast<unsigned char>(encoded[i + 2])))
        {
            char hex[3] = {encoded[i + 1], encoded[i + 2], '\0'};
            out += static_cast<char>(std::strtol(hex, nullptr, 16));
            i += 2;
        }
        else if (encoded[i] == '+')
        {
            out += ' ';
        }
        else
        {
            out += encoded[i];
        }
    }
    return out;
}

string queryStr(const crow::request &req, const char *key, const string &fallback = "")
{
    const char *v = req.url_params.get(key);
    return (v && *v) ? string(v) : fallback;
}

double queryDouble(const crow::request &req, const char *key, double fallback)
{
    const char *v = req.url_params.get(key);
    if (!v || !*v)
        return fallback;
    char *end = nullptr;
    double parsed = std::strtod(v, &end);
    return (end != v) ? parsed : fallback;
}

int queryInt(const crow::request &req, const char *key, int fallback)
{
    const char *v = req.url_params.get(key);
    if (!v || !*v)
        return fallback;
    char *end = nullptr;
    long parsed = std::strtol(v, &end, 10);
    return (end != v) ? static_cast<int>(parsed) : fallback;
}

crow::json::wvalue stringListJson(const Vector<string> &items)
{
    crow::json::wvalue list = crow::json::wvalue::list();
    for (int i = 0; i < items.size(); ++i)
        list[i] = items[i];
    return list;
}

crow::json::wvalue partnerJson(const AdminPartnerDetail &p, bool includeDossier)
{
    crow::json::wvalue j;
    j["slug"] = p.slug;
    j["officialName"] = p.officialName;
    j["shortName"] = p.shortName;
    j["country"] = p.country;
    j["stateOrProvince"] = p.stateOrProvince;
    j["city"] = p.city;
    j["region"] = p.region;
    j["agreementStatus"] = p.agreementStatus;
    j["agreementScope"] = p.agreementScope;
    j["studentLevels"] = stringListJson(p.studentLevels);
    j["eligibleAucSchools"] = stringListJson(p.eligibleAucSchools);
    j["eligibleMajors"] = stringListJson(p.eligibleMajors);
    j["excludedMajors"] = stringListJson(p.excludedMajors);
    j["availableTerms"] = stringListJson(p.availableTerms);
    if (p.minGpaKnown)
        j["minGpa"] = p.minGpa;
    else
        j["minGpa"] = nullptr;
    j["seats"] = p.seats;
    j["seatsAssumed"] = p.seatsAssumed;
    if (p.livingCostUsdKnown)
        j["livingCostUsd"] = p.livingCostUsd;
    else
        j["livingCostUsd"] = nullptr;
    j["estMonthlyLocal"] = p.estMonthlyLocal;
    j["tuitionModel"] = p.tuitionModel;
    j["housingAvailability"] = p.housingAvailability;
    j["factSheetStatus"] = p.factSheetStatus;
    j["factSheetFilesCount"] = p.factSheetFilesCount;
    j["firstChoiceDemand"] = p.firstChoiceDemand;
    j["totalDemand"] = p.totalDemand;

    if (includeDossier)
    {
        j["minStandingNote"] = p.minStandingNote;
        j["instructionLanguages"] = stringListJson(p.instructionLanguages);
        j["englishCourseAvailability"] = p.englishCourseAvailability;
        j["englishProficiency"] = p.englishProficiency;
        j["hostLanguageRequirement"] = p.hostLanguageRequirement;
        j["fallTermDates"] = p.fallTermDates;
        j["springTermDates"] = p.springTermDates;
        j["nominationDeadlines"] = p.nominationDeadlines;
        if (p.finalSemesterAllowed < 0)
            j["finalSemesterAllowed"] = nullptr;
        else
            j["finalSemesterAllowed"] = (p.finalSemesterAllowed == 1);
        j["creditSystemAndLoad"] = p.creditSystemAndLoad;
        j["courseRestrictionsNote"] = p.courseRestrictionsNote;
        j["requiredDocs"] = stringListJson(p.requiredDocs);
        j["mandatoryHostFees"] = p.mandatoryHostFees;
        j["housingNotes"] = p.housingNotes;
        j["hostGrants"] = p.hostGrants;
        j["citizenshipRestrictions"] = p.citizenshipRestrictions;
        j["visaNotes"] = p.visaNotes;
        j["academicStyle"] = p.academicStyle;
        j["internationalSupport"] = p.internationalSupport;
        j["officialWebsiteUrl"] = p.officialWebsiteUrl;
        j["exchangePageUrl"] = p.exchangePageUrl;
        j["courseCatalogUrl"] = p.courseCatalogUrl;
        j["englishCourseListUrl"] = p.englishCourseListUrl;
    }
    return j;
}

crow::json::wvalue applicationRowJson(const AdminApplicationRow &a, bool includeEssays)
{
    crow::json::wvalue j;
    j["email"] = a.studentEmail;
    j["firstName"] = a.firstName;
    j["surname"] = a.surname;
    j["name"] = a.fullName;
    j["aucId"] = a.aucId;
    j["level"] = a.level;
    j["standing"] = a.standing;
    j["major"] = a.major;
    j["gpa"] = a.gpa;
    j["credits"] = a.credits;
    j["hasCredits"] = a.hasCredits;
    j["majorDeclared"] = a.majorDeclared;
    j["rhet110"] = a.rhet110;
    j["rhet120"] = a.rhet120;
    j["rhet1020"] = a.rhet1020;
    j["expectedGraduation"] = a.expectedGraduation;
    j["finalSemester"] = a.finalSemester;
    j["academicProbation"] = a.academicProbation;
    j["disciplinaryProbation"] = a.disciplinaryProbation;
    j["englishComplete"] = a.englishComplete;
    j["firstSemesterNoGpa"] = a.firstSemesterNoGpa;
    j["eligibilitySaved"] = a.eligibilitySaved;
    j["nationality"] = a.nationality;
    j["secondNationality"] = a.secondNationality;
    j["gender"] = a.gender;
    j["phone"] = a.phone;
    j["dateOfBirth"] = a.dateOfBirth;
    j["programType"] = a.programType;
    j["scholarship"] = a.scholarship;
    j["accommodation"] = a.accommodation;
    j["budget"] = a.budget;
    j["prior"] = a.prior;
    j["travelExperience"] = a.travelExperience;
    j["status"] = a.status;
    if (a.goodStanding < 0)
        j["goodStanding"] = nullptr;
    else
        j["goodStanding"] = (a.goodStanding == 1);
    j["declaration"] = a.declaration;
    j["submittedAt"] = a.submittedAt;
    j["updatedAt"] = a.updatedAt;

    j["prefSlugs"] = crow::json::wvalue::list();
    j["prefNames"] = crow::json::wvalue::list();
    for (int i = 0; i < 3; ++i)
    {
        j["prefSlugs"][i] = a.prefSlugs[i];
        j["prefNames"][i] = a.prefNames[i];
    }

    j["uploadedDocs"] = stringListJson(a.uploadedDocs);
    j["letterReceivedCount"] = a.letterReceivedCount;
    j["letterPendingCount"] = a.letterPendingCount;
    j["letterDeclinedCount"] = a.letterDeclinedCount;

    j["hasStatementRating"] = a.hasStatementRating;
    if (a.hasStatementRating)
        j["statementRating"] = a.statementRating;
    else
        j["statementRating"] = nullptr;
    j["adminNotes"] = a.adminNotes;
    j["isDemo"] = a.isDemo;
    j["flagged"] = a.flagged;
    j["eligibilityReason"] = a.eligibilityReason;

    if (includeEssays)
    {
        j["guardianName"] = a.guardianName;
        j["guardianRelation"] = a.guardianRelation;
        j["guardianPhone"] = a.guardianPhone;
        j["guardianEmail"] = a.guardianEmail;
        j["guardianLanguage"] = a.guardianLanguage;
        j["qPreparedness"] = a.qPreparedness;
        j["qGoals"] = a.qGoals;
        j["qAdaptability"] = a.qAdaptability;
        j["qTeamwork"] = a.qTeamwork;
        j["qReflection"] = a.qReflection;
        j["qFinance"] = a.qFinance;
    }
    return j;
}

crow::json::wvalue weightsJson(const ScoringWeights &w, bool allowUnranked, const string &mode)
{
    crow::json::wvalue j;
    j["pref"] = w.preference;
    j["gpa"] = w.gpa;
    j["llm"] = w.statement;
    j["equity"] = w.fairness;
    j["allowUnranked"] = allowUnranked;
    j["mode"] = mode;
    return j;
}

crow::json::wvalue placementJson(const AdminPlacementRow &p)
{
    crow::json::wvalue j;
    j["studentEmail"] = p.studentEmail;
    j["studentName"] = p.studentName;
    j["studentAucId"] = p.studentAucId;
    j["studentMajor"] = p.studentMajor;
    j["studentGpa"] = p.studentGpa;
    j["universitySlug"] = p.universitySlug;
    j["universityName"] = p.universityName;
    j["seatLabel"] = p.seatLabel;
    j["prefRank"] = p.prefRank;
    j["rankLabel"] = p.rankLabel;
    j["weight"] = p.score;
    return j;
}

crow::json::wvalue unplacedJson(const AdminUnplacedRow &u)
{
    crow::json::wvalue j;
    j["studentEmail"] = u.studentEmail;
    j["studentName"] = u.studentName;
    j["studentAucId"] = u.studentAucId;
    j["studentMajor"] = u.studentMajor;
    j["gpa"] = u.studentGpa;
    j["prefsLabel"] = u.prefsSummary;
    j["bottleneck"] = u.bottleneck;
    j["reason"] = u.reason;
    return j;
}

crow::json::wvalue universityFillJson(const AdminUniversityFillRow &u)
{
    crow::json::wvalue item;
    item["slug"] = u.slug;
    item["name"] = u.name;
    item["country"] = u.country;
    item["region"] = u.region;
    item["totalSeats"] = u.totalSeats;
    item["seatsAssumed"] = u.seatsAssumed;
    item["filledSeats"] = u.filledSeats;
    item["leftSeats"] = u.leftSeats;
    item["demandCount"] = u.demandCount;
    item["demandRatio"] = u.demandRatio;
    if (u.hasCutoff)
        item["cutoffScore"] = u.cutoffScore;
    else
        item["cutoffScore"] = nullptr;
    return item;
}

crow::json::wvalue diffJson(const AdminRunDiff &d)
{
    crow::json::wvalue j;
    j["hasBaseline"] = d.hasBaseline;
    j["baselineRunId"] = d.baselineRunId;
    j["baselineRunCode"] = d.baselineRunCode;
    j["baselineTitle"] = d.baselineTitle;
    j["placedDelta"] = d.placedDelta;
    j["firstChoiceDelta"] = d.firstChoiceDelta;
    j["gainedCount"] = d.gainedCount;
    j["lostCount"] = d.lostCount;
    j["shiftedCount"] = d.shiftedCount;
    j["changes"] = crow::json::wvalue::list();
    for (int i = 0; i < d.changes.size(); ++i)
    {
        const AdminRunDiffStudent &c = d.changes[i];
        crow::json::wvalue item;
        item["studentEmail"] = c.studentEmail;
        item["studentName"] = c.studentName;
        item["changeType"] = c.changeType;
        item["fromUniSlug"] = c.fromUniSlug;
        item["fromUniName"] = c.fromUniName;
        item["toUniSlug"] = c.toUniSlug;
        item["toUniName"] = c.toUniName;
        item["scoreDelta"] = c.scoreDelta;
        j["changes"][i] = std::move(item);
    }
    return j;
}

crow::json::wvalue runSummaryJson(const AdminSavedRunSummary &r)
{
    crow::json::wvalue j;
    j["id"] = r.id;
    j["runCode"] = r.runCode;
    j["cycle"] = r.cycle;
    j["title"] = r.title;
    j["mode"] = r.mode;
    j["allowUnranked"] = r.allowUnranked;
    j["weights"] = weightsJson(r.weights, r.allowUnranked, r.mode);
    j["defaultSeats"] = r.defaultSeats;
    j["status"] = r.status;
    j["placed"] = r.placedCount;
    j["eligibleCount"] = r.eligibleCount;
    j["hkMax"] = r.hkMax;
    j["firstChoiceCount"] = r.firstChoiceCount;
    j["secondChoiceCount"] = r.secondChoiceCount;
    j["thirdChoiceCount"] = r.thirdChoiceCount;
    j["unrankedCount"] = r.unrankedCount;
    j["firstChoicePct"] = r.firstChoicePct;
    j["avgScore"] = r.avgScore;
    j["createdBy"] = r.createdBy;
    j["createdAt"] = r.createdAt;
    return j;
}

AdminSimulationRequest parseSimulationRequest(const crow::json::rvalue &body)
{
    AdminSimulationRequest req;
    string cycle = jsonText(body, "cycle");
    if (!cycle.empty())
        req.cycle = cycle;
    string mode = jsonText(body, "mode");
    if (!mode.empty())
        req.mode = mode;

    if (body.has("weights") && body["weights"].t() == crow::json::type::Object)
    {
        const auto &w = body["weights"];
        req.weights.preference = jsonNumber(w, "pref", 0.65);
        req.weights.gpa = jsonNumber(w, "gpa", 0.45);
        req.weights.statement = jsonNumber(w, "llm", 0.35);
        req.weights.fairness = jsonNumber(w, "equity", 0.20);
        req.allowUnranked = jsonBool(w, "allowUnranked", false);
        string innerMode = jsonText(w, "mode");
        if (!innerMode.empty() && mode.empty())
            req.mode = innerMode;
    }
    else
    {
        req.weights.preference = jsonNumber(body, "pref", 0.65);
        req.weights.gpa = jsonNumber(body, "gpa", 0.45);
        req.weights.statement = jsonNumber(body, "llm", 0.35);
        req.weights.fairness = jsonNumber(body, "equity", 0.20);
        req.allowUnranked = jsonBool(body, "allowUnranked", false);
    }

    req.defaultSeats = jsonInt(body, "defaultSeats", 2);
    req.shortlistLimit = jsonInt(body, "shortlistLimit", 10);
    req.compareRunId = jsonInt(body, "compareRunId", 0);
    return req;
}

} // namespace

void registerAdminRoutes(App &app, AppContext &ctx)
{
    // GET /api/admin/overview
    CROW_ROUTE(app, "/api/admin/overview").methods("GET"_method)
    ([&ctx](const crow::request &req) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");
        string cycle = queryStr(req, "cycle", "2026–27 · Fall Term");

        lock_guard<mutex> guard(ctx.lock);
        AdminOverviewStats s = ctx.admin.getOverviewStats(cycle);

        crow::json::wvalue j;
        j["totalApplicants"] = s.totalApplicants;
        j["eligibleCount"] = s.eligibleCount;
        j["firstTimeAbroadCount"] = s.firstTimeAbroadCount;
        j["flaggedCount"] = s.flaggedCount;
        j["lowGpaFlaggedCount"] = s.lowGpaFlaggedCount;
        j["reviewQueueWaiting"] = s.reviewQueueWaiting;
        j["demoApplicantCount"] = s.demoApplicantCount;
        j["totalPartners"] = s.totalPartners;
        j["totalConfiguredSeats"] = s.totalConfiguredSeats;
        j["customQuotaCount"] = s.customQuotaCount;
        j["hkMaxFeasible"] = s.hkMaxFeasible;
        return crow::response(200, j);
    });

    // GET /api/admin/applications
    CROW_ROUTE(app, "/api/admin/applications").methods("GET"_method)
    ([&ctx](const crow::request &req) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        AdminApplicationFilter f;
        f.cohort = queryStr(req, "cohort", "all");
        f.search = queryStr(req, "search", "");
        f.standing = queryStr(req, "standing", "all");
        f.level = queryStr(req, "level", "all");
        f.gpaMin = queryDouble(req, "gpaMin", 0.0);
        f.gpaMax = queryDouble(req, "gpaMax", 4.0);
        f.uniSlug = queryStr(req, "uni", "all");
        f.prior = queryStr(req, "prior", "all");
        f.major = queryStr(req, "major", "");
        f.status = queryStr(req, "status", "all");

        lock_guard<mutex> guard(ctx.lock);
        int totalAll = 0;
        Vector<AdminApplicationRow> rows = ctx.admin.listApplications(f, totalAll);

        crow::json::wvalue j;
        j["totalAll"] = totalAll;
        j["count"] = rows.size();
        j["applications"] = crow::json::wvalue::list();
        for (int i = 0; i < rows.size(); ++i)
            j["applications"][i] = applicationRowJson(rows[i], false);
        return crow::response(200, j);
    });

    // GET /api/admin/applications/<email>
    CROW_ROUTE(app, "/api/admin/applications/<string>").methods("GET"_method)
    ([&ctx](const crow::request &req, const string &rawEmail) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        AdminApplicationRow row;
        if (!ctx.admin.getApplicationDetail(urlDecode(rawEmail), row))
            return jsonError(404, "Application not found.");
        return crow::response(200, applicationRowJson(row, true));
    });

    // PUT /api/admin/applications/<email>/review
    CROW_ROUTE(app, "/api/admin/applications/<string>/review").methods("PUT"_method)
    ([&ctx](const crow::request &req, const string &rawEmail) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        auto body = crow::json::load(req.body);
        if (!body)
            return jsonError(400, "Invalid JSON request body.");

        AdminReviewUpdate upd;
        upd.status = jsonText(body, "status");
        if (body.has("goodStanding"))
        {
            upd.hasGoodStanding = true;
            if (body["goodStanding"].t() == crow::json::type::Null)
                upd.goodStanding = -1;
            else
                upd.goodStanding = jsonBool(body, "goodStanding", true) ? 1 : 0;
        }
        if (body.has("statementRating"))
        {
            upd.updateStatementRating = true;
            if (body["statementRating"].t() == crow::json::type::Null)
            {
                upd.hasStatementRating = false;
            }
            else if (body["statementRating"].t() == crow::json::type::Number)
            {
                upd.hasStatementRating = true;
                upd.statementRating = body["statementRating"].d();
            }
        }
        if (body.has("notes") && body["notes"].t() == crow::json::type::String)
        {
            upd.updateNotes = true;
            upd.notes = string(body["notes"].s());
        }

        lock_guard<mutex> guard(ctx.lock);
        AdminApplicationRow updated;
        string error;
        if (!ctx.admin.updateApplicationReview(urlDecode(rawEmail), upd, nowText(), updated, error))
            return jsonError(error == "Application not found." ? 404 : 400, error);
        return crow::response(200, applicationRowJson(updated, true));
    });

    // GET /api/admin/universities
    CROW_ROUTE(app, "/api/admin/universities").methods("GET"_method)
    ([&ctx](const crow::request &req) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        AdminPartnerFilter f;
        f.search = queryStr(req, "search", "");
        f.region = queryStr(req, "region", "all");
        f.agreementStatus = queryStr(req, "status", "all");
        f.term = queryStr(req, "term", "all");
        f.level = queryStr(req, "level", "all");
        f.quotaState = queryStr(req, "quota", "all");

        lock_guard<mutex> guard(ctx.lock);
        AdminPartnerStats stats;
        Vector<AdminPartnerDetail> partners = ctx.admin.listPartners(f, stats);

        crow::json::wvalue j;
        j["count"] = partners.size();
        j["stats"]["totalPartners"] = stats.totalPartners;
        j["stats"]["activePartners"] = stats.activePartners;
        j["stats"]["checkSlotsPartners"] = stats.checkSlotsPartners;
        j["stats"]["loiPartners"] = stats.loiPartners;
        j["stats"]["totalConfiguredSeats"] = stats.totalConfiguredSeats;
        j["stats"]["customQuotaCount"] = stats.customQuotaCount;
        j["stats"]["highDemandCount"] = stats.highDemandCount;
        j["stats"]["factSheetMatchedCount"] = stats.factSheetMatchedCount;
        j["stats"]["knownMinGpaCount"] = stats.knownMinGpaCount;
        j["universities"] = crow::json::wvalue::list();
        for (int i = 0; i < partners.size(); ++i)
            j["universities"][i] = partnerJson(partners[i], false);
        return crow::response(200, j);
    });

    // GET /api/admin/universities/<slug>
    CROW_ROUTE(app, "/api/admin/universities/<string>").methods("GET"_method)
    ([&ctx](const crow::request &req, const string &slug) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        AdminPartnerDetail partner;
        if (!ctx.admin.getPartnerDetail(urlDecode(slug), partner))
            return jsonError(404, "Partner university not found.");
        return crow::response(200, partnerJson(partner, true));
    });

    // PUT /api/admin/universities/<slug>/seats
    CROW_ROUTE(app, "/api/admin/universities/<string>/seats").methods("PUT"_method)
    ([&ctx](const crow::request &req, const string &slug) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");
        auto body = crow::json::load(req.body);
        if (!body || !body.has("seats") || body["seats"].t() != crow::json::type::Number)
            return jsonError(400, "Provide a valid numeric 'seats' count.");

        int seats = static_cast<int>(body["seats"].i());
        lock_guard<mutex> guard(ctx.lock);
        AdminPartnerDetail updated;
        string error;
        if (!ctx.admin.setPartnerSeats(urlDecode(slug), seats, nowText(), updated, error))
            return jsonError(error == "Partner university not found." ? 404 : 400, error);
        return crow::response(200, partnerJson(updated, false));
    });

    // POST /api/admin/matching/simulate
    CROW_ROUTE(app, "/api/admin/matching/simulate").methods("POST"_method)
    ([&ctx](const crow::request &req) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");
        auto body = crow::json::load(req.body);
        if (!body)
            return jsonError(400, "Invalid JSON request body.");

        AdminSimulationRequest simReq = parseSimulationRequest(body);
        lock_guard<mutex> guard(ctx.lock);
        AdminSimulationResult r = ctx.admin.simulate(simReq);
        if (!r.valid)
            return jsonError(400, r.error);

        crow::json::wvalue j;
        j["mode"] = r.mode;
        j["cycle"] = r.cycle;
        j["allowUnranked"] = r.allowUnranked;
        j["weights"] = weightsJson(r.weights, r.allowUnranked, r.mode);
        j["placed"] = r.placedCount;
        j["eligibleCount"] = r.eligibleCount;
        j["unplacedCount"] = r.unplacedCount;
        j["hkMax"] = r.hkMax;
        j["firstChoiceCount"] = r.firstChoiceCount;
        j["secondChoiceCount"] = r.secondChoiceCount;
        j["thirdChoiceCount"] = r.thirdChoiceCount;
        j["unrankedCount"] = r.unrankedCount;
        j["firstChoicePct"] = r.firstChoicePct;
        j["avgWeight"] = r.avgScore;
        j["solveTimeMs"] = r.solveTimeMs;

        j["placements"] = crow::json::wvalue::list();
        for (int i = 0; i < r.placements.size(); ++i)
            j["placements"][i] = placementJson(r.placements[i]);

        j["unplaced"] = crow::json::wvalue::list();
        for (int i = 0; i < r.unplaced.size(); ++i)
            j["unplaced"][i] = unplacedJson(r.unplaced[i]);

        j["universityFills"] = crow::json::wvalue::list();
        for (int i = 0; i < r.universityFills.size(); ++i)
            j["universityFills"][i] = universityFillJson(r.universityFills[i]);

        j["shortlists"] = crow::json::wvalue::list();
        for (int i = 0; i < r.shortlists.size(); ++i)
        {
            const AdminShortlistRow &sl = r.shortlists[i];
            crow::json::wvalue item;
            item["slug"] = sl.slug;
            item["name"] = sl.name;
            item["country"] = sl.country;
            item["totalSeats"] = sl.totalSeats;
            item["seatsAssumed"] = sl.seatsAssumed;
            item["eligibleCount"] = sl.eligibleCount;
            item["candidates"] = crow::json::wvalue::list();
            for (int c = 0; c < sl.candidates.size(); ++c)
            {
                const AdminShortlistCandidateRow &cand = sl.candidates[c];
                crow::json::wvalue cj;
                cj["rank"] = cand.rank;
                cj["studentEmail"] = cand.studentEmail;
                cj["studentName"] = cand.studentName;
                cj["studentAucId"] = cand.studentAucId;
                cj["studentMajor"] = cand.studentMajor;
                cj["studentGpa"] = cand.studentGpa;
                cj["score"] = cand.score;
                cj["minGpaUnknown"] = cand.minGpaUnknown;
                cj["statementMissing"] = cand.statementMissing;
                item["candidates"][c] = std::move(cj);
            }
            j["shortlists"][i] = std::move(item);
        }

        j["diff"] = diffJson(r.diff);
        return crow::response(200, j);
    });

    // GET /api/admin/runs
    CROW_ROUTE(app, "/api/admin/runs").methods("GET"_method)
    ([&ctx](const crow::request &req) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        Vector<AdminSavedRunSummary> runs = ctx.admin.listSavedRuns();

        crow::json::wvalue j;
        j["runs"] = crow::json::wvalue::list();
        for (int i = 0; i < runs.size(); ++i)
            j["runs"][i] = runSummaryJson(runs[i]);
        return crow::response(200, j);
    });

    // GET /api/admin/runs/diff?a=<id>&b=<id>
    CROW_ROUTE(app, "/api/admin/runs/diff").methods("GET"_method)
    ([&ctx](const crow::request &req) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        int a = queryInt(req, "a", 0);
        int b = queryInt(req, "b", 0);
        if (a <= 0 || b <= 0)
            return jsonError(400, "Query params 'a' and 'b' run IDs are required.");

        lock_guard<mutex> guard(ctx.lock);
        AdminRunDiff diff;
        string error;
        if (!ctx.admin.diffRuns(a, b, diff, error))
            return jsonError(404, error);
        return crow::response(200, diffJson(diff));
    });

    // GET /api/admin/runs/<int>
    CROW_ROUTE(app, "/api/admin/runs/<int>").methods("GET"_method)
    ([&ctx](const crow::request &req, int runId) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        AdminSavedRunDetail detail;
        if (!ctx.admin.getSavedRunDetail(runId, detail))
            return jsonError(404, "Saved matching run not found.");

        crow::json::wvalue j = runSummaryJson(detail.summary);
        j["placements"] = crow::json::wvalue::list();
        for (int i = 0; i < detail.placements.size(); ++i)
            j["placements"][i] = placementJson(detail.placements[i]);
        j["unplaced"] = crow::json::wvalue::list();
        for (int i = 0; i < detail.unplaced.size(); ++i)
            j["unplaced"][i] = unplacedJson(detail.unplaced[i]);
        j["universityFills"] = crow::json::wvalue::list();
        for (int i = 0; i < detail.universityFills.size(); ++i)
            j["universityFills"][i] = universityFillJson(detail.universityFills[i]);
        return crow::response(200, j);
    });

    // POST /api/admin/runs
    CROW_ROUTE(app, "/api/admin/runs").methods("POST"_method)
    ([&ctx](const crow::request &req) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");
        auto body = crow::json::load(req.body);
        if (!body)
            return jsonError(400, "Invalid JSON request body.");

        string title = jsonText(body, "title");
        string createdBy = jsonText(body, "createdBy");
        if (createdBy.empty())
            createdBy = caller;
        AdminSimulationRequest simReq = parseSimulationRequest(body);

        lock_guard<mutex> guard(ctx.lock);
        AdminSavedRunDetail created;
        string error;
        if (!ctx.admin.saveRun(title, simReq, createdBy, nowText(), created, error))
            return jsonError(400, error);

        crow::json::wvalue j = runSummaryJson(created.summary);
        j["placements"] = crow::json::wvalue::list();
        for (int i = 0; i < created.placements.size(); ++i)
            j["placements"][i] = placementJson(created.placements[i]);
        j["unplaced"] = crow::json::wvalue::list();
        for (int i = 0; i < created.unplaced.size(); ++i)
            j["unplaced"][i] = unplacedJson(created.unplaced[i]);
        j["universityFills"] = crow::json::wvalue::list();
        for (int i = 0; i < created.universityFills.size(); ++i)
            j["universityFills"][i] = universityFillJson(created.universityFills[i]);
        return crow::response(201, j);
    });

    // POST /api/admin/runs/<int>/publish
    CROW_ROUTE(app, "/api/admin/runs/<int>/publish").methods("POST"_method)
    ([&ctx](const crow::request &req, int runId) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        AdminSavedRunSummary published;
        string error;
        if (!ctx.admin.publishRun(runId, nowText(), published, error))
            return jsonError(error == "Run not found." ? 404 : 400, error);
        return crow::response(200, runSummaryJson(published));
    });

    // DELETE /api/admin/runs/<int>
    CROW_ROUTE(app, "/api/admin/runs/<int>").methods("DELETE"_method)
    ([&ctx](const crow::request &req, int runId) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        if (!ctx.admin.deleteRun(runId, error))
            return jsonError(404, error);

        crow::json::wvalue j;
        j["deleted"] = runId;
        return crow::response(200, j);
    });

    // POST /api/admin/demo/seed
    CROW_ROUTE(app, "/api/admin/demo/seed").methods("POST"_method)
    ([&ctx](const crow::request &req) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        int seeded = 0;
        string error;
        if (!ctx.admin.seedDemoCohort(nowText(), seeded, error))
            return jsonError(500, error);

        crow::json::wvalue j;
        j["seeded"] = seeded;
        return crow::response(200, j);
    });

    // DELETE /api/admin/demo/seed
    CROW_ROUTE(app, "/api/admin/demo/seed").methods("DELETE"_method)
    ([&ctx](const crow::request &req) {
        string caller = currentUser(req);
        if (caller.empty())
            return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        int removed = 0;
        string error;
        if (!ctx.admin.clearDemoCohort(removed, error))
            return jsonError(500, error);

        crow::json::wvalue j;
        j["removed"] = removed;
        return crow::response(200, j);
    });
}
