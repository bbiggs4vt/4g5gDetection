#include "src/gscn.h"

#include <algorithm>
#include <cmath>

namespace cellsearch {

namespace {

void push_if_in_window(std::vector<double>& out, double ss_ref, double fc_hz,
                       double half_span_hz) {
  if (std::abs(ss_ref - fc_hz) <= half_span_hz) {
    out.push_back(ss_ref);
  }
}

} // namespace

std::vector<double> gscn_ss_ref_hz(double fc_hz, double half_span_hz,
                                   std::size_t max_candidates) {
  std::vector<double> out;
  if (half_span_hz <= 0.0) {
    return out;
  }
  const double lo = fc_hz - half_span_hz;
  const double hi = fc_hz + half_span_hz;

  // 0 – 3000 MHz: SS_ref = N * 1200 kHz + M * 50 kHz, N = 1..2499, M ∈ {1,3,5}.
  if (lo < 3e9) {
    const long n_lo = std::max(1L, static_cast<long>(std::floor(lo / 1.2e6)) - 1);
    const long n_hi = std::min(2499L, static_cast<long>(std::ceil(hi / 1.2e6)) + 1);
    for (long n = n_lo; n <= n_hi; ++n) {
      for (int m : {1, 3, 5}) {
        const double ss = n * 1.2e6 + m * 50e3;
        if (ss < 3e9) {
          push_if_in_window(out, ss, fc_hz, half_span_hz);
        }
      }
    }
  }

  // 3000 – 24250 MHz: SS_ref = 3000 MHz + N * 1.44 MHz, N = 0..14756.
  if (hi >= 3e9 && lo < 24.25e9) {
    const long n_lo =
        std::max(0L, static_cast<long>(std::floor((lo - 3e9) / 1.44e6)) - 1);
    const long n_hi = std::min(
        14756L, static_cast<long>(std::ceil((hi - 3e9) / 1.44e6)) + 1);
    for (long n = n_lo; n <= n_hi; ++n) {
      push_if_in_window(out, 3e9 + n * 1.44e6, fc_hz, half_span_hz);
    }
  }

  std::sort(out.begin(), out.end(), [fc_hz](double a, double b) {
    return std::abs(a - fc_hz) < std::abs(b - fc_hz);
  });
  if (out.size() > max_candidates) {
    out.resize(max_candidates);
  }
  return out;
}

} // namespace cellsearch
