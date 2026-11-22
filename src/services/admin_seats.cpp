#include "services/admin_seats.hpp"

#include <stdexcept>
#include <string>

#include "hashmap.hpp"

namespace
{

// Puts the admin's counts into a lookup book: university name -> seats.
// We use our HashMap so that, later, "did the admin set this university?" is answered
// instantly instead of searching the admin's whole list once per university.
// If the same name appears twice, the LATER entry replaces the earlier one.
void loadAdminCounts(const Vector<AdminSeatCount> &adminCounts, HashMap<std::string, int> &book)
{
    for (int i = 0; i < adminCounts.size(); ++i)
        book[adminCounts[i].slug] = adminCounts[i].seats;
}

// The text shown for a seat, e.g. "uva #2".
std::string seatLabel(const std::string &slug, int number)
{
    return slug + " #" + std::to_string(number);
}

} // namespace

// Builds the whole table once, in local variables, and keeps it only if every
// university's seat count is valid. On any problem we return early and the
// members stay empty, so an invalid table can never hold half-built data.
AdminSeatTable::AdminSeatTable(const Vector<AdminUniversity> &universities,
                               const Vector<AdminSeatCount> &adminCounts,
                               int defaultSeats)
    : isValid(false)
{
    if (defaultSeats < 0)
        return;

    HashMap<std::string, int> book;
    loadAdminCounts(adminCounts, book);

    Vector<AdminSeatSlot> newSlots;
    Vector<AdminUniversitySeats> newInfo;

    for (int u = 0; u < universities.size(); ++u)
    {
        const std::string &slug = universities[u].slug;

        // Use the admin's number if there is one, otherwise the default (marked "assumed").
        AdminUniversitySeats entry;
        entry.universityIndex = u;
        entry.totalSeats = defaultSeats;
        entry.assumed = true;
        if (book.search(slug))
        {
            entry.totalSeats = book.at(slug);
            entry.assumed = false;
        }

        if (entry.totalSeats < 0)
            return; // invalid input: leave the table empty and invalid

        // Create this university's seats. Seat numbers run on across universities.
        if (entry.totalSeats > 0)
        {
            entry.firstSeatIndex = newSlots.size();
            for (int n = 1; n <= entry.totalSeats; ++n)
            {
                AdminSeatSlot seat;
                seat.universityIndex = u;
                seat.seatNumber = n;
                seat.label = seatLabel(slug, n);
                newSlots.push_back(seat);
            }
        }
        newInfo.push_back(entry);
    }

    slots = newSlots;
    info = newInfo;
    isValid = true;
}

// True if the table is usable.
bool AdminSeatTable::valid() const
{
    return isValid;
}

// Total number of seats.
int AdminSeatTable::seatCount() const
{
    return slots.size();
}

// Number of universities the table was built from.
int AdminSeatTable::universityCount() const
{
    return info.size();
}

// One seat by position. Throws if the position does not exist.
// (We check the range ourselves because Vector::operator[] does not.)
const AdminSeatSlot &AdminSeatTable::slot(int seatIndex) const
{
    if (seatIndex < 0 || seatIndex >= slots.size())
        throw std::out_of_range("Seat index is out of range");
    return slots[seatIndex];
}

// One university's seat summary by position. Throws if it does not exist.
const AdminUniversitySeats &AdminSeatTable::universitySeats(int universityIndex) const
{
    if (universityIndex < 0 || universityIndex >= info.size())
        throw std::out_of_range("University index is out of range");
    return info[universityIndex];
}

// The way back from a seat to its university. -1 for a seat that does not exist.
int AdminSeatTable::universityOfSeat(int seatIndex) const
{
    if (seatIndex < 0 || seatIndex >= slots.size())
        return -1;
    return slots[seatIndex].universityIndex;
}

// Turns the scorer's pairs into the matcher's input. Each ELIGIBLE pair becomes one
// line per seat of its university, with the score copied unchanged. Because the
// university's seats are numbered one after another starting at firstSeatIndex, we
// find them with a simple loop, no searching. Order is fixed, so results repeat.
Vector<AdminSeatEdge> AdminSeatTable::buildEdges(const Vector<PairResult> &pairs) const
{
    Vector<AdminSeatEdge> edges;
    if (!isValid)
        return edges;

    for (int i = 0; i < pairs.size(); ++i)
    {
        const PairResult &pair = pairs[i];
        if (!pair.eligible())
            continue;
        if (pair.universityIndex < 0 || pair.universityIndex >= info.size())
            continue; // points at a university we do not know: skip it

        const AdminUniversitySeats &uni = info[pair.universityIndex];
        for (int k = 0; k < uni.totalSeats; ++k)
        {
            AdminSeatEdge edge;
            edge.studentIndex = pair.studentIndex;
            edge.seatIndex = uni.firstSeatIndex + k;
            edge.score = pair.score;
            edges.push_back(edge);
        }
    }
    return edges;
}

// Counts filled and left seats per university from the seats students ended up in.
// A seat number that does not exist, or one that appears twice (two students in one
// seat), means a bug upstream, so we report "invalid" instead of guessing.
AdminSeatUsage AdminSeatTable::countSeatsLeft(const Vector<int> &placedSeatIndices) const
{
    AdminSeatUsage result;
    if (!isValid)
        return result;

    Vector<bool> taken(slots.size(), false);
    Vector<int> filled(info.size(), 0);

    for (int i = 0; i < placedSeatIndices.size(); ++i)
    {
        int seat = placedSeatIndices[i];
        if (seat < 0 || seat >= slots.size())
            return result; // no such seat
        if (taken[seat])
            return result; // same seat used twice
        taken[seat] = true;
        filled[slots[seat].universityIndex]++;
    }

    // Every university gets an entry, including ones with nobody placed.
    for (int u = 0; u < info.size(); ++u)
    {
        AdminUniversityUsage entry;
        entry.universityIndex = u;
        entry.totalSeats = info[u].totalSeats;
        entry.filled = filled[u];
        entry.left = info[u].totalSeats - filled[u];
        result.usage.push_back(entry);
    }
    result.valid = true;
    return result;
}