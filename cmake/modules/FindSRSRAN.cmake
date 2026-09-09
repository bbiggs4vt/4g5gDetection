# FindSRSRAN
# ----------
# Locates an installed srsRAN (srsRAN_4G) PHY library — the DSP engine for
# both the LTE and NR branches. Defines the imported target:
#
#   srsran::phy   libsrsran_phy plus its transitive deps (fftw3f, m, pthread)
#
# Hints:
#   SRSRAN_ROOT (CMake or environment variable) — install prefix of srsRAN,
#   e.g. -DSRSRAN_ROOT=/opt/srsran for a `make install`ed tree.

find_path(SRSRAN_INCLUDE_DIR
  NAMES srsran/srsran.h
  HINTS ${SRSRAN_ROOT} $ENV{SRSRAN_ROOT}
  PATH_SUFFIXES include
)

find_library(SRSRAN_PHY_LIBRARY
  NAMES srsran_phy
  HINTS ${SRSRAN_ROOT} $ENV{SRSRAN_ROOT}
  PATH_SUFFIXES lib lib64
)

# libsrsran_phy is usually static, so its deps must be linked explicitly.
find_library(FFTW3F_LIBRARY NAMES fftw3f)
find_package(Threads QUIET)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SRSRAN
  REQUIRED_VARS SRSRAN_INCLUDE_DIR SRSRAN_PHY_LIBRARY FFTW3F_LIBRARY
  FAIL_MESSAGE "srsRAN not found. Install srsRAN_4G (headers + libsrsran_phy) or set -DSRSRAN_ROOT=<prefix>."
)

if(SRSRAN_FOUND AND NOT TARGET srsran::phy)
  add_library(srsran::phy UNKNOWN IMPORTED)
  set_target_properties(srsran::phy PROPERTIES
    IMPORTED_LOCATION "${SRSRAN_PHY_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SRSRAN_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${FFTW3F_LIBRARY};Threads::Threads;m"
  )
endif()

mark_as_advanced(SRSRAN_INCLUDE_DIR SRSRAN_PHY_LIBRARY FFTW3F_LIBRARY)
