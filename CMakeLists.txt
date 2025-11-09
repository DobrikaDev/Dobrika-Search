cmake_minimum_required(VERSION 3.15)
project(DobrikaSearchEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
include(FetchContent)

# Xapian via pkg-config
pkg_check_modules(XAPIAN REQUIRED xapian-core)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}            # for generated protobuf headers
    ${Protobuf_INCLUDE_DIRS}
    ${XAPIAN_INCLUDE_DIRS}
)

# Protobuf generation
set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/DSResponse.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/DSRequest.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/DServer.proto
    
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Library with search layer
set(SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Components/xapian_processor/xapian_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/dse_tools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/config_generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/static.cpp
)

add_library(dobrika_search STATIC
    ${SRC_FILES}
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_link_libraries(dobrika_search
    PRIVATE
    ${Protobuf_LIBRARIES}
    ${XAPIAN_LIBRARIES}
    Threads::Threads
)

target_compile_options(dobrika_search PRIVATE -Wall -Wextra -Wpedantic)

########################################
# Tests
########################################
option(DOBRIKA_WITH_SERVER "Build Drogon-based HTTP server and HTTP tests" OFF)
if (DOBRIKA_WITH_SERVER)
  find_package(Drogon QUIET CONFIG)
  if (NOT Drogon_FOUND)
    # Fallback to pkg-config if CMake package is not available or not detected
    pkg_check_modules(DROGON QUIET drogon)
    if (DROGON_FOUND)
      add_library(Drogon::Drogon INTERFACE IMPORTED)
      set_target_properties(Drogon::Drogon PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${DROGON_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${DROGON_LIBRARIES}"
      )
      set(Drogon_FOUND TRUE)
      message(STATUS "Drogon found via pkg-config")
    endif()
  endif()
  # Normalize detection: prefer imported target presence
  if (TARGET Drogon::Drogon)
    set(HAVE_DROGON TRUE)
  elseif (Drogon_FOUND)
    set(HAVE_DROGON TRUE)
  else()
    set(HAVE_DROGON FALSE)
  endif()
  # Final fallback: manual search for headers and library
  if (NOT HAVE_DROGON)
    find_path(DROGON_MANUAL_INCLUDE_DIR drogon/drogon.h
      PATHS /usr/include /usr/local/include
    )
    find_library(DROGON_MANUAL_LIBRARY drogon
      PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
    )
    if (DROGON_MANUAL_INCLUDE_DIR AND DROGON_MANUAL_LIBRARY)
      # Try to also discover JsonCpp and Trantor to satisfy Drogon's public headers
      set(EXTRA_DROGON_INCLUDES "")
      set(EXTRA_DROGON_LIBS "")
      pkg_check_modules(JSONCPP QUIET jsoncpp)
      if (JSONCPP_FOUND)
        list(APPEND EXTRA_DROGON_INCLUDES ${JSONCPP_INCLUDE_DIRS})
        list(APPEND EXTRA_DROGON_LIBS ${JSONCPP_LIBRARIES})
      else()
        find_path(JSONCPP_INCLUDE_DIR json/json.h
          PATH_SUFFIXES jsoncpp
          PATHS /usr/include /usr/local/include
        )
        find_library(JSONCPP_LIBRARY jsoncpp
          PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
        )
        if (JSONCPP_INCLUDE_DIR)
          list(APPEND EXTRA_DROGON_INCLUDES ${JSONCPP_INCLUDE_DIR})
        endif()
        if (JSONCPP_LIBRARY)
          list(APPEND EXTRA_DROGON_LIBS ${JSONCPP_LIBRARY})
        endif()
      endif()
      pkg_check_modules(TRANTOR QUIET trantor)
      if (TRANTOR_FOUND)
        list(APPEND EXTRA_DROGON_INCLUDES ${TRANTOR_INCLUDE_DIRS})
        list(APPEND EXTRA_DROGON_LIBS ${TRANTOR_LIBRARIES})
      else()
        find_path(TRANTOR_INCLUDE_DIR trantor/Logger.h
          PATHS /usr/include /usr/local/include
        )
        find_library(TRANTOR_LIBRARY trantor
          PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
        )
        if (TRANTOR_INCLUDE_DIR)
          list(APPEND EXTRA_DROGON_INCLUDES ${TRANTOR_INCLUDE_DIR})
        endif()
        if (TRANTOR_LIBRARY)
          list(APPEND EXTRA_DROGON_LIBS ${TRANTOR_LIBRARY})
        endif()
      endif()
      add_library(Drogon::Drogon INTERFACE IMPORTED)
      set_target_properties(Drogon::Drogon PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${DROGON_MANUAL_INCLUDE_DIR};${EXTRA_DROGON_INCLUDES}"
        INTERFACE_LINK_LIBRARIES "${DROGON_MANUAL_LIBRARY};${EXTRA_DROGON_LIBS}"
      )
      set(HAVE_DROGON TRUE)
      message(STATUS "Drogon found via manual search")
    endif()
  endif()
  if (HAVE_DROGON)
    message(STATUS "Drogon found: enabling web server and HTTP tests")
    add_library(dobrika_server STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/server/web_server.cpp
    )
    target_include_directories(dobrika_server
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_link_libraries(dobrika_server
        PRIVATE
        dobrika_search
        Drogon::Drogon
    )
    target_compile_options(dobrika_server PRIVATE -Wall -Wextra -Wpedantic)

    add_executable(dobrika_server_main
        ${CMAKE_CURRENT_SOURCE_DIR}/src/server/main.cpp
    )
    target_link_libraries(dobrika_server_main
        PRIVATE
        dobrika_server
    )
  else()
    message(WARNING "Drogon not found; web server targets will not be built")
  endif()
endif()
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

enable_testing()

add_executable(dse_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xapian_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_backup.cpp
)

target_link_libraries(dse_tests
    PRIVATE
    dobrika_search
    Catch2::Catch2WithMain
    ${Protobuf_LIBRARIES}
    ${XAPIAN_LIBRARIES}
    Threads::Threads
)

add_test(NAME dse_tests COMMAND dse_tests)

if (DOBRIKA_WITH_SERVER AND HAVE_DROGON)
    add_executable(dse_http_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_http_server.cpp
    )
    target_link_libraries(dse_http_tests
        PRIVATE
        dobrika_server
        Catch2::Catch2WithMain
    )
    add_test(NAME dse_http_tests COMMAND dse_http_tests)
endif()
