cmake_minimum_required (VERSION 2.6)
set (CMAKE_CXX_STANDARD_11)
project (MODSIM)
set(APP_TARGET MODSIM)

include_directories(
  src
  lib
)

add_library(simlib STATIC
  lib/simlib.c
)

add_executable(tscm src/tscomp2q.c)
target_link_libraries(
    tscm
    simlib
)