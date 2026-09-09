cmake_minimum_required(VERSION 3.16)
project(cellsearch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# srsRAN is the PHY engine for both RATs (sync + PBCH/MIB); see FindSRSRAN.cmake
# for the search paths and the SRSRAN_ROOT hint.
find_package(SRSRAN REQUIRED)

# Core decode library: IQ stream in, CellRecord out. No SDR/driver code here.
add_library(cellsearch
  src/detector.cc
  src/framing.cc
  src/lte_branch.cc
  src/nr_branch.cc
  src/file_iq_stream.cc
)
target_include_directories(cellsearch
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(cellsearch PRIVATE srsran::phy)
target_compile_options(cellsearch PRIVATE -Wall -Wextra)

add_executable(cellsearch_probe apps/cellsearch_probe.cc)
target_link_libraries(cellsearch_probe PRIVATE cellsearch)

enable_testing()
add_executable(framing_test tests/framing_test.cc)
target_include_directories(framing_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framing_test PRIVATE cellsearch)
add_test(NAME framing_test COMMAND framing_test)

# End-to-end decode tests against known captures (git-ignored; regenerate
# with scripts/gen_lte_captures.sh). Skipped when a capture is absent.
add_executable(lte_e2e_test tests/lte_e2e_test.cc)
target_link_libraries(lte_e2e_test PRIVATE cellsearch)
set(CAPTURES ${CMAKE_CURRENT_SOURCE_DIR}/captures)
add_test(NAME lte_e2e_6prb COMMAND lte_e2e_test
         ${CAPTURES}/lte_pci123_fs1.92e6_fc806e6.fc32 1.92e6 806e6 123 6)
add_test(NAME lte_e2e_50prb_decim COMMAND lte_e2e_test
         ${CAPTURES}/lte_pci321_fs11.52e6_fc1842.5e6.fc32 11.52e6 1842.5e6 321 50)
set_tests_properties(lte_e2e_6prb lte_e2e_50prb_decim PROPERTIES
                     SKIP_RETURN_CODE 77)
