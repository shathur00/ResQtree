// haversine.h
//
// Great-circle distance helpers, all header-only and `inline` so that any
// .cpp file can include this header without a matching .cpp file.
//
// The Earth is treated as a sphere of radius 6371.0 km. That is the mean
// radius of the Earth and is accurate enough for a city / regional scale
// dataset (the error against the WGS-84 ellipsoid is well under 0.5%).
//
// Besides the real distance function we provide two *lower bound*
// helpers. A lower bound is a number that is guaranteed to be <= the
// real distance. The KD-Tree uses them to decide whether it is safe to
// skip ("prune") a whole branch of the tree: if even the smallest
// possible distance to that branch is bigger than the best answer found
// so far, the branch cannot contain a better answer and may be ignored.
//
// If a lower bound were ever WRONG (too large), the KD-Tree could throw
// away the true nearest facility, so the reasoning behind each bound is
// spelled out below.
#ifndef RESQTREE_HAVERSINE_H
#define RESQTREE_HAVERSINE_H

#include <algorithm>
#include <cmath>

namespace haversine {

// Mean radius of the Earth in kilometres.
constexpr double kEarthRadiusKm = 6371.0;
// Pi, spelled out so we do not need <numbers> (C++20) or M_PI (non-standard).
constexpr double kPi = 3.14159265358979323846;

inline double degToRad(double degrees) {
    return degrees * kPi / 180.0;
}

// ---------------------------------------------------------------------------
// haversineKm: the real great-circle distance between two points.
// ---------------------------------------------------------------------------
//
// Inputs are (latitude, longitude) in DEGREES. Output is kilometres.
//
// The Haversine formula measures the angle theta at the centre of the
// Earth between the two points and multiplies it by the radius:
//
//   a = sin^2(dLat/2) + cos(lat1) * cos(lat2) * sin^2(dLon/2)
//   theta = 2 * asin( sqrt(a) )
//   distance = R * theta
//
// `a` is the square of the sine of the *half* angle, which is what makes
// the formula numerically stable for small distances.
inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double dLat = degToRad(lat2 - lat1);
    const double dLon = degToRad(lon2 - lon1);

    const double sinHalfDLat = std::sin(dLat / 2.0);
    const double sinHalfDLon = std::sin(dLon / 2.0);

    double a = sinHalfDLat * sinHalfDLat +
               std::cos(degToRad(lat1)) * std::cos(degToRad(lat2)) *
                   sinHalfDLon * sinHalfDLon;

    // Floating point rounding can push `a` a hair outside [0, 1], and
    // sqrt() of a negative number is NaN. Clamping keeps the result sane.
    a = std::min(1.0, std::max(0.0, a));

    return 2.0 * kEarthRadiusKm * std::asin(std::sqrt(a));
}

// ---------------------------------------------------------------------------
// latLineDistKm: lower bound for "how far am I from the line lat = splitLat?"
// ---------------------------------------------------------------------------
//
// Axis 0 of the KD-Tree splits the world along parallels (lines of constant
// latitude). This function returns the distance from the target point to
// the parallel that passes through the splitting point.
//
// Why is this a valid lower bound?
//   The parallel at latitude `splitLat` is the circle of all points that
//   have that latitude. The point of that parallel closest to the target
//   is the one on the target's own meridian, and the distance to it is
//   exactly the north-south arc length, R * |targetLat - splitLat| (in
//   radians). Every other point of that parallel is further away, and
//   every point beyond that parallel is further still. So no point that
//   lies on the far side of the split can be closer than this value.
//
//   (Latitude never wraps around, so unlike longitude there is no
//   special case to worry about here.)
inline double latLineDistKm(double targetLat, double splitLat) {
    return kEarthRadiusKm * std::fabs(degToRad(targetLat - splitLat));
}

// ---------------------------------------------------------------------------
// lonLineDistKm: lower bound for "how far am I from the meridian splitLon?"
// ---------------------------------------------------------------------------
//
// Axis 1 of the KD-Tree splits the world along meridians (lines of constant
// longitude). This function returns the minimum great-circle distance from
// the target to the meridian that passes through the splitting point.
//
// Why is this a valid lower bound?
//   Take the target T = (targetLat, targetLon) and the meridian M through
//   splitLon. Let dlon be the longitude difference, normalised to
//   [-pi, pi] so that we always take the *short way around* the Earth.
//   The closest point of M to T is the point where the great circle
//   through T and the pole crosses M, and the distance to it works out to
//
//       R * asin( | cos(targetLat) * sin(dlon) | )
//
//   The two factors have an intuitive meaning: sin(dlon) is how far round
//   the Earth we are, and cos(targetLat) shrinks that distance towards
//   the poles (at the north pole every meridian passes through the same
//   point, so the distance is zero - exactly what cos(90 degrees) = 0
//   gives us). Any other point of the meridian is further away, so this
//   value is a lower bound for the whole meridian.
//
//   Note that the great circle from the target to the pole crosses the
//   meridian at splitLon AND at splitLon + 180 degrees, so the value
//   bounds the distance to that whole great circle. That is exactly what
//   we want, because a "meridian at splitLon" in spherical geometry is
//   that great circle, and its two halves are the meridians at splitLon
//   and at splitLon + 180.
//
// Conservative special case:
//   If the target and the splitting meridian are more than 90 degrees of
//   longitude apart, we return 0. Zero means "I promise nothing", so the
//   KD-Tree simply does not prune that branch. This costs a little speed
//   but can never discard a closer facility.
//
// This function is the bound for the splitting LINE. The KD-Tree actually
// prunes a whole branch, which holds a range of longitudes rather than a
// single line - see lonFarBoundKm below for the bound used in practice.
inline double lonLineDistKm(double targetLat, double targetLon, double splitLon) {
    const double targetLatRad = degToRad(targetLat);

    // Longitude difference in radians, normalised to [-pi, pi]: this is
    // the shortest way round the Earth, so 350 degrees becomes -10.
    double dlon = degToRad(splitLon - targetLon);
    dlon = std::remainder(dlon, 2.0 * kPi);

    // Too far apart to say anything useful - be conservative.
    if (std::fabs(dlon) > kPi / 2.0) {
        return 0.0;
    }

    // |cos(lat) * sin(dlon)| is always <= 1, so asin() is safe here.
    return kEarthRadiusKm * std::asin(std::fabs(std::cos(targetLatRad) * std::sin(dlon)));
}

// ---------------------------------------------------------------------------
// minAbsSinOverRange: the smallest |sin| reached on a range of angles.
// ---------------------------------------------------------------------------
//
// Angles are in degrees and the range is the closed interval [a, b].
//
// |sin| is zero at every multiple of 180 degrees, so if the interval
// contains one of those the answer is 0. Otherwise the smallest value is
// reached at one of the two ends of the interval.
inline double minAbsSinOverRange(double a, double b) {
    // Look for a multiple of 180 inside [a, b].
    const double firstK = std::floor(a / 180.0);
    const double lastK = std::ceil(b / 180.0);
    for (double k = firstK; k <= lastK; k += 1.0) {
        const double zero = 180.0 * k;
        if (zero >= a && zero <= b) {
            return 0.0;
        }
    }

    // No zero inside the interval, so the minimum is at an end.
    return std::min(std::fabs(std::sin(degToRad(a))),
                    std::fabs(std::sin(degToRad(b))));
}

// ---------------------------------------------------------------------------
// lonFarBoundKm: lower bound for the FAR child of a longitude split.
// ---------------------------------------------------------------------------
//
// Why is this needed when lonLineDistKm() already exists?
//
// lonLineDistKm() measures the distance to the splitting MERIDIAN. But
// the branch the KD-Tree is about to prune holds every point whose
// longitude lies in a whole RANGE, not just on the meridian:
//
//   far child (high side): longitudes in [splitLon, 180]
//   far child (low side) : longitudes in [-180, splitLon]
//
// The longitudes are stored in the plain range [-180, 180], but the Earth
// is round, so a range like [170, 180] is really the arc that runs east
// from 170 degrees to 180 degrees - and it *wraps around* to -180.
//
// Concrete example of the bug this prevents: the target is at longitude
// -179 and the tree splits at longitude 170. The far child then holds
// longitudes in [170, 180], and the point at longitude 175 in that
// branch is only 6 degrees of longitude away from the target - the
// target is one degree past the antimeridian, right next to 180. The
// distance to the splitting meridian is about 1223 km, so a bound based
// on the meridian alone would prune that branch and throw away a
// facility that was in fact much closer.
//
// The fix is to bound the distance by the nearest longitude in the
// branch's whole range:
//
//   For every longitude L in the range, the closest point of that
//   meridian to the target is at distance
//       R * asin( |cos(lat) * sin(L - targetLon)| )
//   and the distance to the meridian is never larger than the distance
//   to any point on it, so the value above is always a valid lower
//   bound for everything in that slice. Taking the smallest such value
//   over the whole range gives a bound that is valid for every point
//   in the branch.
//
// The longitude gap that matters is the "circular" one, and
// |sin(L - targetLon)| is exactly the factor that makes the two
// neighbouring longitudes around the antimeridian count as close.
//
// `farIsHigh` tells us which side of the split the branch sits on.
inline double lonFarBoundKm(double targetLat, double targetLon, double splitLon,
                             bool farIsHigh) {    // The longitude range occupied by the branch we are about to prune.
    const double lo = farIsHigh ? splitLon : -180.0;
    const double hi = farIsHigh ? 180.0 : splitLon;

    // The smallest |sin| over that whole range, relative to the target.
    const double minSin = minAbsSinOverRange(lo - targetLon, hi - targetLon);

    const double latRad = degToRad(targetLat);
    const double value = std::fabs(std::cos(latRad) * minSin);

    // value is in [0, 1] by construction, but clamp anyway for safety.
    return kEarthRadiusKm * std::asin(std::min(1.0, value));
}

}  // namespace haversine

#endif  // RESQTREE_HAVERSINE_H
