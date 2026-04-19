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
// been authored." Calibrated against V4 schema semantics, NOT a raw
// V3-line-count translation.
//
// Derivation (schema-based, independent of any repo's data):
//   ComputePhaseDesignChars sums 9 fields: scope + output +
//   investigation + code_entity_contract + code_snippets +
//   best_practices + handoff + readiness_gate + multi_platforming.
//
//   Minimum substantive populated record: 5-7 fields × 1-3 sentences
//   each (~300-500 chars) → ~3000 chars total. Below this, a reader
//   cannot reconstruct what happened across the schema surface —
//   truly hollow, regardless of whether any single field is non-empty.
//
//   Fully populated record: 9 fields × multi-paragraph (~1000-1200
//   chars each) → ~10000 chars total. Above this, the phase is
//   exhaustively documented — a "properly authored playbook" in V4
//   terms. 150 V3 content-LOC × ~67 chars/signal-line ≈ 10000 V4
//   chars, so the LOC-form `kLegacyRichMinLoc` (150) aligns cleanly
//   with the chars-form `kPhaseRichMinChars` (10000).
//
// Classification:
//   < kPhaseHollowChars     — hollow: not enough authored prose
//   [hollow, rich) chars    — thin:   executable but sparse
//   ≥ kPhaseRichMinChars    — rich:   exhaustively documented
//
// Version history:
//   v0.80.0: bumped from 500 / 2000 (too lenient — ~6 / ~25 lines).
//   v0.80.0–0.82.0: 4000 / 16000, derived by mechanically translating
//                   "V3 Playbook.md was 200+ lines" × 80 chars/line.
//                   This OVER-translated: V3 .md carried banner +
//                   section headers + transition prose overhead that
//                   V4 JSON discards. Signal content of a 200-line
//                   V3 playbook is well below 16000 V4 chars.
//   v0.83.0: recalibrated to 3000 / 10000 against schema semantics,
//            ratified by the user. The hollow gate retains its
//            structural-evidence safety valve
//            (no_hollow_completed_phase only fires when jobs AND
//            testing AND file_manifest are all empty AND prose is
//            below the threshold), so these thresholds bound the
//            pure-prose-only path.
static constexpr size_t kPhaseHollowChars = 3000;   // ≈ 5-7 populated fields
static constexpr size_t kPhaseRichMinChars = 10000; // ≈ all 9 fields richly

} // namespace UniPlan
