cmake_minimum_required(VERSION 3.10)
project(FloodEvacuationPlanner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE src/graph.cpp src/planner.cpp)

# Console version
add_executable(planner src/main.cpp ${CORE})

# GUI version (only built if SFML 3 is installed)
find_package(SFML 3 COMPONENTS Graphics QUIET)
if(SFML_FOUND)
    add_executable(planner_gui gui/gui_main.cpp ${CORE})
    target_link_libraries(planner_gui PRIVATE SFML::Graphics)
else()
    message(STATUS "SFML 3 not found: building console version only")
endif()
