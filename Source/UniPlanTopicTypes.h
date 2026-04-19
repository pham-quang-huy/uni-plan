#pragma once

#include "UniPlanTaxonomyTypes.h"

#include <map>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Topic bundle types — one .Plan.json file per topic.
// Schema: plan-v4
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// FChangeLogEntry — one changelog entry.
// ---------------------------------------------------------------------------

struct FChangeLogEntry
{
    int mPhase = -1; // phase index (0, 1, ...) or -1 for topic-level
    std::string mDate;
    std::string mChange;
    std::string mAffected; // entity refs (e.g. "phases[0].jobs[2]")
    EChangeType mType = EChangeType::Chore;
    ETestingActor mActor = ETestingActor::Human;
};

// ---------------------------------------------------------------------------
// FVerificationEntry — one verification entry.
// ---------------------------------------------------------------------------

struct FVerificationEntry
{
    int mPhase = -1; // phase index (0, 1, ...) or -1 for topic-level
    std::string mDate;
    std::string mCheck;
    std::string mResult;
    std::string mDetail;
};

// ---------------------------------------------------------------------------
// FTestingRecord — one testing step within a phase.
// ---------------------------------------------------------------------------

struct FTestingRecord
{
    std::string mSession;
    ETestingActor mActor = ETestingActor::Human;
    std::string mStep;
    std::string mAction;
    std::string mExpected;
    std::string mEvidence;
};

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// FPhaseLifecycle — execution status, timestamps, and tracking.
// Changes when phase progress changes.
// ---------------------------------------------------------------------------

struct FPhaseLifecycle
{
    EExecutionStatus mStatus = EExecutionStatus::NotStarted;
    std::string mDone;
    std::string mRemaining;
    std::string mBlockers;
    std::string mStartedAt;
    std::string mCompletedAt;
    std::string mAgentContext;
};

// ---------------------------------------------------------------------------
// FPhaseDesignMaterial — investigation, code snippets, constraints.
// Changes when phase design/planning changes.
// ---------------------------------------------------------------------------

struct FPhaseDesignMaterial
{
    std::string mInvestigation;
    std::string mCodeSnippets;
    std::vector<FBundleReference> mDependencies;
    std::string mReadinessGate;
    std::string mHandoff;
    std::string mCodeEntityContract;
    std::string mBestPractices;
    std::vector<FValidationCommand> mValidationCommands;
    std::string mMultiPlatforming;
};

// ---------------------------------------------------------------------------
// FPhaseRecord — complete phase with execution taxonomy + tracking.
// ---------------------------------------------------------------------------

struct FPhaseRecord
{
    std::string mScope;
    std::string mOutput;
    FPhaseLifecycle mLifecycle;
    FPhaseDesignMaterial mDesign;
    std::vector<FLaneRecord> mLanes;
    std::vector<FJobRecord> mJobs;
    std::vector<FTestingRecord> mTesting;
    std::vector<FFileManifestItem> mFileManifest;
    // Legacy V3 artifacts that fed this phase. Populated by
    // `uni-plan legacy-scan`, consumed by `uni-plan legacy-gap`.
    // Empty vector means no known V3 heritage (native V4 phase).
    std::vector<FLegacyMdSource> mLegacySources;
};

// ---------------------------------------------------------------------------
// FPlanMetadata — plan-level descriptive fields.
// Grouped by single-responsibility: these change when the plan's
// scope, goals, or constraints change.
// ---------------------------------------------------------------------------

struct FPlanMetadata
{
    std::string mTitle;
    std::string mSummary;
    std::string mGoals;
    std::string mNonGoals;
    std::string mRisks;
    std::string mAcceptanceCriteria;
    std::string mProblemStatement;
    std::vector<FValidationCommand> mValidationCommands;
    std::string mBaselineAudit;
    std::string mExecutionStrategy;
    std::string mLockedDecisions;
    std::string mSourceReferences;
    std::vector<FBundleReference> mDependencies;
};

// ---------------------------------------------------------------------------
// FTopicBundle — complete governance bundle for one topic.
// Stored as a single <TopicKey>.Plan.json file.
// ---------------------------------------------------------------------------

struct FTopicBundle
{
    std::string mTopicKey;
    ETopicStatus mStatus = ETopicStatus::NotStarted;
    FPlanMetadata mMetadata;

    std::vector<FPhaseRecord> mPhases;
    std::string mNextActions;

    std::vector<FChangeLogEntry> mChangeLogs;
    std::vector<FVerificationEntry> mVerifications;

    // Topic-level legacy V3 artifacts (Plan.md, Impl.md, their sidecars).
    // Per-phase legacy playbooks live on FPhaseRecord.mLegacySources.
    // Populated by `uni-plan legacy-scan`, consumed by `uni-plan legacy-gap`.
    std::vector<FLegacyMdSource> mLegacySources;

    // Runtime-only: path where this bundle was loaded from.
    // Not serialized to JSON. Set by TryLoadBundleByTopic and
    // LoadAllBundles. Used by WriteBundleBack to write back
    // to the same location.
    std::string mBundlePath;
};

} // namespace UniPlan
