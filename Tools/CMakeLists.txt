cmake_minimum_required(VERSION 3.21)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

option(TRACY_ENABLE "" ON)
option(TRACY_ON_DEMAND "" ON)

add_subdirectory("tracy")

set_target_properties(TracyClient PROPERTIES FOLDER "Tools")
