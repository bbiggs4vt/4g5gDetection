#pragma once

#include <vector>

namespace cellsearch {

/// NR synchronization raster (GSCN, TS 38.101-1 table 5.4.3.1-1): the only
/// frequencies an SSB may be centered on. Returns the SS_ref points within
/// `half_span_hz` of `fc_hz`, sorted nearest-to-fc first, capped at
/// `max_candidates`. FR2 (> 24.25 GHz) is not enumerated.
std::vector<double> gscn_ss_ref_hz(double fc_hz, double half_span_hz,
                                   std::size_t max_candidates = 32);

} // namespace cellsearch
