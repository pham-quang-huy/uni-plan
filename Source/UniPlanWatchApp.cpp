#include "UniPlanWatchApp.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"
#include "UniPlanWatchPanels.h"
#include "UniPlanWatchSnapshot.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace UniPlan
{

DocWatchApp::DocWatchApp(const std::string &InRepoRoot,
                         const DocConfig &InConfig)
    : mRepoRoot(InRepoRoot), mConfig(InConfig),
      mbUseCache(InConfig.mbCacheEnabled)
{
}

DocWatchApp::~DocWatchApp()
{
    RequestStop();
    if (mDataThread.joinable())
    {
        mDataThread.join();
    }
}

void DocWatchApp::RequestStop()
{
    mRunning.store(false, std::memory_order_relaxed);
    mStopCondition.notify_all();
}

bool DocWatchApp::WaitForNextPoll()
{
    std::unique_lock<std::mutex> Lock(mStopMutex);
    mStopCondition.wait_for(
        Lock, std::chrono::seconds(3),
        [this] { return !mRunning.load(std::memory_order_relaxed); });
    return mRunning.load(std::memory_order_relaxed);
}

int DocWatchApp::Run()
{
    using namespace ftxui;

    auto screen = ScreenInteractive::Fullscreen();
    screen.TrackMouse(false);
    auto ExitLoop = screen.ExitLoopClosure();

    // Panels
    InventoryPanel PanelInventory;
    ValidationPanel PanelValidation;
    LintPanel PanelLint;
    ActivePlansPanel PanelActivePlans;
    PhaseDetailPanel PanelPhaseDetail;
    BlockersPanel PanelBlockers;
    NonActivePlansPanel PanelNonActive;
    ValidationFailPanel PanelValidationFail;
    ExecutionTaxonomyPanel PanelTaxonomy;
    FileManifestPanel PanelFileManifest;
    SchemaPanel PanelSchema;
    PlaybookChangeLogPanel PanelPBChangeLog;
    PlaybookVerificationPanel PanelPBVerification;
    PlanChangeLogPanel PanelPlanChangeLog;
    PlanVerificationPanel PanelPlanVerification;
    ImplChangeLogPanel PanelImplChangeLog;
    ImplVerificationPanel PanelImplVerification;
    PlanDetailPanel PanelPlanDetail;
    WatchStatusBar PanelStatusBar;

    auto dashboard = Renderer(
        [&]
        {
            // Resolve selected plan from whichever block is focused
            FWatchPlanSummary SelectedPlan;
            if (mbActiveBlockFocused && mSelectedPlanIndex >= 0 &&
                mSelectedPlanIndex <
                    static_cast<int>(mSnapshot.mActivePlans.size()))
            {
                SelectedPlan =
                    mSnapshot
                        .mActivePlans[static_cast<size_t>(mSelectedPlanIndex)];
            }
            else if (!mbActiveBlockFocused && mSelectedNonActiveIndex >= 0 &&
                     mSelectedNonActiveIndex <
                         static_cast<int>(mSnapshot.mNonActivePlans.size()))
            {
                SelectedPlan = mSnapshot.mNonActivePlans[static_cast<size_t>(
                    mSelectedNonActiveIndex)];
            }

            // ── LEFT PANE (navigator) ─────────────────────────────
            auto leftRow1 = PanelInventory.Render(mSnapshot.mInventory);
            auto leftRow2 = hbox({
                PanelValidation.Render(mSnapshot.mValidation) | flex,
                PanelLint.Render(mSnapshot.mLint) | flex,
            });
            auto leftRow3 = PanelActivePlans.Render(mSnapshot.mActivePlans,
                                                    mSelectedPlanIndex);
            auto leftRow4 = PanelNonActive.Render(mSnapshot.mNonActivePlans,
                                                  mSelectedNonActiveIndex);

            auto leftPane = vbox({
                leftRow1,
                leftRow2,
                leftRow3,
                leftRow4,
            });

            // Auto-select first in_progress phase if no phase is selected
            if (mSelectedPhaseIndex < 0 && !SelectedPlan.mPhases.empty())
            {
                for (int PhIdx = 0;
                     PhIdx < static_cast<int>(SelectedPlan.mPhases.size());
                     ++PhIdx)
                {
                    if (SelectedPlan.mPhases[static_cast<size_t>(PhIdx)]
                            .mStatus == EExecutionStatus::InProgress)
                    {
                        mSelectedPhaseIndex = PhIdx;
                        break;
                    }
                }
                if (mSelectedPhaseIndex < 0)
                {
                    mSelectedPhaseIndex = 0;
                }
            }

            // ── RIGHT PANE (detail for selected plan) ─────────────
            auto rightRow1 = PanelPhaseDetail.Render(
                SelectedPlan, mSelectedPhaseIndex, mbShowPhaseMetricView);
            auto rightRow2 = PanelTaxonomy.Render(
                SelectedPlan, mSelectedPhaseIndex, mSelectedWaveIndex,
                mSelectedLaneIndex, mbFocusMode);

            // Resolve selected phase taxonomy for file manifest
            FPhaseTaxonomy SelectedTaxonomy;
            if (mSelectedPhaseIndex >= 0 &&
                mSelectedPhaseIndex <
                    static_cast<int>(SelectedPlan.mPhases.size()))
            {
                for (const FPhaseTaxonomy &T : SelectedPlan.mPhaseTaxonomies)
                {
                    if (T.mPhaseIndex == mSelectedPhaseIndex)
                    {
                        SelectedTaxonomy = T;
                        break;
                    }
                }
            }
            auto rightRow3 =
                PanelFileManifest.Render(SelectedTaxonomy, mFilePageIndex);
            auto rightRow4 = PanelBlockers.Render(mSnapshot.mAllBlockers);
            auto rightRow5 = PanelValidationFail.Render(
                mSnapshot.mValidation.mFailedCheckDetails);

            auto rightRow0 = PanelPlanDetail.Render(SelectedPlan);

            // Focus mode drops the plan-detail header row and surfaces the
            // BLOCKERS + VALIDATION FAILURES row; default mode hides both
            // so the right pane stays compact for overview use.
            Elements RightPaneRows;
            if (!mbFocusMode)
            {
                RightPaneRows.push_back(rightRow0);
            }
            RightPaneRows.push_back(rightRow1);
            RightPaneRows.push_back(rightRow2);
            RightPaneRows.push_back(rightRow3);
            if (mbFocusMode)
            {
                RightPaneRows.push_back(hbox({
                    rightRow4 | flex,
                    rightRow5 | flex,
                }));
            }
            auto rightPane = vbox(std::move(RightPaneRows));

            // ── Status bar ────────────────────────────────────────
            auto bar = PanelStatusBar.Render(
                kCliVersion, mSnapshot.mSnapshotAtUTC, mTickCount,
                mSnapshot.mPollDurationMs, mSnapshot.mInventory,
                mSnapshot.mPerformance);

            // ── Title ─────────────────────────────────────────────
            const std::string Title =
                " UNI-PLAN WATCH  v" + std::string(kCliVersion) +
                "  \xe2\x97\x8f  " + mSnapshot.mRepoRoot + "  \xe2\x97\x8f  " +
                mSnapshot.mSnapshotAtUTC + " ";

            // ── Layout (2/3/4-pane with schema and/or impl) ─────
            const bool bHasSchema = mbShowSchemaPane;
            const bool bHasImpl = mbShowImplPane;
            const int LeftWidth = (bHasSchema && bHasImpl) ? 50 : 80;

            auto implPane = vbox({
                PanelPlanChangeLog.Render(SelectedPlan),
                PanelPlanVerification.Render(SelectedPlan),
                PanelImplChangeLog.Render(SelectedPlan),
                PanelImplVerification.Render(SelectedPlan),
                PanelPBChangeLog.Render(SelectedPlan, mSelectedPhaseIndex),
                PanelPBVerification.Render(SelectedPlan, mSelectedPhaseIndex),
            });

            // Compose horizontally. Focus mode drops the left overview pane;
            // the schema / impl middle panes only appear when their own
            // toggles (`s` / `i`) are on. Right pane is always present.
            Elements MainRow;
            if (!mbFocusMode)
            {
                MainRow.push_back(leftPane | size(WIDTH, EQUAL, LeftWidth));
                MainRow.push_back(separator());
            }
            if (bHasSchema)
            {
                MainRow.push_back(
                    PanelSchema.Render(SelectedPlan, mSelectedPhaseIndex) |
                    size(WIDTH, EQUAL, 120));
                MainRow.push_back(separator());
            }
            if (bHasImpl)
            {
                MainRow.push_back(implPane | size(WIDTH, EQUAL, 60));
                MainRow.push_back(separator());
            }
            MainRow.push_back(rightPane | flex);
            Element MainContent = hbox(std::move(MainRow));

            return vbox({
                       text(Title) | bold | hcenter | color(Color::Yellow),
                       MainContent | flex,
                       separator(),
                       bar,
                   }) |
                   border;
        });

    // Keyboard handling
    dashboard = CatchEvent(
        dashboard,
        [this, ExitLoop](Event InEvent)
        {
            if (InEvent == Event::q || InEvent == Event::Q ||
                InEvent == Event::Escape || InEvent == Event::CtrlC)
            {
                RequestStop();
                ExitLoop();
                return true;
            }
            if (InEvent == Event::Character('a'))
            {
                mbActiveBlockFocused = true;
                mSelectedNonActiveIndex = -1;
                mSelectedPhaseIndex = -1;
                mSelectedWaveIndex = -1;
                mSelectedLaneIndex = -1;
                if (!mSnapshot.mActivePlans.empty())
                {
                    mSelectedPlanIndex =
                        (mSelectedPlanIndex + 1) %
                        static_cast<int>(mSnapshot.mActivePlans.size());
                }
                return true;
            }
            if (InEvent == Event::Character('A'))
            {
                mbActiveBlockFocused = true;
                mSelectedNonActiveIndex = -1;
                mSelectedPhaseIndex = -1;
                mSelectedWaveIndex = -1;
                mSelectedLaneIndex = -1;
                if (!mSnapshot.mActivePlans.empty())
                {
                    mSelectedPlanIndex =
                        (mSelectedPlanIndex - 1 +
                         static_cast<int>(mSnapshot.mActivePlans.size())) %
                        static_cast<int>(mSnapshot.mActivePlans.size());
                }
                return true;
            }
            if (InEvent == Event::Character('n'))
            {
                mbActiveBlockFocused = false;
                mSelectedPlanIndex = -1;
                mSelectedPhaseIndex = -1;
                mSelectedWaveIndex = -1;
                mSelectedLaneIndex = -1;
                if (!mSnapshot.mNonActivePlans.empty())
                {
                    if (mSelectedNonActiveIndex < 0)
                    {
                        mSelectedNonActiveIndex = -1;
                    }
                    mSelectedNonActiveIndex =
                        (mSelectedNonActiveIndex + 1) %
                        static_cast<int>(mSnapshot.mNonActivePlans.size());
                }
                return true;
            }
            if (InEvent == Event::Character('N'))
            {
                mbActiveBlockFocused = false;
                mSelectedPlanIndex = -1;
                mSelectedPhaseIndex = -1;
                mSelectedWaveIndex = -1;
                mSelectedLaneIndex = -1;
                if (!mSnapshot.mNonActivePlans.empty())
                {
                    if (mSelectedNonActiveIndex < 0)
                    {
                        mSelectedNonActiveIndex =
                            static_cast<int>(mSnapshot.mNonActivePlans.size());
                    }
                    mSelectedNonActiveIndex =
                        (mSelectedNonActiveIndex - 1 +
                         static_cast<int>(mSnapshot.mNonActivePlans.size())) %
                        static_cast<int>(mSnapshot.mNonActivePlans.size());
                }
                return true;
            }
            // Resolve selected plan for keyboard navigation
            FWatchPlanSummary KeySelectedPlan;
            if (mbActiveBlockFocused && mSelectedPlanIndex >= 0 &&
                mSelectedPlanIndex <
                    static_cast<int>(mSnapshot.mActivePlans.size()))
            {
                KeySelectedPlan =
                    mSnapshot
                        .mActivePlans[static_cast<size_t>(mSelectedPlanIndex)];
            }
            else if (!mbActiveBlockFocused && mSelectedNonActiveIndex >= 0 &&
                     mSelectedNonActiveIndex <
                         static_cast<int>(mSnapshot.mNonActivePlans.size()))
            {
                KeySelectedPlan = mSnapshot.mNonActivePlans[static_cast<size_t>(
                    mSelectedNonActiveIndex)];
            }

            if (InEvent == Event::Character('p'))
            {
                const int PhaseCount =
                    static_cast<int>(KeySelectedPlan.mPhases.size());
                if (PhaseCount > 0)
                {
                    mSelectedPhaseIndex =
                        (mSelectedPhaseIndex + 1) % PhaseCount;
                    mSelectedWaveIndex = -1;
                    mSelectedLaneIndex = -1;
                    mFilePageIndex = 0;
                }
                return true;
            }
            if (InEvent == Event::Character('P'))
            {
                const int PhaseCount =
                    static_cast<int>(KeySelectedPlan.mPhases.size());
                if (PhaseCount > 0)
                {
                    if (mSelectedPhaseIndex < 0)
                    {
                        mSelectedPhaseIndex = PhaseCount;
                    }
                    mSelectedPhaseIndex =
                        (mSelectedPhaseIndex - 1 + PhaseCount) % PhaseCount;
                    mSelectedLaneIndex = -1;
                    mFilePageIndex = 0;
                }
                return true;
            }

            if (InEvent == Event::Character('w'))
            {
                const FPhaseTaxonomy *rpTax = nullptr;
                if (mSelectedPhaseIndex >= 0 &&
                    mSelectedPhaseIndex <
                        static_cast<int>(KeySelectedPlan.mPhases.size()))
                {

                    for (const FPhaseTaxonomy &T :
                         KeySelectedPlan.mPhaseTaxonomies)
                    {
                        if (T.mPhaseIndex == mSelectedPhaseIndex)
                        {
                            rpTax = &T;
                            break;
                        }
                    }
                }
                if (rpTax != nullptr && rpTax->mWaveCount > 0)
                {
                    mSelectedWaveIndex =
                        (mSelectedWaveIndex + 1) % rpTax->mWaveCount;
                }
                return true;
            }
            if (InEvent == Event::Character('W'))
            {
                const FPhaseTaxonomy *rpTax = nullptr;
                if (mSelectedPhaseIndex >= 0 &&
                    mSelectedPhaseIndex <
                        static_cast<int>(KeySelectedPlan.mPhases.size()))
                {

                    for (const FPhaseTaxonomy &T :
                         KeySelectedPlan.mPhaseTaxonomies)
                    {
                        if (T.mPhaseIndex == mSelectedPhaseIndex)
                        {
                            rpTax = &T;
                            break;
                        }
                    }
                }
                if (rpTax != nullptr && rpTax->mWaveCount > 0)
                {
                    mSelectedWaveIndex =
                        (mSelectedWaveIndex - 1 + rpTax->mWaveCount) %
                        rpTax->mWaveCount;
                }
                return true;
            }
            if (InEvent == Event::Character('l'))
            {
                const FPhaseTaxonomy *rpTax = nullptr;
                if (mSelectedPhaseIndex >= 0 &&
                    mSelectedPhaseIndex <
                        static_cast<int>(KeySelectedPlan.mPhases.size()))
                {

                    for (const FPhaseTaxonomy &T :
                         KeySelectedPlan.mPhaseTaxonomies)
                    {
                        if (T.mPhaseIndex == mSelectedPhaseIndex)
                        {
                            rpTax = &T;
                            break;
                        }
                    }
                }
                if (rpTax != nullptr && !rpTax->mLanes.empty())
                {
                    mSelectedLaneIndex = (mSelectedLaneIndex + 1) %
                                         static_cast<int>(rpTax->mLanes.size());
                }
                return true;
            }
            if (InEvent == Event::Character('L'))
            {
                const FPhaseTaxonomy *rpTax = nullptr;
                if (mSelectedPhaseIndex >= 0 &&
                    mSelectedPhaseIndex <
                        static_cast<int>(KeySelectedPlan.mPhases.size()))
                {

                    for (const FPhaseTaxonomy &T :
                         KeySelectedPlan.mPhaseTaxonomies)
                    {
                        if (T.mPhaseIndex == mSelectedPhaseIndex)
                        {
                            rpTax = &T;
                            break;
                        }
                    }
                }
                if (rpTax != nullptr && !rpTax->mLanes.empty())
                {
                    const int LaneCount =
                        static_cast<int>(rpTax->mLanes.size());
                    mSelectedLaneIndex =
                        (mSelectedLaneIndex - 1 + LaneCount) % LaneCount;
                }
                return true;
            }
            if (InEvent == Event::Character('f'))
            {
                const FPhaseTaxonomy *rpTax = nullptr;
                if (mSelectedPhaseIndex >= 0 &&
                    mSelectedPhaseIndex <
                        static_cast<int>(KeySelectedPlan.mPhases.size()))
                {

                    for (const FPhaseTaxonomy &T :
                         KeySelectedPlan.mPhaseTaxonomies)
                    {
                        if (T.mPhaseIndex == mSelectedPhaseIndex)
                        {
                            rpTax = &T;
                            break;
                        }
                    }
                }
                if (rpTax != nullptr && !rpTax->mFileManifest.empty())
                {
                    const int PageCount =
                        (static_cast<int>(rpTax->mFileManifest.size()) + 9) /
                        10;
                    mFilePageIndex = (mFilePageIndex + 1) % PageCount;
                }
                return true;
            }
            if (InEvent == Event::Character('F'))
            {
                const FPhaseTaxonomy *rpTax = nullptr;
                if (mSelectedPhaseIndex >= 0 &&
                    mSelectedPhaseIndex <
                        static_cast<int>(KeySelectedPlan.mPhases.size()))
                {

                    for (const FPhaseTaxonomy &T :
                         KeySelectedPlan.mPhaseTaxonomies)
                    {
                        if (T.mPhaseIndex == mSelectedPhaseIndex)
                        {
                            rpTax = &T;
                            break;
                        }
                    }
                }
                if (rpTax != nullptr && !rpTax->mFileManifest.empty())
                {
                    const int PageCount =
                        (static_cast<int>(rpTax->mFileManifest.size()) + 9) /
                        10;
                    mFilePageIndex =
                        (mFilePageIndex - 1 + PageCount) % PageCount;
                }
                return true;
            }
            if (InEvent == Event::Character('s'))
            {
                mbShowSchemaPane = !mbShowSchemaPane;
                return true;
            }
            if (InEvent == Event::Character('i'))
            {
                mbShowImplPane = !mbShowImplPane;
                return true;
            }
            if (InEvent == Event::Character('d'))
            {
                mbShowPhaseMetricView = !mbShowPhaseMetricView;
                return true;
            }
            if (InEvent == Event::Character('`'))
            {
                mbFocusMode = !mbFocusMode;
                return true;
            }
            if (InEvent == Event::Character('r'))
            {
                mbForceRefresh.store(true, std::memory_order_relaxed);
                return true;
            }
            return false;
        });

    // Background data collection thread
    mRunning.store(true, std::memory_order_relaxed);
    mDataThread = std::thread(
        [this, &screen]
        {
            bool bFirstTick = true;
            while (mRunning.load(std::memory_order_relaxed))
            {
                const bool bForceRefresh =
                    mbForceRefresh.exchange(false, std::memory_order_relaxed);

                // Full snapshot rebuild
                FDocWatchSnapshot Fresh;
                try
                {
                    Fresh = BuildWatchSnapshotCached(
                        mRepoRoot, mbUseCache, mConfig.mCacheDir,
                        mConfig.mbCacheVerbose, mSnapshotCache,
                        bForceRefresh || bFirstTick);
                }
                catch (const std::exception &Ex)
                {
                    std::cerr << "[watch] snapshot error: " << Ex.what()
                              << "\n";
                    if (!WaitForNextPoll())
                    {
                        break;
                    }
                    continue;
                }

                if (!mRunning.load(std::memory_order_relaxed))
                {
                    break;
                }
                bFirstTick = false;

                // Post the fresh snapshot back to the UI thread.
                //
                // Pair the state-updating Closure with a follow-up
                // Event::Custom. FTXUI's ScreenInteractive::HandleTask
                // only marks the frame invalid for Event tasks — Closure
                // tasks run and return without touching `frame_valid_`,
                // so a bare Post() leaves the screen frozen on its
                // prior render. Posting Event::Custom after the Closure
                // forces the next Draw() pass to re-render against the
                // just-updated mSnapshot.
                screen.Post(
                    [this, Snap = std::move(Fresh)]() mutable
                    {
                        mSnapshot = std::move(Snap);
                        mTickCount++;
                    });
                screen.PostEvent(ftxui::Event::Custom);

                if (!WaitForNextPoll())
                {
                    break;
                }
            }
        });

    screen.Loop(dashboard);

    RequestStop();
    if (mDataThread.joinable())
    {
        mDataThread.join();
    }

    return 0;
}

int RunDocWatch(const std::string &InRepoRoot, const DocConfig &InConfig)
{
    DocWatchApp App(InRepoRoot, InConfig);
    return App.Run();
}

} // namespace UniPlan
