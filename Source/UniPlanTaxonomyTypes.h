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
// FLegacyMdSource — one legacy V3 markdown artifact that fed this V4
// bundle or phase. Stored as `legacy_sources[]` on topic and per-phase
// levels. Consumed by `uni-plan legacy-gap` for deterministic V3↔V4
// parity auditing. Populated by `uni-plan legacy-scan` via filename
// convention matching at migration time; hand-edits may add bespoke
// entries. Empty vector means no known legacy heritage.
// ---------------------------------------------------------------------------

struct FLegacyMdSource
{
    ELegacyMdKind mKind = ELegacyMdKind::Playbook;
    std::string mPath; // repo-relative
};

// ---------------------------------------------------------------------------
// FPhaseTaxonomy — display-oriented taxonomy for watch mode panels.
// Tasks are flat here for display convenience (flattened from
// nested FJobRecord.mTasks).
// ---------------------------------------------------------------------------

struct FPhaseTaxonomy
{
    int mPhaseIndex = -1;
    std::string mPlaybookPath;
    std::vector<FLaneRecord> mLanes;
    std::vector<FJobRecord> mJobs;
    std::vector<FTaskRecord> mTasks; // flattened for display
    std::vector<FFileManifestItem> mFileManifest;
    int mWaveCount = 0;
    int mPlaybookLineCount = 0;
};

} // namespace UniPlan
