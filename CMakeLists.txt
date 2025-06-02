# Minimum Version for CMAKE. This file was generally used for the containerized version of the application programmer interface.
# You may reference https://github.com/kandrsn99/Docker-PostgreSQL-and-Crow-API for how that might be set up.
cmake_minimum_required(VERSION 3.10)
project(CrowApp) # Create the Project
# Crow makes usage of C++ 17 Standards.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# We will use the FetchContent to download our necessary repositories for the libraries we shall use.
include(FetchContent)
# We will use the FetchContent to download the Asio Repository. Thus, ensuring it is in our environment for compilation.
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git # https://github.com/boostorg/asio.git
  GIT_TAG master  # Master tag on github. Choose your favorite tag.
)
FetchContent_MakeAvailable(asio)
# We have to set the directory, otherwise crow won't have a file to find for building. We also set the flag to set up.
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "ASIO include directory")
set(ASIO_FOUND TRUE CACHE BOOL "ASIO is found")
include_directories(${ASIO_INCLUDE_DIR})
# Get the Crow repository.
FetchContent_Declare(
  crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG master  # Specification for release or commit tag from the repository.
)
FetchContent_MakeAvailable(crow)
# Create the target library alias for crow when it is linked later for server.cpp file.
if(NOT TARGET crow::crow)
    add_library(crow INTERFACE)
    target_include_directories(crow INTERFACE ${crow_SOURCE_DIR}/include)
    add_library(crow::crow ALIAS crow)
endif()
# FetchContent to download the libpqxx library for us to use.
FetchContent_Declare(
  libpqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG master # Master tag on github. Choose your favorite tag.
)
FetchContent_MakeAvailable(libpqxx)
# If we recieved pqxx_static we create an alias for when pqxx is linked later for server.cpp file.
if(TARGET pqxx_static AND NOT TARGET pqxx::pqxx)
    add_library(pqxx::pqxx ALIAS pqxx_static)
endif()
# Otherwise, if we have pqxx we add the library with the proper alias.
if(TARGET pqxx AND NOT TARGET pqxx::pqxx)
    add_library(pqxx::pqxx ALIAS pqxx)
endif()
# FetchContent to download the nlohmann json library for us to use.
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.10.5 # You can change this to the specific tag or commit you require.
)
FetchContent_MakeAvailable(nlohmann_json)
# Find PostgreSQL
find_package(PostgreSQL REQUIRED)
# Add the cpp file and subsequent executable.
add_executable(server server.cpp)
# Link libpqxx to PostgreSQL, a C dependency.
target_link_libraries(server PRIVATE pqxx::pqxx ${PostgreSQL_LIBRARIES})
target_include_directories(server PRIVATE ${PostgreSQL_INCLUDE_DIRS})
# Thread support for crow (multithreading).
find_package(Threads REQUIRED)
# Target libraries during the build and link.
target_link_libraries(server PRIVATE crow::crow pqxx::pqxx Threads::Threads nlohmann_json::nlohmann_json)
