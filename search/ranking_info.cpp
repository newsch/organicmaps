#include "search/ranking_info.hpp"

#include "ugc/types.hpp"

#include <iomanip>
#include <limits>
#include <sstream>

using namespace std;

namespace search
{
namespace
{
// See search/search_quality/scoring_model.py for details.  In short,
// these coeffs correspond to coeffs in a linear model.
double constexpr kDistanceToPivot = -0.8175524;
double constexpr kRank = 1.0000000;
// todo: (@t.yan) Adjust.
double constexpr kPopularity = 0.0500000;
// todo: (@t.yan) Adjust.
double constexpr kRating = 0.0500000;
double constexpr kFalseCats = -0.3745520;
double constexpr kErrorsMade = -0.1090870;
double constexpr kMatchedFraction = 0.7859737;
double constexpr kAllTokensUsed = 1.0000000;
double constexpr kHasName = 0.5;
double constexpr kNameScore[NameScore::NAME_SCORE_COUNT] = {
  -0.1752510 /* Zero */,
  0.0309111 /* Substring */,
  0.0127291 /* Prefix */,
  0.1316108 /* Full Match */
};
double constexpr kType[Model::TYPE_COUNT] = {
  -0.1554708 /* POI */,
  -0.1554708 /* Building */,
  -0.1052415 /* Street */,
  -0.1650949 /* Unclassified */,
  -0.1556262 /* Village */,
  0.1771632 /* City */,
  0.0604687 /* State */,
  0.3438015 /* Country */
};

// Coeffs sanity checks.
static_assert(kDistanceToPivot <= 0, "");
static_assert(kRank >= 0, "");
static_assert(kPopularity >= 0, "");
static_assert(kErrorsMade <= 0, "");
static_assert(kHasName >= 0, "");

double TransformDistance(double distance)
{
  return min(distance, RankingInfo::kMaxDistMeters) / RankingInfo::kMaxDistMeters;
}

double TransformRating(pair<uint8_t, float> const & rating)
{
  double r = 0.0;
  // From statistics.
  double constexpr kAverageRating = 7.6;
  if (rating.first != 0)
  {
    r = (static_cast<double>(rating.second) - kAverageRating) /
        (ugc::UGC::kMaxRating - ugc::UGC::kRatingDetalizationThreshold);
    r *= static_cast<double>(rating.first) / 3.0 /* maximal confidence */;
  }
  return r;
}
}  // namespace

// static
double const RankingInfo::kMaxDistMeters = 2e6;

// static
void RankingInfo::PrintCSVHeader(ostream & os)
{
  os << "DistanceToPivot"
     << ",Rank"
     << ",Popularity"
     << ",Rating"
     << ",NameScore"
     << ",ErrorsMade"
     << ",MatchedFraction"
     << ",SearchType"
     << ",PureCats"
     << ",FalseCats"
     << ",AllTokensUsed"
     << ",IsCategorialRequest"
     << ",HasName";
}

string DebugPrint(RankingInfo const & info)
{
  ostringstream os;
  os << boolalpha;
  os << "RankingInfo [";
  os << "m_distanceToPivot:" << info.m_distanceToPivot;
  os << ", m_rank:" << static_cast<int>(info.m_rank);
  os << ", m_popularity:" << static_cast<int>(info.m_popularity);
  os << ", m_rating:[" << static_cast<int>(info.m_rating.first) << ", " << info.m_rating.second
     << "]";
  os << ", m_nameScore:" << DebugPrint(info.m_nameScore);
  os << ", m_errorsMade:" << DebugPrint(info.m_errorsMade);
  os << ", m_maxErrorsMade:" << info.m_maxErrorsMade;
  os << ", m_matchedFraction:" << info.m_matchedFraction;
  os << ", m_type:" << DebugPrint(info.m_type);
  os << ", m_pureCats:" << info.m_pureCats;
  os << ", m_falseCats:" << info.m_falseCats;
  os << ", m_allTokensUsed:" << info.m_allTokensUsed;
  os << ", m_categorialRequest:" << info.m_categorialRequest;
  os << ", m_hasName:" << info.m_hasName;
  os << "]";
  return os.str();
}

void RankingInfo::ToCSV(ostream & os) const
{
  os << fixed;
  os << m_distanceToPivot << ",";
  os << static_cast<int>(m_rank) << ",";
  os << static_cast<int>(m_popularity) << ",";
  os << TransformRating(m_rating) << ",";
  os << DebugPrint(m_nameScore) << ",";
  os << GetErrorsMade() << ",";
  os << m_matchedFraction << ",";
  os << DebugPrint(m_type) << ",";
  os << m_pureCats << ",";
  os << m_falseCats << ",";
  os << (m_allTokensUsed ? 1 : 0) << ",";
  os << (m_categorialRequest ? 1 : 0) << ",";
  os << (m_hasName ? 1 : 0);
}

double RankingInfo::GetLinearModelRank() const
{
  // NOTE: this code must be consistent with scoring_model.py.  Keep
  // this in mind when you're going to change scoring_model.py or this
  // code. We're working on automatic rank calculation code generator
  // integrated in the build system.
  double const distanceToPivot = TransformDistance(m_distanceToPivot);
  double const rank = static_cast<double>(m_rank) / numeric_limits<uint8_t>::max();
  double const popularity = static_cast<double>(m_popularity) / numeric_limits<uint8_t>::max();
  double const rating = TransformRating(m_rating);

  auto nameScore = m_nameScore;
  if (m_pureCats || m_falseCats)
  {
    // If the feature was matched only by categorial tokens, it's
    // better for ranking to set name score to zero.  For example,
    // when we're looking for a "cafe", cafes "Cafe Pushkin" and
    // "Lermontov" both match to the request, but must be ranked in
    // accordance to their distances to the user position or viewport,
    // in spite of "Cafe Pushkin" has a non-zero name rank.
    nameScore = NAME_SCORE_ZERO;
  }

  double result = 0.0;
  result += kDistanceToPivot * distanceToPivot;
  result += kRank * rank;
  result += kPopularity * popularity;
  result += kRating * rating;
  result += m_falseCats * kFalseCats;
  if (!m_categorialRequest)
  {
    result += kType[m_type];
    result += kNameScore[nameScore];
    result += kErrorsMade * GetErrorsMade();
    result += kMatchedFraction * m_matchedFraction;
    result += (m_allTokensUsed ? 1 : 0) * kAllTokensUsed;
  }
  else
  {
    result += m_hasName * kHasName;
  }
  return result;
}

double RankingInfo::GetErrorsMade() const
{
  if (!m_errorsMade.IsValid())
    return 1.0;

  if (m_maxErrorsMade == 0)
    return 0.0;

  return static_cast<double>(m_errorsMade.m_errorsMade) / static_cast<double>(m_maxErrorsMade);
}
}  // namespace search
