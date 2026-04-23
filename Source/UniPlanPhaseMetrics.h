#pragma once

#include "UniPlanTopicTypes.h"

#include <cstddef>

namespace UniPlan
{

// Runtime-only advisory metrics for phase depth. These values are computed
// from the typed bundle model and are never serialized into .Plan.json files.
static constexpr size_t kPhaseMetricSolidHollowWords = 12;
static constexpr size_t kPhaseMetricSolidRichWords = 40;
static constexpr size_t kPhaseMetricRecursiveHollowWords = 450;
static constexpr size_t kPhaseMetricRecursiveRichWords = 1800;
static constexpr size_t kPhaseMetricFieldCoverageHollowPercent = 35;
static constexpr size_t kPhaseMetricFieldCoverageRichPercent = 75;
static constexpr size_t kPhaseMetricWorkHollowItems = 5;
static constexpr size_t kPhaseMetricWorkRichItems = 20;
static constexpr size_t kPhaseMetricTestingHollowRecords = 1;
static constexpr size_t kPhaseMetricTestingRichRecords = 3;
static constexpr size_t kPhaseMetricFileManifestHollowEntries = 1;
static constexpr size_t kPhaseMetricFileManifestRichEntries = 5;
static constexpr size_t kPhaseMetricEvidenceHollowItems = 1;
static constexpr size_t kPhaseMetricEvidenceRichItems = 5;
static constexpr size_t kPhaseMetricAuthoredFieldTotal = 22;

struct FPhaseRuntimeMetrics
{
    size_t mDesignChars = 0;
    size_t mSolidWordCount = 0;
    size_t mRecursiveWordCount = 0;
    size_t mAuthoredFieldCount = 0;
    size_t mAuthoredFieldTotal = kPhaseMetricAuthoredFieldTotal;
    size_t mFieldCoveragePercent = 0;
    size_t mLaneCount = 0;
    size_t mJobCount = 0;
    size_t mTaskCount = 0;
    size_t mWorkItemCount = 0;
    size_t mTestingRecordCount = 0;
    size_t mFileManifestCount = 0;
    size_t mEvidenceItemCount = 0;
    size_t mVerificationCount = 0;
    size_t mChangelogCount = 0;
};

FPhaseRuntimeMetrics ComputePhaseDepthMetrics(const FTopicBundle &InBundle,
                                              size_t InPhaseIndex);

} // namespace UniPlan
