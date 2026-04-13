#pragma once

#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Execution taxonomy types shared between CLI commands and watch mode.
// These replace the former FWatch*Item types in UniPlanWatchSnapshot.h.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// FLaneRecord — one execution lane within a phase playbook.
// Source: playbook execution_lanes table.
// Replaces: FWatchLaneItem.
// ---------------------------------------------------------------------------

struct FLaneRecord
{
    std::string mLaneID;
    std::string mStatus;
    std::string mScope;
    std::string mExitCriteria;
};

// ---------------------------------------------------------------------------
// FJobRecord — one job within a wave/lane in the job board.
// Source: playbook wave_lane_job_board table.
// Replaces: FWatchJobItem.
// ---------------------------------------------------------------------------

struct FJobRecord
{
    std::string mWaveID;
    std::string mLaneID;
    std::string mJobID;
    std::string mJobName;
    std::string mStatus;
    std::string mScope;
    std::string mExitCriteria;
};

// ---------------------------------------------------------------------------
// FTaskRecord — one checklist task within a job.
// Source: playbook job_task_checklist table.
// Replaces: FWatchTaskItem.
// ---------------------------------------------------------------------------

struct FTaskRecord
{
    std::string mJobRef;
    std::string mTaskID;
    std::string mStatus;
    std::string mDescription;
    std::string mEvidence;
};

// ---------------------------------------------------------------------------
// FFileManifestItem — one entry in the target file manifest.
// Source: playbook target_file_manifest table.
// Replaces: FWatchFileManifestItem.
// ---------------------------------------------------------------------------

struct FFileManifestItem
{
    std::string mFilePath;
    std::string mAction;
    std::string mDescription;
};

// ---------------------------------------------------------------------------
// FPhaseTaxonomy — complete execution taxonomy for one phase.
// Aggregates lanes, jobs, tasks, and file manifest from a playbook.
// Replaces: FWatchPhaseTaxonomy.
// ---------------------------------------------------------------------------

struct FPhaseTaxonomy
{
    std::string mPhaseKey;
    std::string mPlaybookPath;
    std::vector<FLaneRecord> mLanes;
    std::vector<FJobRecord> mJobs;
    std::vector<FTaskRecord> mTasks;
    std::vector<FFileManifestItem> mFileManifest;
    int mWaveCount = 0;
    int mPlaybookLineCount = 0;
};

} // namespace UniPlan
