#include "UniPlanWatchApp.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"
#include "UniPlanWatchPanels.h"
#include "UniPlanWatchSnapshot.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace UniPlan
{

namespace
{
static constexpr int kNavigatorPaneWidth = 50;
static constexpr int kMiddlePaneWidthWithCode = 150;

class EqualHeightFlexNode : public ftxui::Node
{
  public:
    explicit EqualHeightFlexNode(ftxui::Element InChild)
        : Node(ftxui::Elements{std::move(InChild)})
    {
    }

    void ComputeRequirement() override
    {
        children_[0]->ComputeRequirement();
        requirement_ = children_[0]->requirement();
        requirement_.min_y = 0;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_y = 1;
    }

    void SetBox(ftxui::Box InBox) override
    {
        Node::SetBox(InBox);
        children_[0]->SetBox(InBox);
    }

    void Render(ftxui::Screen &InScreen) override
    {
        children_[0]->Render(InScreen);
    }
};

static ftxui::Decorator EqualHeightFlex()
{
    return [](ftxui::Element InChild) -> ftxui::Element
    {
        return std::make_shared<EqualHeightFlexNode>(std::move(InChild));
    };
}
} // namespace

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
    CodeSnippetPanel PanelCodeSnippet;
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
                                                    mSelectedPlanIndex,
                                                    mScrollState.mActivePlans);
            auto leftRow4 = PanelNonActive.Render(mSnapshot.mNonActivePlans,
                                                  mSelectedNonActiveIndex,
                                                  mScrollState.mNonActivePlans);

            auto leftPane = vbox({
                leftRow1,
                leftRow2,
                leftRow3 | EqualHeightFlex(),
                leftRow4 | EqualHeightFlex(),
            });

            // Auto-select the newest in_progress phase; otherwise select the
            // newest phase so the descending detail panel opens at the top.
            if (mSelectedPhaseIndex < 0 && !SelectedPlan.mPhases.empty())
            {
                for (int PhIdx =
                         static_cast<int>(SelectedPlan.mPhases.size()) - 1;
                     PhIdx >= 0; --PhIdx)
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
                    mSelectedPhaseIndex =
                        static_cast<int>(SelectedPlan.mPhases.size()) - 1;
                }
            }

            // ── RIGHT PANE (detail for selected plan) ─────────────
            auto rightRow1 = PanelPhaseDetail.Render(
                SelectedPlan, mSelectedPhaseIndex, mbShowPhaseMetricView,
                mScrollState.mPhaseDetail);
            auto rightRow2 = PanelTaxonomy.Render(
                SelectedPlan, mSelectedPhaseIndex, mSelectedWaveIndex,
                mSelectedLaneIndex, mbFocusMode, mScrollState.mLanes);

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
                PanelFileManifest.Render(SelectedTaxonomy,
                                         mScrollState.mFileManifest);
            auto rightRow4 = PanelBlockers.Render(mSnapshot.mAllBlockers);
            auto rightRow5 = PanelValidationFail.Render(mSnapshot.mValidation);

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

            // ── Layout ───────────────────────────────────────────
            // Compose horizontally. Focus mode drops the left overview pane;
            // the right pane is always present.
            Elements MainRow;
            if (!mbFocusMode)
            {
                MainRow.push_back(leftPane |
                                  size(WIDTH, EQUAL, kNavigatorPaneWidth));
                MainRow.push_back(separator());
            }
            if (mbShowCodePane)
            {
                auto codePane = PanelCodeSnippet.Render(
                    SelectedPlan, mSelectedPhaseIndex,
                    mScrollState.mCodeSnippets);
                MainRow.push_back(rightPane |
                                  size(WIDTH, EQUAL,
                                       kMiddlePaneWidthWithCode));
                MainRow.push_back(separator());
                MainRow.push_back(codePane | flex);
            }
            else
            {
                MainRow.push_back(rightPane | flex);
            }
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
                mScrollState.mPhaseDetail.Reset();
                mScrollState.mLanes.Reset();
                mScrollState.mFileManifest.Reset();
                mScrollState.mCodeSnippets.Reset();
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
                mScrollState.mPhaseDetail.Reset();
                mScrollState.mLanes.Reset();
                mScrollState.mFileManifest.Reset();
                mScrollState.mCodeSnippets.Reset();
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
                mScrollState.mPhaseDetail.Reset();
                mScrollState.mLanes.Reset();
                mScrollState.mFileManifest.Reset();
                mScrollState.mCodeSnippets.Reset();
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
                mScrollState.mPhaseDetail.Reset();
                mScrollState.mLanes.Reset();
                mScrollState.mFileManifest.Reset();
                mScrollState.mCodeSnippets.Reset();
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
                    mScrollState.mLanes.Reset();
                    mScrollState.mFileManifest.Reset();
                    mScrollState.mCodeSnippets.Reset();
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
                    mScrollState.mLanes.Reset();
                    mScrollState.mFileManifest.Reset();
                    mScrollState.mCodeSnippets.Reset();
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
                    ++mScrollState.mFileManifest.mOffset;
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
                    if (mScrollState.mFileManifest.mOffset > 0)
                    {
                        --mScrollState.mFileManifest.mOffset;
                    }
                }
                return true;
            }
            if (InEvent == Event::Character('d'))
            {
                mbShowPhaseMetricView = !mbShowPhaseMetricView;
                return true;
            }
            if (InEvent == Event::F12)
            {
                mbShowCodePane = !mbShowCodePane;
                if (mbShowCodePane)
                {
                    mScrollState.mCodeSnippets.Reset();
                }
                return true;
            }
            if (mbShowCodePane && InEvent == Event::Character('['))
            {
                if (mScrollState.mCodeSnippets.mOffset > 0)
                {
                    --mScrollState.mCodeSnippets.mOffset;
                }
                return true;
            }
            if (mbShowCodePane && InEvent == Event::Character(']'))
            {
                ++mScrollState.mCodeSnippets.mOffset;
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
            const auto PostSnapshot =
                [this, &screen](FDocWatchSnapshot &&InSnapshot)
            {
                screen.Post(
                    [this, Snap = std::move(InSnapshot)]() mutable
                    {
                        mSnapshot = std::move(Snap);
                        mTickCount++;
                    });
                screen.PostEvent(ftxui::Event::Custom);
            };

            bool bFirstTick = true;
            while (mRunning.load(std::memory_order_relaxed))
            {
                const bool bForceRefresh =
                    mbForceRefresh.exchange(false, std::memory_order_relaxed);

                // Build an inventory-first snapshot before expensive
                // validators. This lets the UI show real plan counts and
                // navigation while validation/lint catch up in the same
                // background thread.
                FDocWatchSnapshot Fresh;
                try
                {
                    FWatchSnapshotBuildOptions FastOptions;
                    FastOptions.mbRunValidation = false;
                    FastOptions.mbRunLint = false;
                    FastOptions.mbMarkSkippedValidationRunning = true;
                    FastOptions.mbMarkSkippedLintRunning = true;
                    Fresh = BuildWatchSnapshotCached(
                        mRepoRoot, mbUseCache, mConfig.mCacheDir,
                        mConfig.mbCacheVerbose, mSnapshotCache,
                        bForceRefresh || bFirstTick, FastOptions);
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

                const bool bNeedsValidation =
                    Fresh.mValidation.mState !=
                    FWatchValidationSummary::EState::Ready;
                const bool bNeedsLint =
                    Fresh.mLint.mState != FWatchLintSummary::EState::Ready;

                // Post the inventory-first snapshot back to the UI thread.
                //
                // Pair the state-updating Closure with a follow-up
                // Event::Custom. FTXUI's ScreenInteractive::HandleTask
                // only marks the frame invalid for Event tasks — Closure
                // tasks run and return without touching `frame_valid_`,
                // so a bare Post() leaves the screen frozen on its
                // prior render. Posting Event::Custom after the Closure
                // forces the next Draw() pass to re-render against the
                // just-updated mSnapshot.
                PostSnapshot(std::move(Fresh));

                if ((bNeedsValidation || bNeedsLint) &&
                    mRunning.load(std::memory_order_relaxed))
                {
                    FDocWatchSnapshot Full;
                    try
                    {
                        Full = BuildWatchSnapshotCached(
                            mRepoRoot, mbUseCache, mConfig.mCacheDir,
                            mConfig.mbCacheVerbose, mSnapshotCache,
                            /*InForceRefresh=*/false);
                    }
                    catch (const std::exception &Ex)
                    {
                        std::cerr << "[watch] validation snapshot error: "
                                  << Ex.what() << "\n";
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
                    PostSnapshot(std::move(Full));
                }

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
