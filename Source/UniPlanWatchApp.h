#pragma once

#include "UniPlanTypes.h"
#include "UniPlanWatchPanels.h"
#include "UniPlanWatchScroll.h"
#include "UniPlanWatchSnapshot.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace UniPlan
{

enum class EWatchSidePane : uint8_t
{
    None,
    PhaseDetails,
    CodeSnippets
};

class DocWatchApp
{
  public:
    explicit DocWatchApp(const std::string &InRepoRoot,
                         const DocConfig &InConfig);
    ~DocWatchApp();

    DocWatchApp(const DocWatchApp &) = delete;
    DocWatchApp &operator=(const DocWatchApp &) = delete;

    int Run();

  private:
    void RequestStop();
    bool WaitForNextPoll();
    void StepPhaseSelection(int InPhaseCount, int InDelta);
    void ResetPlanScopedScroll();
    void ResetPhaseScopedScroll();
    void ResetSelectedPhaseDependentScroll();
    void ResetSidePaneScroll();
    void ToggleSidePane(EWatchSidePane InPane);
    bool ScrollCurrentSidePane(int InDelta);
    const FWatchPlanSummary *ResolveSelectedPlan() const;
    const FPhaseTaxonomy *
    ResolveSelectedPhaseTaxonomy(const FWatchPlanSummary &InPlan) const;

    std::string mRepoRoot;
    DocConfig mConfig;
    bool mbUseCache = true;

    FDocWatchSnapshot mSnapshot{};
    FWatchSnapshotCache mSnapshotCache{};
    FWatchPanelRenderCache mPanelRenderCache{};
    uint64_t mSnapshotGeneration = 0;
    std::atomic<bool> mRunning{false};
    std::thread mDataThread;
    std::mutex mStopMutex;
    std::condition_variable mStopCondition;
    int mTickCount = 0;
    int mSelectedPlanIndex = 0;
    int mSelectedNonActiveIndex = -1;
    FWatchScrollState mScrollState{};
    bool mbActiveBlockFocused = true;
    int mSelectedPhaseIndex = -1;
    int mSelectedWaveIndex = -1;
    int mSelectedLaneIndex = -1;
    bool mbShowPhaseMetricView = false;
    // Focus mode (toggled via `): hides the left overview pane and the plan
    // detail row of the right pane to give execution detail more space.
    bool mbFocusMode = false;
    EWatchSidePane mSidePane = EWatchSidePane::None;
    std::atomic<bool> mbForceRefresh{false};
};

int RunDocWatch(const std::string &InRepoRoot, const DocConfig &InConfig);

} // namespace UniPlan
