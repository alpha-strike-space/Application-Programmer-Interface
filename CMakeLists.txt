# Minimum Version for CMAKE. This file was generally used for the containerized version of the application programmer interface.
# You may reference https://github.com/kandrsn99/Docker-PostgreSQL-and-Crow-API for how that might be set up.
cmake_minimum_required(VERSION 3.14)
project(CrowApp) # Create the Project

# Crow makes usage of C++ 17 Standards.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages before using their variables
find_package(PostgreSQL REQUIRED)
find_package(Threads REQUIRED)

# We will use the FetchContent to download our necessary repositories for the libraries we shall use.
include(FetchContent)

# FetchContent to download the Asio Repository.
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git # https://github.com/boostorg/asio.git
  GIT_TAG master  # Specification for release or commit tag from the repository.
)
FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "ASIO include directory")
set(ASIO_FOUND TRUE CACHE BOOL "ASIO is found")

# Get the Crow repository.
FetchContent_Declare(
  crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG v1.2.1.2  # Specification for release or commit tag from the repository.
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
  GIT_TAG 7.8.1 # Specification for release or commit tag from the repository.
)
FetchContent_MakeAvailable(libpqxx)

# If we received pqxx_static we create an alias for when pqxx is linked later for server.cpp file.
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
  GIT_TAG v3.12.0  # Specification for release or commit tag from the repository.
)
FetchContent_MakeAvailable(nlohmann_json)

# Add your source files here (update this list as needed)
set(SOURCES
    Server.cpp
    Routes.cpp
    pgListener.cpp
    Serializer.cpp
)

# Put together
add_executable(server ${SOURCES})

# Includes from dependencies
target_include_directories(server PRIVATE
    ${crow_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/single_include
    ${libpqxx_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PostgreSQL_INCLUDE_DIRS}
    ${ASIO_INCLUDE_DIR}
)

# Target libraries. Use pqxx::pqxx if available, otherwise fallback.
if(TARGET pqxx::pqxx)
    set(PQXX_TARGET pqxx::pqxx)
else()
    set(PQXX_TARGET pqxx)
endif()

target_link_libraries(server PRIVATE
    nlohmann_json::nlohmann_json
    ${PQXX_TARGET}
    crow::crow
    Threads::Threads
    ${PostgreSQL_LIBRARIES}
)

