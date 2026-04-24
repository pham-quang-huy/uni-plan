#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanTypes.h"
#include "UniPlanWatchSnapshot.h"

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
// phase metric
// ===================================================================

TEST_F(FBundleTestFixture, PhaseMetricJsonReturnsMetrics)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"metric", "--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"], "uni-plan-phase-metric-v1");
    EXPECT_EQ(Json["topic"], "SampleTopic");
    EXPECT_EQ(Json["status_filter"], "all");
    EXPECT_EQ(Json["count"], 3);
    ASSERT_TRUE(Json.contains("thresholds"));
    EXPECT_EQ(Json["thresholds"]["field_coverage_percent"]["rich"], 100);
    EXPECT_EQ(Json["thresholds"]["work_items"]["hollow"], 10);
    EXPECT_EQ(Json["thresholds"]["work_items"]["rich"], 40);
    EXPECT_EQ(Json["thresholds"]["testing_records"]["hollow"], 2);
    EXPECT_EQ(Json["thresholds"]["testing_records"]["rich"], 8);
    ASSERT_TRUE(Json.contains("phases"));
    ASSERT_TRUE(Json["phases"][0].contains("solid_words"));
    ASSERT_TRUE(Json["phases"][0].contains("recursive_words"));
    ASSERT_TRUE(Json["phases"][0].contains("field_coverage_percent"));
    ASSERT_TRUE(Json["phases"][0].contains("evidence_items"));
}

TEST_F(FBundleTestFixture, PhaseMetricSinglePhaseSelector)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"metric", "--topic", "SampleTopic", "--phase", "1", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"], 1);
    EXPECT_EQ(Json["phases"][0]["index"], 1);
}

TEST_F(FBundleTestFixture, PhaseMetricBatchSelectorSortsAndDedupes)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"metric", "--topic", "SampleTopic", "--phases", "2,0,2", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"], 2);
    EXPECT_EQ(Json["phases"][0]["index"], 0);
    EXPECT_EQ(Json["phases"][1]["index"], 2);
}

TEST_F(FBundleTestFixture, PhaseMetricStatusFilter)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"metric", "--topic", "SampleTopic", "--status", "in_progress",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["status_filter"], "in_progress");
    ASSERT_EQ(Json["count"], 1);
    EXPECT_EQ(Json["phases"][0]["status"], "in_progress");
}

TEST_F(FBundleTestFixture, PhaseMetricHumanRendersTable)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"metric", "--topic", "SampleTopic", "--phase", "0", "--human",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    EXPECT_NE(mCapturedStdout.find("Phase Metrics"), std::string::npos);
    EXPECT_NE(mCapturedStdout.find("SOLID"), std::string::npos);
    EXPECT_NE(mCapturedStdout.find("Evidence"), std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseMetricMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"metric", "--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, PhaseMetricOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"metric", "--topic", "SampleTopic", "--phase", "9", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("out of range"), std::string::npos);
}

#if defined(UPLAN_WATCH)
TEST_F(FBundleTestFixture, PhaseMetricWatchSnapshotCarriesMetrics)
{
    CopyFixture("SampleTopic");
    const UniPlan::FDocWatchSnapshot Snapshot =
        UniPlan::BuildWatchSnapshot(mRepoRoot.string(), false, "", false);

    ASSERT_EQ(Snapshot.mActivePlans.size(), 1);
    const UniPlan::FWatchPlanSummary &Plan = Snapshot.mActivePlans[0];
    ASSERT_FALSE(Plan.mPhases.empty());
    EXPECT_EQ(Plan.mPhases[0].mV4DesignChars,
              Plan.mPhases[0].mMetrics.mDesignChars);
    EXPECT_GT(Plan.mPhases[0].mMetrics.mRecursiveWordCount, 0);
}
#endif

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
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "DriftLane.Plan.json";
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
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "DriftDone.Plan.json";
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
    CreateMinimalFixture("DriftPlaceholder", UniPlan::ETopicStatus::InProgress,
                         1, UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("DriftPlaceholder", Bundle));
    // "Not started" is a status-word placeholder — should not drift.
    Bundle.mPhases[0].mLifecycle.mDone = "Not started";
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "DriftPlaceholder.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code =
        UniPlan::RunBundlePhaseCommand({"drift", "--topic", "DriftPlaceholder",
                                        "--repo-root", mRepoRoot.string()},
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
    const int Code =
        UniPlan::RunBundlePhaseCommand({"drift", "--topic", "DriftCompletion",
                                        "--repo-root", mRepoRoot.string()},
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
    // v0.86.0: opt out of file_manifest_required_for_code_phases — the
    // minimal fixture's populated code_entity_contract makes the phase
    // code-bearing in that evaluator's eyes, but this test is scoped to
    // actor-coverage behavior, not manifest authoring discipline.
    Bundle.mPhases[0].mbNoFileManifest = true;
    Bundle.mPhases[0].mFileManifestSkipReason =
        "Test fixture: no manifest authoring required for actor-coverage "
        "regression test";

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

// ===================================================================
// Stable `index` field on changelog / verification / typed-array queries
// (v0.95.0+ index-drift regression guards)
//
// The class-of-bug these cover: query commands sort or filter before
// emitting, while `set --index N` / `remove --index N` mutations
// target the raw storage index in the bundle's underlying vector. If
// the query output doesn't expose the storage index, an operator /
// agent reading the query and citing --index N addresses the wrong
// row. These tests lock in the v0.95.0 contract: every filter or
// sorted render emits a stable `index` field equal to the storage
// position, and a mutation against that index targets the exact row
// that appeared in the query output.
// ===================================================================

TEST_F(FBundleTestFixture, ChangelogQueryEmitsStableIndexAcrossSort)
{
    // SampleTopic fixture has mixed-phase changelog entries whose
    // storage order differs from the sorted render order. Capture the
    // JSON, then re-target each emitted row via `changelog set --index
    // <captured>` and verify the mutation lands on the same row the
    // query claimed.
    CopyFixture("SampleTopic");
    StartCapture();
    ASSERT_EQ(UniPlan::RunBundleChangelogCommand(
                  {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("entries"));
    ASSERT_GT(Json["entries"].size(), 0u);

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));

    // Every emitted entry must carry an `index` field that points at
    // the same underlying row the renderer showed.
    for (const auto &Entry : Json["entries"])
    {
        ASSERT_TRUE(Entry.contains("index"))
            << "changelog entry missing 'index' field";
        const size_t Idx = Entry["index"].get<size_t>();
        ASSERT_LT(Idx, Before.mChangeLogs.size());
        EXPECT_EQ(Before.mChangeLogs[Idx].mChange,
                  Entry["change"].get<std::string>())
            << "index " << Idx << " should map to the same `change` row "
            << "the renderer emitted";
    }

    // Round-trip: pick the FIRST emitted entry, mutate via
    // `changelog set --index <its emitted index>`, assert the mutation
    // landed on exactly that row.
    const size_t TargetIdx = Json["entries"][0]["index"].get<size_t>();
    const std::string OriginalChange = Before.mChangeLogs[TargetIdx].mChange;
    const std::string NewChange = "REINDEXED-" + OriginalChange;
    StartCapture();
    const int SetCode = UniPlan::RunChangelogSetCommand(
        {"--topic", "SampleTopic", "--index", std::to_string(TargetIdx),
         "--change", NewChange, "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(SetCode, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mChangeLogs[TargetIdx].mChange, NewChange)
        << "mutation must land on the row the query emitted at that index";
}

TEST_F(FBundleTestFixture, ChangelogQueryEmitsStableIndexUnderPhaseFilter)
{
    // Phase filter narrows the result set; remaining rows MUST still
    // expose their original storage index, not a 0..N-1 filtered
    // position, or `changelog set --index <N>` after filtering targets
    // the wrong row.
    CopyFixture("SampleTopic");
    StartCapture();
    ASSERT_EQ(UniPlan::RunBundleChangelogCommand({"--topic", "SampleTopic",
                                                  "--phase", "1", "--repo-root",
                                                  mRepoRoot.string()},
                                                 mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("entries"));

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (const auto &Entry : Json["entries"])
    {
        ASSERT_TRUE(Entry.contains("index"));
        const size_t Idx = Entry["index"].get<size_t>();
        ASSERT_LT(Idx, Bundle.mChangeLogs.size());
        EXPECT_EQ(Bundle.mChangeLogs[Idx].mPhase, 1)
            << "filter=1 should only emit rows whose storage-index row "
            << "has mPhase=1";
    }
}

TEST_F(FBundleTestFixture, VerificationQueryEmitsStableIndex)
{
    CopyFixture("SampleTopic");
    StartCapture();
    ASSERT_EQ(UniPlan::RunBundleVerificationCommand(
                  {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("entries"));

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (const auto &Entry : Json["entries"])
    {
        ASSERT_TRUE(Entry.contains("index"))
            << "verification entry missing 'index' field";
        const size_t Idx = Entry["index"].get<size_t>();
        ASSERT_LT(Idx, Bundle.mVerifications.size());
        EXPECT_EQ(Bundle.mVerifications[Idx].mCheck,
                  Entry["check"].get<std::string>());
    }
}

TEST_F(FBundleTestFixture, RiskListEmitsStableIndexAcrossFilter)
{
    // Seed two risks with differing severities, then list filtered to
    // `high` and verify the surviving row carries its original
    // storage index (not the filtered 0-position).
    CopyFixture("SampleTopic");
    ASSERT_EQ(UniPlan::RunRiskAddCommand(
                  {"--topic", "SampleTopic", "--statement", "Low-severity risk",
                   "--severity", "low", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    ASSERT_EQ(
        UniPlan::RunRiskAddCommand({"--topic", "SampleTopic", "--statement",
                                    "High-severity risk", "--severity", "high",
                                    "--repo-root", mRepoRoot.string()},
                                   mRepoRoot.string()),
        0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    // Find the storage index of the high-severity entry we just added.
    size_t HighSeverityIdx = Bundle.mMetadata.mRisks.size();
    for (size_t I = 0; I < Bundle.mMetadata.mRisks.size(); ++I)
    {
        if (Bundle.mMetadata.mRisks[I].mStatement == "High-severity risk")
            HighSeverityIdx = I;
    }
    ASSERT_LT(HighSeverityIdx, Bundle.mMetadata.mRisks.size());

    StartCapture();
    ASSERT_EQ(
        UniPlan::RunRiskListCommand({"--topic", "SampleTopic", "--severity",
                                     "high", "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string()),
        0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("risks"));
    bool bFound = false;
    for (const auto &Risk : Json["risks"])
    {
        ASSERT_TRUE(Risk.contains("index"))
            << "risk list entry missing 'index' field";
        if (Risk["statement"].get<std::string>() == "High-severity risk")
        {
            EXPECT_EQ(Risk["index"].get<size_t>(), HighSeverityIdx)
                << "filtered list must expose the pre-filter storage index";
            bFound = true;
        }
    }
    EXPECT_TRUE(bFound);
}

TEST_F(FBundleTestFixture, NextActionListEmitsStableIndexAcrossFilter)
{
    CopyFixture("SampleTopic");
    ASSERT_EQ(UniPlan::RunNextActionAddCommand(
                  {"--topic", "SampleTopic", "--statement", "Pending action",
                   "--status", "pending", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    ASSERT_EQ(UniPlan::RunNextActionAddCommand(
                  {"--topic", "SampleTopic", "--statement", "Completed action",
                   "--status", "completed", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    size_t CompletedIdx = Bundle.mNextActions.size();
    for (size_t I = 0; I < Bundle.mNextActions.size(); ++I)
    {
        if (Bundle.mNextActions[I].mStatement == "Completed action")
            CompletedIdx = I;
    }
    ASSERT_LT(CompletedIdx, Bundle.mNextActions.size());

    StartCapture();
    ASSERT_EQ(UniPlan::RunNextActionListCommand(
                  {"--topic", "SampleTopic", "--status", "completed",
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("next_actions"));
    bool bFound = false;
    for (const auto &Action : Json["next_actions"])
    {
        ASSERT_TRUE(Action.contains("index"));
        if (Action["statement"].get<std::string>() == "Completed action")
        {
            EXPECT_EQ(Action["index"].get<size_t>(), CompletedIdx);
            bFound = true;
        }
    }
    EXPECT_TRUE(bFound);
}

TEST_F(FBundleTestFixture, AcceptanceCriterionListEmitsStableIndexAcrossFilter)
{
    CopyFixture("SampleTopic");
    ASSERT_EQ(UniPlan::RunAcceptanceCriterionAddCommand(
                  {"--topic", "SampleTopic", "--statement", "Unmet criterion",
                   "--status", "not_met", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    ASSERT_EQ(UniPlan::RunAcceptanceCriterionAddCommand(
                  {"--topic", "SampleTopic", "--statement", "Met criterion",
                   "--status", "met", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    size_t MetIdx = Bundle.mMetadata.mAcceptanceCriteria.size();
    for (size_t I = 0; I < Bundle.mMetadata.mAcceptanceCriteria.size(); ++I)
    {
        if (Bundle.mMetadata.mAcceptanceCriteria[I].mStatement ==
            "Met criterion")
            MetIdx = I;
    }
    ASSERT_LT(MetIdx, Bundle.mMetadata.mAcceptanceCriteria.size());

    StartCapture();
    ASSERT_EQ(UniPlan::RunAcceptanceCriterionListCommand(
                  {"--topic", "SampleTopic", "--status", "met", "--repo-root",
                   mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("acceptance_criteria"));
    bool bFound = false;
    for (const auto &C : Json["acceptance_criteria"])
    {
        ASSERT_TRUE(C.contains("index"));
        if (C["statement"].get<std::string>() == "Met criterion")
        {
            EXPECT_EQ(C["index"].get<size_t>(), MetIdx);
            bFound = true;
        }
    }
    EXPECT_TRUE(bFound);
}

// ===================================================================
// No-truncation contract (v0.97.0+)
//
// Every query surface — JSON and --human — emits byte-identical stored
// content. No string is clipped, no `...` ellipsis is appended by the
// renderer. Guards seed content 2000+ bytes long with a unique
// sentinel at the end; any upstream clip loses the sentinel from
// output. Covers every prior truncation threshold
// (200, 120, 80, 77, 60, 50, 40, 35).
// ===================================================================

static std::string MakeLongSeed(const std::string &InPrefix)
{
    return InPrefix + std::string(2000, 'A') + "_SENTINEL_UNIQUE_TAIL_0x97";
}

TEST_F(FBundleTestFixture, TopicGetJsonEmitsFullPhaseScope)
{
    CreateMinimalFixture("LongTopic", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    const std::string LongScope = MakeLongSeed("Long scope prefix. ");
    ASSERT_EQ(UniPlan::RunPhaseSetCommand({"--topic", "LongTopic", "--phase",
                                           "0", "--scope", LongScope,
                                           "--repo-root", mRepoRoot.string()},
                                          mRepoRoot.string()),
              0);

    StartCapture();
    ASSERT_EQ(UniPlan::RunTopicCommand({"get", "--topic", "LongTopic",
                                        "--repo-root", mRepoRoot.string()},
                                       mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("phase_summary"));
    ASSERT_GT(Json["phase_summary"].size(), 0u);
    EXPECT_EQ(Json["phase_summary"][0]["scope"].get<std::string>(), LongScope)
        << "topic get must emit the phase scope byte-identical to storage";
}

TEST_F(FBundleTestFixture, PhaseGetBatchJsonEmitsFullScope)
{
    CreateMinimalFixture("PGBatch", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    const std::string LongScope = MakeLongSeed("Batch seed. ");
    ASSERT_EQ(UniPlan::RunPhaseSetCommand({"--topic", "PGBatch", "--phase", "1",
                                           "--scope", LongScope, "--repo-root",
                                           mRepoRoot.string()},
                                          mRepoRoot.string()),
              0);

    StartCapture();
    ASSERT_EQ(UniPlan::RunBundlePhaseCommand({"get", "--topic", "PGBatch",
                                              "--phases", "0,1", "--repo-root",
                                              mRepoRoot.string()},
                                             mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("phases"));
    ASSERT_EQ(Json["phases"].size(), 2u);
    EXPECT_EQ(Json["phases"][1]["scope"].get<std::string>(), LongScope)
        << "phase get --phases <csv> batch path must emit full scope";
}

TEST_F(FBundleTestFixture, PhaseGetBriefJsonEmitsFullDone)
{
    CreateMinimalFixture("BriefSeed", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    const std::string LongDone = MakeLongSeed("Done prefix. ");
    ASSERT_EQ(UniPlan::RunPhaseSetCommand({"--topic", "BriefSeed", "--phase",
                                           "0", "--done", LongDone,
                                           "--repo-root", mRepoRoot.string()},
                                          mRepoRoot.string()),
              0);

    StartCapture();
    ASSERT_EQ(UniPlan::RunBundlePhaseCommand(
                  {"get", "--topic", "BriefSeed", "--phase", "0", "--brief",
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["done"].get<std::string>(), LongDone)
        << "phase get --brief JSON `done` must emit full content";
}

TEST_F(FBundleTestFixture, ChangelogQueryJsonEmitsFullPhaseLabelAndChange)
{
    CreateMinimalFixture("CLFull", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    const std::string LongScope =
        MakeLongSeed("Changelog phase_label scope seed. ");
    ASSERT_EQ(UniPlan::RunPhaseSetCommand({"--topic", "CLFull", "--phase", "0",
                                           "--scope", LongScope, "--repo-root",
                                           mRepoRoot.string()},
                                          mRepoRoot.string()),
              0);
    const std::string LongChange = MakeLongSeed("Changelog body. ");
    ASSERT_EQ(UniPlan::RunChangelogAddCommand(
                  {"--topic", "CLFull", "--phase", "0", "--change", LongChange,
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);

    StartCapture();
    ASSERT_EQ(UniPlan::RunBundleChangelogCommand(
                  {"--topic", "CLFull", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("entries"));
    bool bSawChange = false;
    for (const auto &Entry : Json["entries"])
    {
        if (Entry.contains("change") &&
            Entry["change"].get<std::string>() == LongChange)
        {
            bSawChange = true;
            EXPECT_NE(Entry["phase_label"].get<std::string>().find(
                          "_SENTINEL_UNIQUE_TAIL_0x97"),
                      std::string::npos)
                << "phase_label must embed the full phase scope";
        }
    }
    EXPECT_TRUE(bSawChange);
}

TEST_F(FBundleTestFixture, VerificationQueryJsonEmitsFullCheckAndDetail)
{
    CreateMinimalFixture("VFull", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    const std::string LongCheck = MakeLongSeed("Verification check. ");
    const std::string LongDetail = MakeLongSeed("Verification detail. ");
    ASSERT_EQ(UniPlan::RunVerificationAddCommand(
                  {"--topic", "VFull", "--phase", "0", "--check", LongCheck,
                   "--detail", LongDetail, "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);

    StartCapture();
    ASSERT_EQ(UniPlan::RunBundleVerificationCommand(
                  {"--topic", "VFull", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("entries"));
    bool bFound = false;
    for (const auto &Entry : Json["entries"])
    {
        if (Entry.contains("check") &&
            Entry["check"].get<std::string>() == LongCheck)
        {
            bFound = true;
            EXPECT_EQ(Entry["detail"].get<std::string>(), LongDetail);
        }
    }
    EXPECT_TRUE(bFound);
}

TEST_F(FBundleTestFixture, ValidateJsonEmitsFullDuplicateChangelogDetail)
{
    // no_duplicate_changelog previously clipped the duplicate `change`
    // text preview at 60 chars + "..." inside issue.detail. v0.97.0
    // removes the clip — the full duplicate change text must appear.
    CreateMinimalFixture("DupCL", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    const std::string LongChange = MakeLongSeed("Duplicate changelog body. ");
    ASSERT_EQ(UniPlan::RunChangelogAddCommand(
                  {"--topic", "DupCL", "--phase", "0", "--change", LongChange,
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    ASSERT_EQ(UniPlan::RunChangelogAddCommand(
                  {"--topic", "DupCL", "--phase", "0", "--change", LongChange,
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);

    StartCapture();
    ASSERT_EQ(UniPlan::RunBundleValidateCommand(
                  {"--topic", "DupCL", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("issues"));
    bool bFoundFullDetail = false;
    for (const auto &Issue : Json["issues"])
    {
        if (Issue["id"].get<std::string>() == "no_duplicate_changelog")
        {
            const std::string Detail = Issue["detail"].get<std::string>();
            if (Detail.find("_SENTINEL_UNIQUE_TAIL_0x97") != std::string::npos)
                bFoundFullDetail = true;
        }
    }
    EXPECT_TRUE(bFoundFullDetail)
        << "validate issue detail must carry the full duplicate change "
           "text (v0.97.0 — previously clipped at 60 chars)";
}

TEST_F(FBundleTestFixture, TopicGetHumanEmitsFullPhaseScope)
{
    CreateMinimalFixture("HumanLong", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    const std::string LongScope = MakeLongSeed("Human render seed. ");
    ASSERT_EQ(UniPlan::RunPhaseSetCommand({"--topic", "HumanLong", "--phase",
                                           "0", "--scope", LongScope,
                                           "--repo-root", mRepoRoot.string()},
                                          mRepoRoot.string()),
              0);

    StartCapture();
    ASSERT_EQ(
        UniPlan::RunTopicCommand({"get", "--topic", "HumanLong", "--human",
                                  "--repo-root", mRepoRoot.string()},
                                 mRepoRoot.string()),
        0);
    StopCapture();
    EXPECT_NE(mCapturedStdout.find("_SENTINEL_UNIQUE_TAIL_0x97"),
              std::string::npos)
        << "--human must emit the full phase scope verbatim";
}

TEST_F(FBundleTestFixture, ChangelogHumanEmitsFullChange)
{
    CreateMinimalFixture("CLH", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    const std::string LongChange = MakeLongSeed("Human changelog body. ");
    ASSERT_EQ(UniPlan::RunChangelogAddCommand(
                  {"--topic", "CLH", "--phase", "0", "--change", LongChange,
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);

    StartCapture();
    ASSERT_EQ(
        UniPlan::RunBundleChangelogCommand(
            {"--topic", "CLH", "--human", "--repo-root", mRepoRoot.string()},
            mRepoRoot.string()),
        0);
    StopCapture();
    EXPECT_NE(mCapturedStdout.find("_SENTINEL_UNIQUE_TAIL_0x97"),
              std::string::npos)
        << "changelog --human must emit the full change byte-identical";
}
