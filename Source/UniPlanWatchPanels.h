#pragma once

#include "UniPlanWatchScroll.h"
#include "UniPlanWatchSnapshot.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace UniPlan
{

struct FWatchProseLogicalLine
{
    std::string mText;
    std::string mPrefix;
    ftxui::Color mForeground = ftxui::Color::Default;
    ftxui::Color mBackground = ftxui::Color::Default;
    bool mbDim = false;
    bool mbBold = false;
    bool mbNumbered = true;
    bool mbSeparator = false;
};

struct FWatchProseVisualRow
{
    int mLineNumber = 0;
    bool mbContinuation = false;
    bool mbSeparator = false;
    std::string mText;
    ftxui::Color mForeground = ftxui::Color::Default;
    ftxui::Color mBackground = ftxui::Color::Default;
    bool mbDim = false;
    bool mbBold = false;
};

struct FWatchProseLayoutCache
{
    uint64_t mSnapshotGeneration = 0;
    std::string mTopicKey;
    int mPhaseIndex = -1;
    int mAvailableTextCells = -1;
    int mLineCount = 0;
    int mGutterWidth = 0;
    int mLogicalBuildCount = 0;
    int mVisualBuildCount = 0;
    bool mbLogicalValid = false;
    std::vector<FWatchProseLogicalLine> mLines;
    std::vector<FWatchProseVisualRow> mRows;

    void Reset();
};

struct FWatchMetricGaugeScale
{
    size_t mHollow = 0;
    size_t mRich = 0;
    size_t mMinValue = 0;
    size_t mMaxValue = 0;
    bool mbUseRelativeRichRange = false;
};

struct FWatchPhaseListRowModel
{
    int mPhaseIndex = -1;
    std::string mPhaseKey;
    std::string mStatus;
    size_t mDesignChars = 0;
    std::string mTaxonomy;
    std::string mScope;
    std::string mOutput;
    FPhaseRuntimeMetrics mMetrics;
};

struct FWatchPhaseListLayoutCache
{
    uint64_t mSnapshotGeneration = 0;
    std::string mTopicKey;
    bool mbMetricView = false;
    bool mbValid = false;
    int mBuildCount = 0;
    int mPhaseColumnWidth = 0;
    int mDefaultDesignColumnWidth = 0;
    int mTaxonomyColumnWidth = 0;
    int mMetricDesignColumnWidth = 0;
    int mBloatColumnWidth = 0;
    int mSolidColumnWidth = 0;
    int mWordsColumnWidth = 0;
    int mFieldsColumnWidth = 0;
    int mWorkColumnWidth = 0;
    int mTestsColumnWidth = 0;
    int mFilesColumnWidth = 0;
    int mEvidenceColumnWidth = 0;
    FWatchMetricGaugeScale mDesignScale;
    FWatchMetricGaugeScale mSolidScale;
    FWatchMetricGaugeScale mWordsScale;
    FWatchMetricGaugeScale mFieldsScale;
    FWatchMetricGaugeScale mWorkScale;
    FWatchMetricGaugeScale mTestsScale;
    FWatchMetricGaugeScale mFilesScale;
    FWatchMetricGaugeScale mEvidenceScale;
    std::vector<FWatchPhaseListRowModel> mRows;

    void Reset();
};

struct FCodeSnippetLogicalLine
{
    std::string mText;
    bool mbCode = true;
};

struct FCodeSnippetVisualRow
{
    int mLineNumber = 0;
    bool mbContinuation = false;
    bool mbCode = true;
    std::string mText;
};

struct FWatchCodeSnippetLayoutCache
{
    uint64_t mSnapshotGeneration = 0;
    std::string mTopicKey;
    int mPhaseIndex = -1;
    int mAvailableTextCells = -1;
    int mLineCount = 0;
    int mGutterWidth = 0;
    int mLogicalBuildCount = 0;
    int mVisualBuildCount = 0;
    bool mbLogicalValid = false;
    std::vector<FCodeSnippetLogicalLine> mLines;
    std::vector<FCodeSnippetVisualRow> mRows;

    void Reset();
};

struct FWatchFileManifestRowModel
{
    std::string mFilePath;
    EFileAction mAction = EFileAction::Modify;
    std::string mActionText;
    std::string mDescription;
};

struct FWatchFileManifestVisualRow
{
    std::string mFilePath;
    std::string mActionText;
    EFileAction mAction = EFileAction::Modify;
    std::string mDescription;
    bool mbContinuation = false;
};

struct FWatchFileManifestLayoutCache
{
    uint64_t mSnapshotGeneration = 0;
    std::string mTopicKey;
    int mPhaseIndex = -1;
    int mAvailableDescriptionCells = -1;
    int mBuildCount = 0;
    int mVisualBuildCount = 0;
    int mFileColumnWidth = 0;
    int mActionColumnWidth = 0;
    bool mbValid = false;
    std::vector<FWatchFileManifestRowModel> mRows;
    std::vector<FWatchFileManifestVisualRow> mVisualRows;

    void Reset();
};

struct FWatchTaxonomyLaneRowModel
{
    int mLaneIndex = 0;
    std::string mStatus;
    std::string mScope;
    std::string mExitCriteria;
};

struct FWatchTaxonomyJobRowModel
{
    int mJobIndex = 0;
    int mWave = 0;
    int mLane = 0;
    std::string mStatus;
    std::string mScope;
};

struct FWatchTaxonomyTaskRowModel
{
    int mJobIndex = 0;
    int mTaskIndex = 0;
    std::string mStatus;
    std::string mDescription;
    std::string mEvidence;
};

struct FWatchExecutionTaxonomyLayoutCache
{
    uint64_t mSnapshotGeneration = 0;
    std::string mTopicKey;
    int mPhaseIndex = -1;
    int mSelectedWaveIndex = -1;
    int mSelectedLaneIndex = -1;
    int mWaveFilter = -1;
    int mLaneFilter = -1;
    int mLanesDone = 0;
    int mJobsDone = 0;
    int mJobsActive = 0;
    int mJobsTodo = 0;
    int mTasksDone = 0;
    int mTasksActive = 0;
    int mTasksTodo = 0;
    int mVisibleJobCount = 0;
    int mVisibleTaskCount = 0;
    int mBuildCount = 0;
    bool mbValid = false;
    bool mbHasFlatTasks = false;
    std::vector<FWatchTaxonomyLaneRowModel> mLanes;
    std::vector<FWatchTaxonomyJobRowModel> mJobs;
    std::vector<FWatchTaxonomyTaskRowModel> mTasks;

    void Reset();
};

struct FWatchPanelRenderCache
{
    FWatchProseLayoutCache mPhaseDetails;
    FWatchPhaseListLayoutCache mPhaseList;
    FWatchCodeSnippetLayoutCache mCodeSnippets;
    FWatchFileManifestLayoutCache mFileManifest;
    FWatchExecutionTaxonomyLayoutCache mExecutionTaxonomy;

    void Reset();
};

class InventoryPanel
{
  public:
    ftxui::Element Render(const FWatchInventoryCounters &InCounters) const;
};

class ValidationPanel
{
  public:
    ftxui::Element Render(const FWatchValidationSummary &InValidation) const;
};

class LintPanel
{
  public:
    ftxui::Element Render(const FWatchLintSummary &InLint) const;
};

class ActivePlansPanel
{
  public:
    ftxui::Element Render(const std::vector<FWatchPlanSummary> &InPlans,
                          int InSelectedIndex,
                          FWatchScrollRegionState &InOutScrollState) const;
};

class PhaseListPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex, bool InMetricView,
                          FWatchScrollRegionState &InOutScrollState) const;
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex, bool InMetricView,
                          FWatchScrollRegionState &InOutScrollState,
                          FWatchPhaseListLayoutCache &InOutCache,
                          uint64_t InSnapshotGeneration) const;
};

class BlockersPanel
{
  public:
    ftxui::Element Render(const std::vector<BlockerItem> &InBlockers) const;
};

class NonActivePlansPanel
{
  public:
    ftxui::Element Render(const std::vector<FWatchPlanSummary> &InPlans,
                          int InSelectedIndex,
                          FWatchScrollRegionState &InOutScrollState) const;
};

class DeferredPlansPanel
{
  public:
    ftxui::Element Render(const std::vector<FWatchPlanSummary> &InPlans) const;
};

class ValidationFailPanel
{
  public:
    ftxui::Element Render(const FWatchValidationSummary &InValidation) const;
};

class ExecutionTaxonomyPanel
{
  public:
    // `InFocusMode` gates the full LANES / JOB BOARD / TASKS sub-panels.
    // Wired to the watch app's focus-mode toggle (`): in focus mode all
    // three render and the summary line still closes the panel; in default
    // mode only the summary line renders, keeping the right pane compact
    // for overview use. Lane / wave selection keybindings remain active in
    // both modes — they continue to drive the job-board filter state even
    // while the board itself is hidden.
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex, int InSelectedWaveIndex,
                          int InSelectedLaneIndex, bool InFocusMode,
                          FWatchScrollRegionState &InOutLaneScrollState) const;
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex, int InSelectedWaveIndex,
                          int InSelectedLaneIndex, bool InFocusMode,
                          FWatchScrollRegionState &InOutLaneScrollState,
                          FWatchExecutionTaxonomyLayoutCache &InOutCache,
                          uint64_t InSnapshotGeneration) const;
};

class FileManifestPanel
{
  public:
    ftxui::Element Render(const FPhaseTaxonomy &InTaxonomy,
                          FWatchScrollRegionState &InOutScrollState) const;
    ftxui::Element Render(const std::string &InTopicKey,
                          const FPhaseTaxonomy &InTaxonomy,
                          FWatchScrollRegionState &InOutScrollState,
                          FWatchFileManifestLayoutCache &InOutCache,
                          uint64_t InSnapshotGeneration) const;
};

class CodeSnippetPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex,
                          FWatchScrollRegionState &InOutScrollState) const;
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex,
                          FWatchScrollRegionState &InOutScrollState,
                          FWatchCodeSnippetLayoutCache &InOutCache,
                          uint64_t InSnapshotGeneration) const;
};

class PhaseDetailsPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex,
                          const FPhaseTaxonomy &InTaxonomy,
                          FWatchScrollRegionState &InOutScrollState) const;
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex,
                          const FPhaseTaxonomy &InTaxonomy,
                          FWatchScrollRegionState &InOutScrollState,
                          FWatchProseLayoutCache &InOutCache,
                          uint64_t InSnapshotGeneration) const;
};

class PlanDetailPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan) const;
};

class WatchStatusBar
{
  public:
    ftxui::Element
    Render(const std::string &InVersion, const std::string &InTime, int InTick,
           int InPollMs, const FWatchInventoryCounters &InCounters,
           const FDocWatchSnapshot::FPerformance &InPerformance) const;
};

} // namespace UniPlan
