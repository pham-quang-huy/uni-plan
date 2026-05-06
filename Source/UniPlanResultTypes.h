#pragma once

#include "UniPlanEnums.h"
#include "UniPlanPhaseMetrics.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Result and data structs — per-command outputs and aggregate telemetry.
// ---------------------------------------------------------------------------

struct StatusCounters
{
    int mNotStarted = 0;
    int mInProgress = 0;
    int mCompleted = 0;
    int mClosed = 0;
    int mBlocked = 0;
    int mCanceled = 0;
    std::string mFirstRaw;
};

struct FBaseResult
{
    std::string mGeneratedUtc;
    std::string mRepoRoot;
    std::vector<std::string> mWarnings;
};

struct LintResult : FBaseResult
{
    int mWarningCount = 0;
    int mNamePatternWarningCount = 0;
    int mMissingH1WarningCount = 0;
};

struct CacheInfoResult
{
    std::string mGeneratedUtc;
    std::string mCacheDir;
    std::string mConfigCacheDir;
    std::string mIniPath;
    bool mbCacheEnabled = true;
    bool mbCacheVerbose = false;
    bool mbCacheExists = false;
    uint64_t mCacheSizeBytes = 0;
    int mCacheEntryCount = 0;
    std::string mCurrentRepoCachePath;
    bool mbCurrentRepoCacheExists = false;
    std::vector<std::string> mWarnings;
};

struct CacheClearResult
{
    std::string mGeneratedUtc;
    std::string mCacheDir;
    int mEntriesRemoved = 0;
    uint64_t mBytesFreed = 0;
    bool mbSuccess = true;
    std::string mError;
    std::vector<std::string> mWarnings;
};

struct CacheConfigResult
{
    std::string mGeneratedUtc;
    std::string mIniPath;
    bool mbSuccess = true;
    std::string mError;
    std::string mDir;
    bool mbEnabled = true;
    bool mbVerbose = false;
    std::vector<std::string> mWarnings;
};

struct HeadingRecord
{
    int mLine = 0;
    int mLevel = 0;
    std::string mText;
    std::string mSectionID;
};

struct MarkdownTableRecord
{
    int mTableID = 0;
    int mStartLine = 0;
    int mEndLine = 0;
    std::string mSectionID;
    std::string mSectionHeading;
    std::vector<std::string> mHeaders;
    std::vector<std::vector<std::string>> mRows;
};

struct ValidateCheck
{
    std::string mID;
    EValidationSeverity mSeverity = EValidationSeverity::Warning;
    bool mbOk = true;
    std::string mTopic; // which bundle (empty for aggregate checks)
    std::string mPath;  // e.g. "phases[2].jobs[1].tasks[0]"
    std::string mDetail;
    int mLine = -1; // 1-based line in the bundle JSON; -1 = unknown
    std::vector<std::string> mDiagnostics;
};

// Single drift observation on a phase, produced by
// ComputePhaseDriftEntries() and shared by RunPhaseDriftCommand and
// EvalPhaseStatusLaneAlignment. Added v0.84.0.
struct FPhaseDriftEntry
{
    std::string mKind; // status_lag_lane | status_lag_done |
                       // status_lag_timestamp | completion_lag_lane
    std::string mDetail;
    std::vector<int> mLaneIndices; // completed/in_progress lanes for
                                   // status_lag_lane; not-completed lanes
                                   // for completion_lag_lane. Empty for
                                   // done/timestamp kinds.
    int mDoneChars = -1;           // substantive-done length (-1 unused)
    bool mbHasCompletedAt = false; // true only for status_lag_timestamp
};

struct BlockerItem
{
    std::string mTopicKey;
    std::string mSourcePath; // absolute path to the .Plan.json bundle
    std::string mKind;       // "status", "text", or "status+text"
    std::string mStatus;     // phase lifecycle status string
    int mPhaseIndex = -1;
    std::string mAction; // the blocker text
    std::string mNotes;  // phase scope (context for the blocker)
};

// PhaseItem — display-oriented projection of a single FPhaseRecord for the
// watch TUI. Mirrors the V4 typed phase-field model directly (mScope,
// mOutput, mDone, mRemaining) — no key/value bag and no V3 fuzzy-match
// heuristics. Populated by BuildPlanSummaryFromBundle from FPhaseRecord.
//
// `mV4DesignChars` drives the default PHASE DETAIL `Design` column. The
// full `mMetrics` payload drives the metrics view and the `phase metric`
// CLI command. Both are runtime projections computed from existing bundle
// fields; no metric is persisted into .Plan.json.
struct PhaseItem
{
    std::string mPhaseKey; // stringified zero-based phase index
    std::string mStatusRaw;
    EExecutionStatus mStatus = EExecutionStatus::NotStarted;
    std::string mScope;
    std::string mOutput;
    std::string mDone;
    std::string mRemaining;
    std::string mCodeSnippets;
    size_t mV4DesignChars = 0; // ComputePhaseDesignChars(FPhaseRecord)
    FPhaseRuntimeMetrics mMetrics;
};

// Per-subcommand help entry (v0.85.0). Each FCommandHelpEntry carries an
// optional array of these so `<cmd> <sub> --help` can print targeted
// help for a specific subcommand instead of the pooled group block.
//
// mModes         — optional block for mutually exclusive modes
//                  (e.g. phase get's --brief/--design/--execution).
// mOutputSchema  — optional schema name constant (e.g. kPhaseGetSchema)
//                  surfaced so agents can learn the response contract
//                  without running the command.
// mbIsProseCommand — true appends the shared kFileFlagFooter for
//                  commands that accept `--<field>-file <path>` flags.
// mbSupportsHuman  — true appends kHumanTable / kHumanList / similar
//                  for commands with a --human renderer.
struct FSubcommandHelpEntry
{
    const char *mName;
    const char *mUsageLine;
    const char *mDescription;
    const char *mRequiredOptions; // nullable
    const char *mSpecificOptions; // nullable
    const char *mModes;           // nullable
    const char *mOutputSchema;    // nullable
    const char *mExamples;
    bool mbIsProseCommand = false;
    bool mbSupportsHuman = false;
};

// Per-command help entry. Groups (topic, phase, ...) carry a subcommand
// array; single-command groups (validate, timeline, ...) leave
// mpSubcommands null and populate the leaf fields directly.
struct FCommandHelpEntry
{
    const char *mName;
    const char *mUsageLine;
    const char *mDescription;
    const char *mRequiredOptions;
    const char *mSpecificOptions;
    const char *mHumanLabel;
    const char *mExamples;
    // Added v0.85.0. Existing 7-field aggregate initializers continue to
    // compile — trailing fields zero-init to {nullptr, 0}.
    const FSubcommandHelpEntry *mpSubcommands = nullptr;
    size_t mSubcommandCount = 0;
};

// ---------------------------------------------------------------------------
// Legacy-gap result types (stateless per-phase V3 <-> V4 parity, 0.75.0+)
// ---------------------------------------------------------------------------

// FPhaseGapRow — one phase's parity status row. Consumed by both JSON and
// human emitters of `uni-plan legacy-gap`.
struct FPhaseGapRow
{
    std::string mTopic;
    int mPhaseIndex = -1;
    EExecutionStatus mPhaseStatus = EExecutionStatus::NotStarted;
    std::string mLegacyPath;   // repo-relative; empty if LegacyAbsent/V4Only
    int mLegacyLoc = 0;        // content LOC, banner-stripped
    size_t mV4DesignChars = 0; // scope + output + design material total
    size_t mV4JobsCount = 0;
    EPhaseGapCategory mCategory = EPhaseGapCategory::LegacyAbsent;
};

// FLegacyGapReport — one full audit pass for `uni-plan legacy-gap`.
struct FLegacyGapReport
{
    std::string mGeneratedUtc;
    std::string mRepoRoot;
    int mBundleCount = 0;
    std::vector<FPhaseGapRow> mRows;
    std::vector<std::string> mWarnings;
};

} // namespace UniPlan
