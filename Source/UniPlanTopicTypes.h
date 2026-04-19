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
    // Semantic provenance stamp — durable, filesystem-independent.
    // `NativeV4` for phases authored directly against the V4 schema;
    // `V3Migration` for phases produced from the V3 markdown corpus.
    // Missing key on read (pre-0.75.0 bundles) deserializes to NativeV4.
    EPhaseOrigin mOrigin = EPhaseOrigin::NativeV4;
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

    // Runtime-only: path where this bundle was loaded from.
    // Not serialized to JSON. Set by TryLoadBundleByTopic and
    // LoadAllBundles. Used by WriteBundleBack to write back
    // to the same location.
    std::string mBundlePath;
};

// ---------------------------------------------------------------------------
// Phase-level computed helpers.
// ---------------------------------------------------------------------------

// Total char count of all authored prose on a phase: scope + output +
// every design material field. Mirrors `v4_design_chars` from the
// `legacy-gap` schema — a single honest measure of "how much plan has
// been authored for this phase." Used by the watch TUI to drive the
// PHASE DETAIL `Design` column and by `legacy-gap` to categorize
// phases along the hollow / thin / rich spectrum.
inline size_t ComputePhaseDesignChars(const FPhaseRecord &InPhase)
{
    const FPhaseDesignMaterial &Design = InPhase.mDesign;
    return InPhase.mScope.size() + InPhase.mOutput.size() +
           Design.mInvestigation.size() + Design.mCodeEntityContract.size() +
           Design.mCodeSnippets.size() + Design.mBestPractices.size() +
           Design.mHandoff.size() + Design.mReadinessGate.size() +
           Design.mMultiPlatforming.size();
}

// Phase-depth thresholds — char-based measures of "how much plan has
// been authored." Calibrated against the V3 corpus convention that a
// proper Playbook.md was 200–400 lines of content; at ~80 chars/line
// that's 16000–32000 chars. The chars-form and LOC-form thresholds
// are kept in lockstep (80 chars ≈ 1 line) so V3 LOC and V4 chars
// classify phases into the same hollow / thin / rich buckets.
//
//   < kPhaseHollowChars     — hollow: not enough plan to execute
//   [hollow, rich) chars    — thin:   executable but sparse
//   ≥ kPhaseRichMinChars    — rich:   properly detailed playbook
//
// Bumped in v0.80.0 from the prior 500 / 2000 values, which mapped to
// only ~6 / ~25 lines and classified even bare-skeleton phases as
// "has a plan." The new thresholds match the V3 Playbook.md discipline
// that required 200+ lines of content for a phase to be considered
// authored.
static constexpr size_t kPhaseHollowChars = 4000;   // ≈ 50 lines
static constexpr size_t kPhaseRichMinChars = 16000; // ≈ 200 lines

} // namespace UniPlan
