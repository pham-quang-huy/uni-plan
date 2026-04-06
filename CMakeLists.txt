cmake_minimum_required(VERSION 3.20)
project(UniPlan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(UPLAN_WATCH "Build watch dashboard (requires FTXUI)" ON)

# ---------------------------------------------------------------------------
# FTXUI (terminal UI for watch mode)
# ---------------------------------------------------------------------------

if(UPLAN_WATCH)
    add_subdirectory(ThirdParty/FTXUI)
endif()

# ---------------------------------------------------------------------------
# uni-plan executable
# ---------------------------------------------------------------------------

add_executable(uni-plan
    Source/Main.cpp
    Source/UniPlanRuntime.cpp
    Source/UniPlanParsing.cpp
    Source/UniPlanCache.cpp
    Source/UniPlanValidation.cpp
    Source/UniPlanOutputJson.cpp
    Source/UniPlanOutputText.cpp
    Source/UniPlanOutputHuman.cpp
    Source/UniPlanAnalysis.cpp
    Source/UniPlanOptionParsing.cpp
)

target_include_directories(uni-plan PRIVATE Source/)

# ---------------------------------------------------------------------------
# Watch mode (conditional)
# ---------------------------------------------------------------------------

if(UPLAN_WATCH)
    target_sources(uni-plan PRIVATE
        Source/UniPlanWatchApp.cpp
        Source/UniPlanWatchPanels.cpp
        Source/UniPlanWatchSnapshot.cpp
    )
    target_link_libraries(uni-plan PRIVATE ftxui::component ftxui::dom ftxui::screen)
    target_compile_definitions(uni-plan PRIVATE UPLAN_WATCH=1)
endif()

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
