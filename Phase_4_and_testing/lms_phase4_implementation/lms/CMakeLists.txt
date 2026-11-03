cmake_minimum_required(VERSION 3.16)
project(ModeratedLMS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)       # used by clang-tidy / cppcheck

option(LMS_BUILD_TESTS "Build the automated tests" ON)

# ---- dependencies: SQLite 3 and libsodium (Argon2id) ---------------------------------
find_package(SQLite3 REQUIRED)

find_package(unofficial-sodium CONFIG QUIET)           # vcpkg on Windows
if(TARGET unofficial-sodium::sodium)
    set(LMS_SODIUM_TARGET unofficial-sodium::sodium)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SODIUM IMPORTED_TARGET libsodium)
    endif()
    if(TARGET PkgConfig::SODIUM)
        set(LMS_SODIUM_TARGET PkgConfig::SODIUM)
    else()
        find_path(SODIUM_INCLUDE_DIR sodium.h)
        find_library(SODIUM_LIBRARY NAMES sodium libsodium)
        if(NOT SODIUM_INCLUDE_DIR OR NOT SODIUM_LIBRARY)
            message(FATAL_ERROR "libsodium not found. Install libsodium-dev (Ubuntu) or 'vcpkg install libsodium' (Windows).")
        endif()
        add_library(lms_sodium INTERFACE)
        target_include_directories(lms_sodium INTERFACE ${SODIUM_INCLUDE_DIR})
        target_link_libraries(lms_sodium INTERFACE ${SODIUM_LIBRARY})
        set(LMS_SODIUM_TARGET lms_sodium)
    endif()
endif()

# ---- core library: everything except main() ------------------------------------------
add_library(lms_core STATIC
    src/common/TimeUtil.cpp
    src/domain/Types.cpp
    src/domain/User.cpp
    src/infra/LibsodiumHasher.cpp
    src/infra/ManagedFolderFileStore.cpp
    src/data/Database.cpp
    src/data/SchemaSql.cpp
    src/data/SqliteRepositories.cpp
    src/business/Penalty.cpp
    src/business/Lifecycle.cpp
    src/business/PermissionPolicy.cpp
    src/business/AccessScopeAndAudit.cpp
    src/business/AuthUserServices.cpp
    src/business/CourseAssignmentServices.cpp
    src/business/SubmissionService.cpp
    src/business/MarkingService.cpp
    src/business/ModerationService.cpp
    src/business/BackupAndReport.cpp
    src/presentation/InputReader.cpp
    src/presentation/Menu.cpp
    src/presentation/Commands.cpp
    src/presentation/ConsoleApp.cpp)
target_include_directories(lms_core PUBLIC src)
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 ${LMS_SODIUM_TARGET})
if(MSVC)
    target_compile_options(lms_core PUBLIC /W4 /permissive-)
else()
    target_compile_options(lms_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lms_core)

# ---- tests -----------------------------------------------------------------------------
if(LMS_BUILD_TESTS)
    enable_testing()
    add_executable(lms_tests tests/test_core.cpp)
    target_link_libraries(lms_tests PRIVATE lms_core)
    add_test(NAME core_tests COMMAND lms_tests)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_FOUND AND NOT WIN32)
        add_test(NAME console_end_to_end
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/e2e_console.py $<TARGET_FILE:lms>)
    endif()
endif()
