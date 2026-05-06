#pragma once

#include "UniPlanTypes.h"
#include "UniPlanWatchScroll.h"
#include "UniPlanWatchSnapshot.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace UniPlan
{

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

    std::string mRepoRoot;
    DocConfig mConfig;
    bool mbUseCache = true;

    FDocWatchSnapshot mSnapshot{};
    FWatchSnapshotCache mSnapshotCache{};
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
    bool mbShowCodePane = false;
    std::atomic<bool> mbForceRefresh{false};
};

int RunDocWatch(const std::string &InRepoRoot, const DocConfig &InConfig);

} // namespace UniPlan
