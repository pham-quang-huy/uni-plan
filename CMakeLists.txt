cmake_minimum_required(VERSION 3.20)
project(UniPlan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(UPLAN_WATCH "Build watch dashboard (requires FTXUI)" ON)
option(UPLAN_TESTS "Build unit tests (fetches Google Test)" OFF)

# ---------------------------------------------------------------------------
# FTXUI (terminal UI for watch mode)
# ---------------------------------------------------------------------------

if(UPLAN_WATCH)
    add_subdirectory(ThirdParty/FTXUI)
endif()

# ---------------------------------------------------------------------------
# uni-plan-lib — object library shared by executable and tests
# ---------------------------------------------------------------------------

add_library(uni-plan-lib OBJECT
    Source/UniPlanCommandDispatch.cpp
    Source/UniPlanCommandHelp.cpp
    Source/UniPlanRuntime.cpp
    Source/UniPlanParsing.cpp
    Source/UniPlanCache.cpp
    Source/UniPlanValidation.cpp
    Source/UniPlanValidationContent.cpp
    Source/UniPlanJSONLineIndex.cpp
    Source/UniPlanOutputJSON.cpp
    Source/UniPlanOutputText.cpp
    Source/UniPlanOutputHuman.cpp
    Source/UniPlanOptionParsing.cpp
    Source/UniPlanJSONIO.cpp
    Source/UniPlanBundleWriteGuard.cpp
    Source/UniPlanSchemaValidation.cpp
    Source/UniPlanCommandBundle.cpp
    Source/UniPlanCommandValidate.cpp
    Source/UniPlanCommandHistory.cpp
    Source/UniPlanCommandMutationCommon.cpp
    Source/UniPlanCommandMutation.cpp
    Source/UniPlanCommandLifecycle.cpp
    Source/UniPlanCommandEntity.cpp
    Source/UniPlanCommandEntityCrud.cpp
    Source/UniPlanCommandCatalog.cpp
    Source/UniPlanCommandPlanEntries.cpp
    Source/UniPlanCommandGraph.cpp
    Source/UniPlanCommandMigrate.cpp
    Source/UniPlanCommandSemanticQuery.cpp
    Source/UniPlanCommandTopic.cpp
    Source/UniPlanCommandPhase.cpp
    Source/UniPlanCommandLegacyGap.cpp
    Source/UniPlanLegacyDiscovery.cpp
    Source/UniPlanPhaseKind.cpp
)

target_include_directories(uni-plan-lib PUBLIC
    Source/
    ThirdParty/json/
)

if(UPLAN_WATCH)
    target_sources(uni-plan-lib PRIVATE
        Source/UniPlanWatchApp.cpp
        Source/UniPlanWatchPanels.cpp
        Source/UniPlanWatchSnapshot.cpp
    )
    target_link_libraries(uni-plan-lib PUBLIC
        ftxui::component ftxui::dom ftxui::screen)
    target_compile_definitions(uni-plan-lib PUBLIC UPLAN_WATCH=1)
endif()

# ---------------------------------------------------------------------------
# uni-plan executable
# ---------------------------------------------------------------------------

add_executable(uni-plan Source/Main.cpp)
target_link_libraries(uni-plan PRIVATE uni-plan-lib)

# ---------------------------------------------------------------------------
# Post-build: copy config and schemas next to binary
# ---------------------------------------------------------------------------

add_custom_command(TARGET uni-plan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/uni-plan.ini"
        "$<TARGET_FILE_DIR:uni-plan>/uni-plan.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Schemas"
        "$<TARGET_FILE_DIR:uni-plan>/Schemas"
)

# ---------------------------------------------------------------------------
# Unit tests (optional, fetches Google Test via FetchContent)
# ---------------------------------------------------------------------------

if(UPLAN_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest
        GIT_TAG        v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(uni-plan-tests
        Test/UniPlanTestMain.cpp
        Test/UniPlanTestFixture.cpp
        Test/UniPlanTestOptionParsing.cpp
        Test/UniPlanTestQuery.cpp
        Test/UniPlanTestMutation.cpp
        Test/UniPlanTestSemantic.cpp
        Test/UniPlanTestEvidence.cpp
        Test/UniPlanTestEntity.cpp
        Test/UniPlanTestValidationContent.cpp
        Test/UniPlanTestHelp.cpp
        Test/UniPlanTestPhaseKind.cpp
        Test/UniPlanTestGraph.cpp
        Test/UniPlanTestBundleWriteGuard.cpp
        Test/UniPlanTestEndToEnd.cpp
    )

    target_link_libraries(uni-plan-tests PRIVATE
        uni-plan-lib GTest::gtest GTest::gtest_main)

    target_compile_definitions(uni-plan-tests PRIVATE
        UPLAN_TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/Example"
        UPLAN_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    # Tests need schemas and ini for validate command
    add_custom_command(TARGET uni-plan-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/uni-plan.ini"
            "$<TARGET_FILE_DIR:uni-plan-tests>/uni-plan.ini"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Schemas"
            "$<TARGET_FILE_DIR:uni-plan-tests>/Schemas"
    )

    include(GoogleTest)
    gtest_discover_tests(uni-plan-tests)
endif()
