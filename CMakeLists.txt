cmake_minimum_required(VERSION 3.14)
project(GoldiProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_SYSROOT /Library/Developer/CommandLineTools/SDKs/MacOSX13.3.sdk)

#list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")
#

include_directories(/Library/gurobi1003/macos_universal2/include/)
include_directories("inc")
file(GLOB SOURCES "src/*.cpp")
add_executable(GoldiProject main.cpp ${SOURCES})
add_executable(Search runSearch.cpp ${SOURCES})
add_executable(LNS runLNS.cpp ${SOURCES})

target_link_libraries(GoldiProject
        /Library/gurobi1003/macos_universal2/lib/libgurobi_c++.a
        /Library/gurobi1003/macos_universal2/lib/libgurobi100.dylib
        )

target_link_libraries(Search
        /Library/gurobi1003/macos_universal2/lib/libgurobi_c++.a
        /Library/gurobi1003/macos_universal2/lib/libgurobi100.dylib
        )

target_link_libraries(LNS
        /Library/gurobi1003/macos_universal2/lib/libgurobi_c++.a
        /Library/gurobi1003/macos_universal2/lib/libgurobi100.dylib
        )
#find_path(GUROBI_INCLUDE_DIR
#        NAMES gurobi_c.h
#        HINTS "${GUROBI_DIR}/include" "${GUROBI_DIR}/*/include" "$ENV{GUROBI_DIR}/include" "$ENV{GUROBI_DIR}/*/include")
#find_library(GUROBI_LIBRARY
#        NAMES gurobi100 gurobi95 gurobi91 gurobi90 gurobi81 gurobi80 gurobi75 gurobi70
#        HINTS "${GUROBI_DIR}/lib" "${GUROBI_DIR}/*/lib" "$ENV{GUROBI_DIR}/lib" "$ENV{GUROBI_DIR}/*/lib")
#if (GUROBI_INCLUDE_DIR AND GUROBI_LIBRARY)
#    set(LPS grb CACHE STRING "options for LP solver")
#    target_include_directories(GoldiProject PUBLIC ${GUROBI_INCLUDE_DIR})
#    target_link_libraries(GoldiProject ${GUROBI_LIBRARY})
#    message("Using Gurobi at ${GUROBI_LIBRARY} as LP solver")
#endif ()


#include_directories(/Library/gurobi1002/macos_universal2/include/)

# Add your project's executable
#add_executable(GoldiProject main.cpp)

# Link the Gurobi libraries
#target_link_libraries(GoldiProject GUROBI)