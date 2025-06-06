cmake_minimum_required(VERSION 3.10)
project(alpha_strikes_tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(GTest REQUIRED)
find_package(Crow REQUIRED)
find_package(pqxx REQUIRED)
find_package(nlohmann_json REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# Add test executables
add_executable(unit_tests
    unit/test_main.cpp
    unit/health_test.cpp
    unit/location_test.cpp
    unit/incident_test.cpp
    unit/totals_test.cpp
)

# Link libraries
target_link_libraries(unit_tests
    GTest::GTest
    GTest::Main
    Crow::Crow
    pqxx
    nlohmann_json::nlohmann_json
)

# Add integration tests
add_executable(integration_tests
    integration/test_main.cpp
    integration/api_test.cpp
)

# Link libraries for integration tests
target_link_libraries(integration_tests
    GTest::GTest
    GTest::Main
    Crow::Crow
    pqxx
    nlohmann_json::nlohmann_json
)

# Enable testing
enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
add_test(NAME integration_tests COMMAND integration_tests) 