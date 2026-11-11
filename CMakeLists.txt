cmake_minimum_required(VERSION 3.20)
project(ADSProject VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # helps editors (clangd / VS Code) find headers

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# -----------------------------------------------------------------------------
# Third-party dependencies (downloaded automatically, nothing to install)
#   - Crow   : HTTP / REST framework (header-only)
#   - Asio   : networking library Crow is built on (header-only, standalone)
#   - SQLite : database engine, compiled from the official amalgamation
# SOURCE_SUBDIR points to a folder that doesn't exist so CMake only downloads the
# sources and we define the targets ourselves below (simpler & portable).
# -----------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-30-2
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  _none_)

FetchContent_Declare(crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG        v1.2.0
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  _none_)

FetchContent_Declare(sqlite3
  URL            https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR  _none_)

FetchContent_MakeAvailable(asio crow sqlite3)

find_package(Threads REQUIRED)

# Crow + Asio (header-only)
add_library(crow_lib INTERFACE)
target_include_directories(crow_lib SYSTEM INTERFACE
  ${crow_SOURCE_DIR}/include
  ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(crow_lib INTERFACE ASIO_STANDALONE)
target_link_libraries(crow_lib INTERFACE Threads::Threads)

# SQLite (C library)
add_library(sqlite3_lib STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_lib SYSTEM PUBLIC ${sqlite3_SOURCE_DIR})
target_link_libraries(sqlite3_lib PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

# -----------------------------------------------------------------------------
# Our code
# -----------------------------------------------------------------------------
# Common warning flags for our own targets only (not third-party code)
add_library(ads_warnings INTERFACE)
if(MSVC)
  target_compile_options(ads_warnings INTERFACE /W4)
else()
  target_compile_options(ads_warnings INTERFACE -Wall -Wextra)
endif()

# ads_core: data structures (include/) + domain code (src/).
# NO STL containers allowed here. When src/ gets .cpp files, add them below
# (or switch to file(GLOB_RECURSE ...)) and turn this into a STATIC library.
file(GLOB_RECURSE ADS_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
if(ADS_CORE_SOURCES)
  add_library(ads_core STATIC ${ADS_CORE_SOURCES})
  target_include_directories(ads_core PUBLIC include src)
  target_link_libraries(ads_core PUBLIC sqlite3_lib PRIVATE ads_warnings)
else()
  add_library(ads_core INTERFACE)
  target_include_directories(ads_core INTERFACE include src)
  target_link_libraries(ads_core INTERFACE sqlite3_lib)
endif()

# ads_server: the REST API (Crow lives only in server/)
file(GLOB_RECURSE ADS_SERVER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/server/*.cpp)
add_executable(ads_server ${ADS_SERVER_SOURCES})
target_link_libraries(ads_server PRIVATE ads_core crow_lib ads_warnings)
target_compile_definitions(ads_server PRIVATE
  ADS_DEFAULT_DB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/db/app.db")

# ads_demo: the original console demo of the data structures
add_executable(ads_demo main.cpp)
target_link_libraries(ads_demo PRIVATE ads_core ads_warnings)

# -----------------------------------------------------------------------------
# Tests (run with: ctest --test-dir build --output-on-failure)
# -----------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
  file(GLOB ADS_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  foreach(test_src ${ADS_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE ads_core ads_warnings)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()

  # Fails if any STL container header is included in include/ or src/
  add_test(NAME no_stl_containers
    COMMAND ${CMAKE_COMMAND}
      -DSCAN_DIRS=${CMAKE_CURRENT_SOURCE_DIR}/include$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/src
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/no_stl_containers.cmake)
endif()
