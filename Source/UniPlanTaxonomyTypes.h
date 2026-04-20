#pragma once

#include "UniPlanEnums.h"

#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Execution taxonomy types shared between CLI commands and watch mode.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// FLaneRecord — one execution lane within a phase.
// ---------------------------------------------------------------------------

struct FLaneRecord
{
    EExecutionStatus mStatus = EExecutionStatus::NotStarted;
    std::string mScope;
    std::string mExitCriteria;
};

// ---------------------------------------------------------------------------
// FTaskRecord — one checklist task within a job.
// Defined before FJobRecord so it can be nested inside it.
// ---------------------------------------------------------------------------

struct FTaskRecord
{
    EExecutionStatus mStatus = EExecutionStatus::NotStarted;
    std::string mDescription;
    std::string mEvidence;
    std::string mNotes;
    std::string mCompletedAt; // ISO 8601
};

// ---------------------------------------------------------------------------
// FJobRecord — one job within a wave/lane in the job board.
// Tasks are nested inside their parent job (no back-reference).
// ---------------------------------------------------------------------------

struct FJobRecord
{
    int mWave = 0;
    int mLane = 0;
    EExecutionStatus mStatus = EExecutionStatus::NotStarted;
    std::string mScope;
    std::string mOutput;
    std::string mExitCriteria;
    std::vector<FTaskRecord> mTasks;

    // Agent execution lifecycle
    std::string mStartedAt;
    std::string mCompletedAt;
};

// ---------------------------------------------------------------------------
// FFileManifestItem — one entry in the target file manifest.
// ---------------------------------------------------------------------------

struct FFileManifestItem
{
    std::string mFilePath;
    EFileAction mAction = EFileAction::Create;
    std::string mDescription;
};

// ---------------------------------------------------------------------------
// FValidationCommand — one row in a bundle's validation_commands table.
// Replaces the former `std::string mValidationCommands` markdown-table
// field with a typed record so the validator can enforce shape (platform
// scope, non-empty command, description) without regex-scanning prose.
// ---------------------------------------------------------------------------

struct FValidationCommand
{
    EPlatformScope mPlatform = EPlatformScope::Any;
    std::string mCommand;     // shell command string (opaque to transforms)
    std::string mDescription; // success criterion / purpose
};

// ---------------------------------------------------------------------------
// FBundleReference — one row in a bundle's dependencies table.
// Replaces the former `std::string mDependencies` markdown-table field with
// a typed record so dependency integrity (topic resolves, phase index valid)
// can be enforced at parse time rather than by regex-scanning prose.
//
// mTopic is the only required field for Kind=Bundle/Phase. For Governance
// or External refs, mPath points at the doc; mTopic may be empty.
// ---------------------------------------------------------------------------

struct FBundleReference
{
    EDependencyKind mKind = EDependencyKind::Bundle;
    std::string mTopic; // topic key, e.g. "ClientServer"
    int mPhase = -1;    // phases[N] if mKind==Phase, -1 otherwise
    std::string mPath;  // file path (Governance/External)
    std::string mNote;  // freeform description
};

// ---------------------------------------------------------------------------
// FRiskEntry — one row in a bundle's risks array.
// Replaces the former `std::string mRisks` pipe-delimited-prose field with
// a typed record so risk severity/status can be enforced at parse time and
// high-severity risks can be validated to carry explicit mitigations.
//
// mStatement is required (non-empty). mId is optional and carries a stable
// token like "R1" for cross-references from changelog/verification prose;
// if empty, consumers may synthesize from index. mMitigation is required
// for quality on High/Critical risks but optional at schema level so a
// new risk can be captured without a mitigation yet and flagged by the
// `risk_severity_populated_for_high_impact` validator.
// ---------------------------------------------------------------------------

struct FRiskEntry
{
    std::string mId;
    std::string mStatement;
    std::string mMitigation;
    ERiskSeverity mSeverity = ERiskSeverity::Medium;
    ERiskStatus mStatus = ERiskStatus::Open;
    std::string mNotes;
};

// ---------------------------------------------------------------------------
// FNextActionEntry — one row in a bundle's next_actions array.
// Replaces the former `std::string mNextActions` pipe-delimited-prose
// field with a typed record so action ordering/status can be enforced at
// parse time and `uni-plan phase next` can surface actionable items.
//
// mStatement is required (non-empty). mOrder is the display order within
// the array (enforced unique by the `next_action_order_unique` validator);
// 0 is reserved for unordered legacy imports. mTargetDate is optional ISO
// 8601 (`YYYY-MM-DD`) or a phase ref like `phases[2]`.
// ---------------------------------------------------------------------------

struct FNextActionEntry
{
    int mOrder = 0;
    std::string mStatement;
    std::string mRationale;
    std::string mOwner;
    EActionStatus mStatus = EActionStatus::Pending;
    std::string mTargetDate;
};

// ---------------------------------------------------------------------------
// FAcceptanceCriterionEntry — one row in a bundle's acceptance_criteria
// array.
// Replaces the former `std::string mAcceptanceCriteria` pipe-delimited-
// prose field with a typed record so per-criterion status can be tracked
// and `completed_topic_criteria_all_met` can validate completion honesty.
//
// mStatement is required (non-empty). mId is optional stable token like
// "AC1". mMeasure captures how to verify; mEvidence references a
// verification entry, commit SHA, or PR when the criterion is met.
// ---------------------------------------------------------------------------

struct FAcceptanceCriterionEntry
{
    std::string mId;
    std::string mStatement;
    ECriterionStatus mStatus = ECriterionStatus::NotMet;
    std::string mMeasure;
    std::string mEvidence;
};

// ---------------------------------------------------------------------------
// FPhaseTaxonomy — display-oriented taxonomy for watch mode panels.
// Tasks are flat here for display convenience (flattened from
// nested FJobRecord.mTasks).
// ---------------------------------------------------------------------------

struct FPhaseTaxonomy
{
    int mPhaseIndex = -1;
    std::vector<FLaneRecord> mLanes;
    std::vector<FJobRecord> mJobs;
    std::vector<FTaskRecord> mTasks; // flattened for display
    std::vector<FFileManifestItem> mFileManifest;
    int mWaveCount = 0;
};

} // namespace UniPlan
