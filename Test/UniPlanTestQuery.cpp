#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

// ===================================================================
// topic list
// ===================================================================

TEST_F(FBundleTestFixture, TopicListJsonReturnsTopics)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"list", "--repo-root", mRepoRoot.string()}, mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"], "uni-plan-topic-list-v1");
    EXPECT_EQ(Json["count"], 1);
    EXPECT_EQ(Json["topics"][0]["topic"], "SampleTopic");
}

TEST_F(FBundleTestFixture, TopicListEmptyRepo)
{
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"list", "--repo-root", mRepoRoot.string()}, mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"], 0);
}

// ===================================================================
// topic get
// ===================================================================

TEST_F(FBundleTestFixture, TopicGetJsonHasFields)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"get", "--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"], "uni-plan-topic-get-v1");
    EXPECT_EQ(Json["topic"], "SampleTopic");
    EXPECT_EQ(Json["status"], "in_progress");
    EXPECT_TRUE(Json.contains("phase_summary"));
    EXPECT_EQ(Json["phase_count"], 3);
    EXPECT_EQ(Json["title"], "Sample Topic for Testing and Reference");
}

TEST_F(FBundleTestFixture, TopicGetEmitsAllMetadataKeys)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"get", "--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    // All 12 metadata fields must be emitted (null or populated)
    for (const char *Key :
         {"summary", "goals", "non_goals", "risks", "acceptance_criteria",
          "problem_statement", "validation_commands", "baseline_audit",
          "execution_strategy", "locked_decisions", "source_references",
          "dependencies"})
    {
        EXPECT_TRUE(Json.contains(Key)) << "topic get missing key: " << Key;
    }
}

// v0.84.0: --sections <csv> filters top-level sections. Identity fields
// (topic/status/title/phase_count + schema envelope) remain; unrequested
// sections are absent from the output. Unknown names throw UsageError.
TEST_F(FBundleTestFixture, TopicGetSectionsFiltersJsonOutput)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"get", "--topic", "SampleTopic", "--sections", "summary,phases",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    // Requested sections present.
    EXPECT_TRUE(Json.contains("summary"));
    EXPECT_TRUE(Json.contains("phase_summary"));
    // Identity fields always present.
    EXPECT_TRUE(Json.contains("topic"));
    EXPECT_TRUE(Json.contains("status"));
    EXPECT_TRUE(Json.contains("title"));
    EXPECT_TRUE(Json.contains("phase_count"));
    // Filtered-out sections absent.
    for (const char *Absent :
         {"goals", "non_goals", "risks", "acceptance_criteria",
          "problem_statement", "validation_commands", "baseline_audit",
          "execution_strategy", "locked_decisions", "source_references",
          "dependencies", "next_actions"})
    {
        EXPECT_FALSE(Json.contains(Absent))
            << "section not requested but emitted: " << Absent;
    }
}

TEST_F(FBundleTestFixture, TopicGetNoSectionsFlagEmitsAll)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"get", "--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    // Backward-compat: absent --sections == emit all (same as
    // TopicGetEmitsAllMetadataKeys above, doubled up here as a gate).
    for (const char *Key :
         {"summary", "goals", "non_goals", "risks", "acceptance_criteria",
          "problem_statement", "validation_commands", "baseline_audit",
          "execution_strategy", "locked_decisions", "source_references",
          "dependencies", "phase_summary"})
    {
        EXPECT_TRUE(Json.contains(Key))
            << "default topic get missing key: " << Key;
    }
}

TEST(OptionParsing, TopicGetSectionsRejectsUnknownName)
{
    EXPECT_THROW(UniPlan::ParseTopicGetOptions(
                     {"--topic", "T", "--sections", "summary,bogus"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, TopicGetSectionsRejectsEmptyField)
{
    EXPECT_THROW(UniPlan::ParseTopicGetOptions(
                     {"--topic", "T", "--sections", "summary,,phases"}),
                 UniPlan::UsageError);
}

TEST_F(FBundleTestFixture, TopicGetMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"get", "--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// topic status
// ===================================================================

TEST_F(FBundleTestFixture, TopicStatusJsonHasCounts)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"status", "--repo-root", mRepoRoot.string()}, mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["total"], 1);
    EXPECT_TRUE(Json.contains("counts"));
    EXPECT_EQ(Json["counts"]["in_progress"], 1);
}

TEST_F(FBundleTestFixture, TopicStatusEmptyRepoReturnsZero)
{
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"status", "--repo-root", mRepoRoot.string()}, mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["total"], 0);
}

// ===================================================================
// phase list
// ===================================================================

TEST_F(FBundleTestFixture, PhaseListJsonReturnsPhases)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"list", "--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"], 3);
    EXPECT_TRUE(Json.contains("phases"));
}

TEST_F(FBundleTestFixture, PhaseListMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"list", "--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// phase get
// ===================================================================

TEST_F(FBundleTestFixture, PhaseGetJsonHasFields)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "SampleTopic", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["phase_index"], 0);
    EXPECT_EQ(Json["status"], "completed");
}

TEST_F(FBundleTestFixture, PhaseGetEmitsAllDesignMaterialKeys)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "SampleTopic", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    // All 11 design material keys must be emitted
    for (const char *Key :
         {"scope", "output", "investigation", "code_entity_contract",
          "code_snippets", "best_practices", "multi_platforming",
          "readiness_gate", "handoff", "validation_commands", "dependencies"})
    {
        EXPECT_TRUE(Json.contains(Key)) << "phase get missing key: " << Key;
    }
}

TEST_F(FBundleTestFixture, PhaseGetDesignEmitsMultiPlatforming)
{
    // --design (renamed from --reference in v0.83.0) emits exactly the
    // fields that contribute to `design_chars`, including
    // `multi_platforming` from FPhaseDesignMaterial.
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "SampleTopic", "--phase", "0", "--design",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json.contains("multi_platforming"));
    // v0.83.0 realignment: --design includes `output` (prev. absent)
    // and excludes `dependencies` / `validation_commands` (moved to
    // --execution since they are structural, not prose).
    EXPECT_TRUE(Json.contains("output"));
    EXPECT_FALSE(Json.contains("dependencies"));
    EXPECT_FALSE(Json.contains("validation_commands"));
}

// Regression guard: every populated phase field must surface in --human
// output. Prevents renderer drift where a new field is added to
// FPhaseRecord and wired into JSON but silently missed by the human
// printer.
TEST_F(FBundleTestFixture, PhaseGetHumanRendersAllPopulatedSections)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    UniPlan::FPhaseRecord &Phase = Bundle.mPhases[0];
    Phase.mScope = "Scope text";
    Phase.mOutput = "Output text";
    Phase.mLifecycle.mDone = "Done text";
    Phase.mLifecycle.mRemaining = "Remaining text";
    Phase.mLifecycle.mBlockers = "Blockers text";
    Phase.mLifecycle.mAgentContext = "Agent context text";
    Phase.mDesign.mReadinessGate = "Readiness gate text";
    Phase.mDesign.mHandoff = "Handoff text";
    Phase.mDesign.mMultiPlatforming = "Multi-platforming text";
    Phase.mDesign.mInvestigation = "Investigation text";
    Phase.mDesign.mCodeEntityContract = "Code entity contract text";
    Phase.mDesign.mCodeSnippets = "Code snippets text";
    Phase.mDesign.mBestPractices = "Best practices text";
    UniPlan::FFileManifestItem FM;
    FM.mFilePath = "Source/Foo.cpp";
    FM.mAction = UniPlan::EFileAction::Create;
    FM.mDescription = "Create Foo";
    Phase.mFileManifest = {FM};
    UniPlan::FTestingRecord TR;
    TR.mActor = UniPlan::ETestingActor::AI;
    TR.mSession = "S";
    TR.mStep = "build";
    TR.mAction = "cmake build";
    TR.mExpected = "0 errors";
    Phase.mTesting = {TR};
    const fs::path BundlePath =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string WriteError;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, BundlePath, WriteError))
        << WriteError;

    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "SampleTopic", "--phase", "0", "--human",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    for (const char *Heading :
         {"Scope", "Output", "Done", "Remaining", "Blockers", "Agent Context",
          "Readiness Gate", "Handoff", "Multi Platforming", "Investigation",
          "Code Entity Contract", "Code Snippets", "Best Practices",
          "File Manifest", "Testing"})
    {
        EXPECT_NE(mCapturedStdout.find(Heading), std::string::npos)
            << "--human phase get missing heading: " << Heading;
    }
}

TEST_F(FBundleTestFixture, PhaseGetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "SampleTopic", "--phase", "99", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// phase next
// ===================================================================

TEST_F(FBundleTestFixture, PhaseNextFindsNotStarted)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"next", "--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["phase_index"], 2);
}

TEST_F(FBundleTestFixture, PhaseNextAllStartedReturnsNegative)
{
    CreateMinimalFixture("AllStarted", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::InProgress);
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"next", "--topic", "AllStarted", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["phase_index"], -1);
}

// ===================================================================
// phase readiness
// ===================================================================

TEST_F(FBundleTestFixture, PhaseReadinessReportsGates)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"readiness", "--topic", "SampleTopic", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json.contains("gates"));
    EXPECT_FALSE(Json["gates"].empty());
}

// ===================================================================
// phase wave-status
// ===================================================================

TEST_F(FBundleTestFixture, PhaseWaveStatusReportsWaves)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"wave-status", "--topic", "SampleTopic", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json.contains("waves"));
    EXPECT_TRUE(Json.contains("current_wave"));
}

TEST_F(FBundleTestFixture, PhaseWaveStatusOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"wave-status", "--topic", "SampleTopic", "--phase", "99",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// phase get --phases <csv> batch mode (v0.84.0)
// ===================================================================

TEST_F(FBundleTestFixture, PhaseGetBatchReturnsV2WrappedArray)
{
    CreateMinimalFixture("BatchT", UniPlan::ETopicStatus::InProgress, 4,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "BatchT", "--phases", "0,2,3", "--brief",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"].get<std::string>(), "uni-plan-phase-get-v2");
    EXPECT_EQ(Json["topic"].get<std::string>(), "BatchT");
    ASSERT_TRUE(Json.contains("phases"));
    ASSERT_EQ(Json["phases"].size(), 3u);
    EXPECT_EQ(Json["phase_count"].get<size_t>(), 3u);
    EXPECT_EQ(Json["phases"][0]["phase_index"].get<int>(), 0);
    EXPECT_EQ(Json["phases"][1]["phase_index"].get<int>(), 2);
    EXPECT_EQ(Json["phases"][2]["phase_index"].get<int>(), 3);
    for (const auto &P : Json["phases"])
    {
        EXPECT_TRUE(P.contains("status"));
        EXPECT_TRUE(P.contains("design_chars"));
        EXPECT_TRUE(P.contains("scope"));
    }
}

TEST_F(FBundleTestFixture, PhaseGetBatchDedupesAndSortsIndices)
{
    CreateMinimalFixture("BatchDedup", UniPlan::ETopicStatus::InProgress, 5,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "BatchDedup", "--phases", "3,1,3,0,1", "--brief",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_EQ(Json["phases"].size(), 3u);
    EXPECT_EQ(Json["phases"][0]["phase_index"].get<int>(), 0);
    EXPECT_EQ(Json["phases"][1]["phase_index"].get<int>(), 1);
    EXPECT_EQ(Json["phases"][2]["phase_index"].get<int>(), 3);
}

TEST_F(FBundleTestFixture, PhaseGetBatchOutOfRangeFails)
{
    CreateMinimalFixture("BatchRange", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "BatchRange", "--phases", "0,99", "--brief",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, PhaseGetSinglePhaseKeepsV1Schema)
{
    CreateMinimalFixture("BatchV1", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "BatchV1", "--phase", "0", "--brief", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    // Single-phase mode preserves v1 flat schema for backward compat.
    EXPECT_EQ(Json["schema"].get<std::string>(), "uni-plan-phase-get-v1");
    EXPECT_FALSE(Json.contains("phases"));
    EXPECT_EQ(Json["phase_index"].get<int>(), 0);
}

TEST(OptionParsing, PhaseGetMutualExclusionPhaseAndPhases)
{
    EXPECT_THROW(UniPlan::ParsePhaseGetOptions(
                     {"--topic", "T", "--phase", "1", "--phases", "1,2"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, PhaseGetRejectsNonNumericPhasesCsv)
{
    EXPECT_THROW(
        UniPlan::ParsePhaseGetOptions({"--topic", "T", "--phases", "1,abc"}),
        UniPlan::UsageError);
    EXPECT_THROW(
        UniPlan::ParsePhaseGetOptions({"--topic", "T", "--phases", "1,,3"}),
        UniPlan::UsageError);
}

// ===================================================================
// phase drift (v0.84.0)
// ===================================================================

TEST_F(FBundleTestFixture, PhaseDriftDetectsStatusLagLane)
{
    CreateMinimalFixture("DriftLane", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("DriftLane", Bundle));
    // Phase status = not_started, but lane 0 is completed.
    UniPlan::FLaneRecord L;
    L.mStatus = UniPlan::EExecutionStatus::Completed;
    L.mScope = "Implemented sub-task A";
    Bundle.mPhases[0].mLanes.push_back(L);
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "DriftLane.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"drift", "--topic", "DriftLane", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_EQ(Json["drift_count"].get<size_t>(), 1u);
    const auto &E = Json["drift_entries"][0];
    EXPECT_EQ(E["topic"].get<std::string>(), "DriftLane");
    EXPECT_EQ(E["phase_index"].get<int>(), 0);
    EXPECT_EQ(E["kind"].get<std::string>(), "status_lag_lane");
    EXPECT_EQ(E["phase_status"].get<std::string>(), "not_started");
    ASSERT_EQ(E["evidence"]["lane_indices"].size(), 1u);
    EXPECT_EQ(E["evidence"]["lane_indices"][0].get<int>(), 0);
}

TEST_F(FBundleTestFixture, PhaseDriftDetectsStatusLagDone)
{
    CreateMinimalFixture("DriftDone", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("DriftDone", Bundle));
    // Phase status = not_started but `done` carries substantive prose
    // that is not a placeholder literal.
    Bundle.mPhases[0].mLifecycle.mDone =
        "Implemented subcommand dispatcher in Main.cpp and verified on both "
        "platforms; 224/224 tests pass.";
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "DriftDone.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"drift", "--topic", "DriftDone", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    bool bFoundDone = false;
    for (const auto &E : Json["drift_entries"])
    {
        if (E["kind"].get<std::string>() == "status_lag_done")
        {
            bFoundDone = true;
            EXPECT_GT(E["evidence"]["done_chars"].get<int>(), 10);
        }
    }
    EXPECT_TRUE(bFoundDone) << "substantive done prose should emit "
                               "status_lag_done";
}

TEST_F(FBundleTestFixture, PhaseDriftIgnoresPlaceholderDone)
{
    CreateMinimalFixture("DriftPlaceholder",
                         UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("DriftPlaceholder", Bundle));
    // "Not started" is a status-word placeholder — should not drift.
    Bundle.mPhases[0].mLifecycle.mDone = "Not started";
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "DriftPlaceholder.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"drift", "--topic", "DriftPlaceholder", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    for (const auto &E : Json["drift_entries"])
    {
        EXPECT_NE(E["kind"].get<std::string>(), "status_lag_done")
            << "placeholder `Not started` must not fire status_lag_done";
    }
}

TEST_F(FBundleTestFixture, PhaseDriftDetectsCompletionLagLane)
{
    CreateMinimalFixture("DriftCompletion", UniPlan::ETopicStatus::InProgress,
                         1, UniPlan::EExecutionStatus::Completed, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("DriftCompletion", Bundle));
    // Phase is completed, but lane 0 is still not_started.
    UniPlan::FLaneRecord L0;
    L0.mStatus = UniPlan::EExecutionStatus::NotStarted;
    L0.mScope = "Verify Windows parity";
    UniPlan::FLaneRecord L1;
    L1.mStatus = UniPlan::EExecutionStatus::Completed;
    L1.mScope = "Verify macOS parity";
    Bundle.mPhases[0].mLanes = {L0, L1};
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "DriftCompletion.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"drift", "--topic", "DriftCompletion", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    bool bFound = false;
    for (const auto &E : Json["drift_entries"])
    {
        if (E["kind"].get<std::string>() == "completion_lag_lane")
        {
            bFound = true;
            ASSERT_EQ(E["evidence"]["lane_indices"].size(), 1u);
            EXPECT_EQ(E["evidence"]["lane_indices"][0].get<int>(), 0);
        }
    }
    EXPECT_TRUE(bFound);
}

TEST_F(FBundleTestFixture, PhaseDriftAllClean)
{
    CreateMinimalFixture("Clean", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"drift", "--topic", "Clean", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["drift_count"].get<size_t>(), 0u);
    EXPECT_EQ(Json["topic_count_scanned"].get<size_t>(), 1u);
}

TEST_F(FBundleTestFixture, PhaseDriftScansAllTopicsWhenTopicOmitted)
{
    CreateMinimalFixture("CleanA", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    CreateMinimalFixture("DriftB", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("DriftB", Bundle));
    UniPlan::FLaneRecord L;
    L.mStatus = UniPlan::EExecutionStatus::Completed;
    L.mScope = "Finished early";
    Bundle.mPhases[0].mLanes.push_back(L);
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "DriftB.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"drift", "--repo-root", mRepoRoot.string()}, mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["topic_count_scanned"].get<size_t>(), 2u);
    EXPECT_EQ(Json["drift_count"].get<size_t>(), 1u);
    EXPECT_EQ(Json["drift_entries"][0]["topic"].get<std::string>(), "DriftB");
}

TEST_F(FBundleTestFixture, PhaseReadinessOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"readiness", "--topic", "SampleTopic", "--phase", "99", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// changelog
// ===================================================================

TEST_F(FBundleTestFixture, ChangelogJsonReturnsEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleChangelogCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_GT(Json["count"].get<int>(), 0);
}

TEST_F(FBundleTestFixture, ChangelogMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunBundleChangelogCommand(
        {"--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// verification
// ===================================================================

TEST_F(FBundleTestFixture, VerificationJsonReturnsEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleVerificationCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_GT(Json["count"].get<int>(), 0);
}

TEST_F(FBundleTestFixture, VerificationMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunBundleVerificationCommand(
        {"--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// timeline
// ===================================================================

TEST_F(FBundleTestFixture, TimelineJsonReturnsMergedEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleTimelineCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_GT(Json["count"].get<int>(), 0);
}

TEST_F(FBundleTestFixture, TimelineMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunBundleTimelineCommand(
        {"--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// blockers
// ===================================================================

TEST_F(FBundleTestFixture, BlockersJsonReturnsResults)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleBlockersCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
}

TEST_F(FBundleTestFixture, BlockersMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunBundleBlockersCommand(
        {"--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// CollectBundleBlockers — single source of truth for blocker detection
// (shared by the `uni-plan blockers` CLI command and the watch snapshot).
// A phase is a blocker when status==Blocked, when blocker text is
// non-placeholder, or both. The returned Kind field records which
// branch fired.

TEST_F(FBundleTestFixture, CollectBundleBlockersPhaseStatusBlockedIsDetected)
{
    UniPlan::FTopicBundle Bundle;
    Bundle.mTopicKey = "T";
    Bundle.mPhases.emplace_back();
    Bundle.mPhases[0].mLifecycle.mStatus = UniPlan::EExecutionStatus::Blocked;
    Bundle.mPhases[0].mLifecycle.mBlockers = "";

    const auto Blockers = UniPlan::CollectBundleBlockers(Bundle);
    ASSERT_EQ(Blockers.size(), 1u);
    EXPECT_EQ(Blockers[0].mPhaseIndex, 0);
    EXPECT_EQ(Blockers[0].mKind, "status");
    EXPECT_EQ(Blockers[0].mTopicKey, "T");
}

TEST_F(FBundleTestFixture, CollectBundleBlockersBlockerTextIsDetected)
{
    UniPlan::FTopicBundle Bundle;
    Bundle.mTopicKey = "T";
    Bundle.mPhases.emplace_back();
    Bundle.mPhases[0].mLifecycle.mStatus =
        UniPlan::EExecutionStatus::InProgress;
    Bundle.mPhases[0].mLifecycle.mBlockers = "Waiting on dependency X";

    const auto Blockers = UniPlan::CollectBundleBlockers(Bundle);
    ASSERT_EQ(Blockers.size(), 1u);
    EXPECT_EQ(Blockers[0].mKind, "text");
    EXPECT_EQ(Blockers[0].mAction, "Waiting on dependency X");
}

TEST_F(FBundleTestFixture, CollectBundleBlockersBothStatusAndTextTagsBoth)
{
    UniPlan::FTopicBundle Bundle;
    Bundle.mTopicKey = "T";
    Bundle.mPhases.emplace_back();
    Bundle.mPhases[0].mLifecycle.mStatus = UniPlan::EExecutionStatus::Blocked;
    Bundle.mPhases[0].mLifecycle.mBlockers = "Waiting on dep";

    const auto Blockers = UniPlan::CollectBundleBlockers(Bundle);
    ASSERT_EQ(Blockers.size(), 1u);
    EXPECT_EQ(Blockers[0].mKind, "status+text");
}

TEST_F(FBundleTestFixture, CollectBundleBlockersPlaceholderTextIsIgnored)
{
    UniPlan::FTopicBundle Bundle;
    Bundle.mTopicKey = "T";
    for (const std::string &Placeholder :
         {"", "None", "none.", "N/A", "n/a", "-", "NONE"})
    {
        UniPlan::FPhaseRecord Phase;
        Phase.mLifecycle.mStatus = UniPlan::EExecutionStatus::InProgress;
        Phase.mLifecycle.mBlockers = Placeholder;
        Bundle.mPhases.push_back(std::move(Phase));
    }

    const auto Blockers = UniPlan::CollectBundleBlockers(Bundle);
    EXPECT_EQ(Blockers.size(), 0u);
}

TEST_F(FBundleTestFixture, CollectBundleBlockersPreservesPhaseIndex)
{
    UniPlan::FTopicBundle Bundle;
    Bundle.mTopicKey = "T";
    Bundle.mPhases.resize(5);
    Bundle.mPhases[3].mLifecycle.mStatus = UniPlan::EExecutionStatus::Blocked;

    const auto Blockers = UniPlan::CollectBundleBlockers(Bundle);
    ASSERT_EQ(Blockers.size(), 1u);
    EXPECT_EQ(Blockers[0].mPhaseIndex, 3);
}

// ===================================================================
// validate
// ===================================================================

TEST_F(FBundleTestFixture, ValidatePassesOnValidFixture)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
}

// v0.71.0: validate --json output must include a top-level `summary`
// section with per-topic + per-phase aggregate stats so that cross-topic
// audits don't require raw `.Plan.json` reads.
TEST_F(FBundleTestFixture, ValidateEmitsSummaryWithTopicCount)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("summary"));
    EXPECT_EQ(Json["summary"]["topic_count"].get<size_t>(), 1u);
    ASSERT_TRUE(Json["summary"].contains("topics"));
    ASSERT_EQ(Json["summary"]["topics"].size(), 1u);
    EXPECT_EQ(Json["summary"]["topics"][0]["topic"].get<std::string>(),
              "SampleTopic");
}

TEST_F(FBundleTestFixture, ValidateSummaryIncludesPhaseStats)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    const auto &Topic = Json["summary"]["topics"][0];
    ASSERT_TRUE(Topic.contains("status_distribution"));
    ASSERT_TRUE(Topic.contains("phases"));
    ASSERT_GT(Topic["phases"].size(), 0u);
    const auto &Phase0 = Topic["phases"][0];
    EXPECT_TRUE(Phase0.contains("index"));
    EXPECT_TRUE(Phase0.contains("status"));
    EXPECT_TRUE(Phase0.contains("scope_chars"));
    EXPECT_TRUE(Phase0.contains("design_chars"));
    EXPECT_TRUE(Phase0.contains("jobs_count"));
    EXPECT_TRUE(Phase0.contains("testing_count"));
    EXPECT_TRUE(Phase0.contains("file_manifest_count"));
    EXPECT_TRUE(Phase0.contains("file_manifest_missing"));
}

TEST_F(FBundleTestFixture, ValidateWarnsActivePhaseMissingActorCoverage)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FTestingRecord Record;
    Record.mSession = "S1";
    Record.mActor = UniPlan::ETestingActor::Human;
    Record.mStep = "1";
    Record.mAction = "Run smoke";
    Record.mExpected = "Pass";
    Bundle.mPhases[0].mTesting.push_back(std::move(Record));

    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["valid"].get<bool>());
    ASSERT_FALSE(Json["issues"].empty());
    bool bFoundActorCoverage = false;
    for (const auto &Issue : Json["issues"])
    {
        if (Issue["id"] == "testing_actor_coverage")
        {
            bFoundActorCoverage = true;
            break;
        }
    }
    EXPECT_TRUE(bFoundActorCoverage);
}

TEST_F(FBundleTestFixture, ValidateIgnoresCompletedPhaseActorCoverage)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::Completed, 1,
                         UniPlan::EExecutionStatus::Completed, true);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FTestingRecord Record;
    Record.mSession = "S1";
    Record.mActor = UniPlan::ETestingActor::Human;
    Record.mStep = "1";
    Record.mAction = "Run smoke";
    Record.mExpected = "Pass";
    Bundle.mPhases[0].mTesting.push_back(std::move(Record));

    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["issues"].empty());
}

TEST_F(FBundleTestFixture, ValidateMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, ValidateFlagsLegacySingularAffectedRef)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FChangeLogEntry Legacy;
    Legacy.mDate = "2026-04-01";
    Legacy.mChange = "Legacy-form drift";
    Legacy.mAffected =
        "phase[0].jobs[1]"; // singular legacy: canonical is phases[0]
    Bundle.mChangeLogs.push_back(std::move(Legacy));

    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    bool bFound = false;
    for (const auto &Issue : Json["issues"])
    {
        if (Issue["id"] == "canonical_entity_ref")
        {
            bFound = true;
            EXPECT_NE(Issue["detail"].get<std::string>().find("legacy"),
                      std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(bFound);
}

TEST_F(FBundleTestFixture, ValidateAcceptsCanonicalAffectedRefs)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Canonical singular forms + top-level targets — all must pass
    for (const std::string &A :
         {"phases[0]", "phases[0].jobs[1]", "phases[0].lanes[2]",
          "phases[0].jobs[1].tasks[3]", "phases[0].testing[0]",
          "phases[0].file_manifest[2]", "phases[0].status", "changelogs[0]",
          "verifications[1]", "plan", ""})
    {
        UniPlan::FChangeLogEntry E;
        E.mDate = "2026-04-01";
        E.mChange = "ok";
        E.mAffected = A;
        Bundle.mChangeLogs.push_back(E);
    }

    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    for (const auto &Issue : Json["issues"])
    {
        EXPECT_NE(Issue["id"], "canonical_entity_ref")
            << "unexpected canonical_entity_ref failure for: " << Issue["path"];
    }
}
