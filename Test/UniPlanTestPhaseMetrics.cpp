#include "UniPlanPhaseMetrics.h"

#include <gtest/gtest.h>

TEST(PhaseMetrics, ComputesRecursiveRuntimeSignals)
{
    UniPlan::FTopicBundle Bundle;
    Bundle.mTopicKey = "MetricTopic";

    UniPlan::FPhaseRecord Phase;
    Phase.mScope =
        "Refactor the runtime command boundary with single responsibility.";
    Phase.mOutput = "Typed domain contract and validation coverage.";
    Phase.mLifecycle.mDone = "Interface segregation proof captured.";
    Phase.mLifecycle.mAgentContext = "Keep low coupling across modules.";
    Phase.mDesign.mInvestigation =
        "Dependency inversion and dependency injection remove coupling.";
    Phase.mDesign.mCodeEntityContract =
        "Public contract owns the abstraction and invariant.";
    Phase.mDesign.mBestPractices =
        "No god struct, no monolith file, preserve behavior change gates.";
    Phase.mDesign.mReadinessGate =
        "Validation command must prove domain boundary isolation.";
    Phase.mDesign.mHandoff =
        "Junior developer can implement the adapter registry.";
    Phase.mDesign.mMultiPlatforming =
        "Module boundary stays independent of platform details.";

    UniPlan::FLaneRecord Lane;
    Lane.mScope = "Boundary cleanup lane.";
    Lane.mExitCriteria = "Interface contract reviewed.";
    Phase.mLanes.push_back(Lane);

    UniPlan::FJobRecord Job;
    Job.mScope = "Split the runtime command facade.";
    Job.mOutput = "Composition based implementation.";
    Job.mExitCriteria = "Validation stays green.";
    UniPlan::FTaskRecord Task;
    Task.mDescription = "Introduce typed enum and ownership boundary.";
    Task.mEvidence = "unit test evidence";
    Task.mNotes = "No raw primitive domain state.";
    Job.mTasks.push_back(Task);
    Phase.mJobs.push_back(Job);

    UniPlan::FTestingRecord Test;
    Test.mSession = "manual";
    Test.mStep = "Run command";
    Test.mAction = "Execute validation";
    Test.mExpected = "Contract remains stable";
    Test.mEvidence = "terminal output evidence";
    Phase.mTesting.push_back(Test);

    UniPlan::FFileManifestItem File;
    File.mFilePath = "Source/RuntimeCommand.cpp";
    File.mDescription = "Owns runtime command boundary.";
    Phase.mFileManifest.push_back(File);

    Bundle.mPhases.push_back(Phase);

    UniPlan::FChangeLogEntry Change;
    Change.mPhase = 0;
    Change.mChange = "Runtime command contract refactor planned.";
    Change.mAffected = "phases[0].jobs[0]";
    Bundle.mChangeLogs.push_back(Change);

    UniPlan::FVerificationEntry Verification;
    Verification.mPhase = 0;
    Verification.mCheck = "Command contract verification.";
    Verification.mResult = "pass";
    Verification.mDetail = "Evidence confirms boundary behavior.";
    Bundle.mVerifications.push_back(Verification);

    const UniPlan::FPhaseRuntimeMetrics Metrics =
        UniPlan::ComputePhaseDepthMetrics(Bundle, 0);

    EXPECT_EQ(Metrics.mDesignChars,
              UniPlan::ComputePhaseDesignChars(Bundle.mPhases[0]));
    EXPECT_EQ(Metrics.mLaneCount, 1);
    EXPECT_EQ(Metrics.mJobCount, 1);
    EXPECT_EQ(Metrics.mTaskCount, 1);
    EXPECT_EQ(Metrics.mWorkItemCount, 3);
    EXPECT_EQ(Metrics.mTestingRecordCount, 1);
    EXPECT_EQ(Metrics.mFileManifestCount, 1);
    EXPECT_EQ(Metrics.mVerificationCount, 1);
    EXPECT_EQ(Metrics.mChangelogCount, 1);
    EXPECT_EQ(Metrics.mEvidenceItemCount, 4);
    EXPECT_EQ(Metrics.mAuthoredFieldTotal,
              UniPlan::kPhaseMetricAuthoredFieldTotal);
    EXPECT_GT(Metrics.mRecursiveWordCount, 60);
    EXPECT_GE(Metrics.mSolidWordCount, 30);
    EXPECT_GE(Metrics.mFieldCoveragePercent, 75);
}

TEST(PhaseMetrics, OutOfRangeReturnsEmptyRuntimeMetrics)
{
    UniPlan::FTopicBundle Bundle;
    const UniPlan::FPhaseRuntimeMetrics Metrics =
        UniPlan::ComputePhaseDepthMetrics(Bundle, 4);

    EXPECT_EQ(Metrics.mDesignChars, 0);
    EXPECT_EQ(Metrics.mSolidWordCount, 0);
    EXPECT_EQ(Metrics.mRecursiveWordCount, 0);
    EXPECT_EQ(Metrics.mAuthoredFieldTotal,
              UniPlan::kPhaseMetricAuthoredFieldTotal);
}
