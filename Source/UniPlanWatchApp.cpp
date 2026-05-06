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

#include <array>
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
static constexpr int kMiddlePaneWidthWithSidePane = 150;

struct FWatchSidePaneBinding
{
    EWatchSidePane mPane = EWatchSidePane::None;
    ftxui::Event mToggleEvent = ftxui::Event::Custom;
};

static const std::array<FWatchSidePaneBinding, 3> &WatchSidePaneBindings()
{
    static const std::array<FWatchSidePaneBinding, 3> Bindings = {{
        {EWatchSidePane::PhaseDetails, ftxui::Event::F5},
        {EWatchSidePane::FileManifest, ftxui::Event::F6},
        {EWatchSidePane::CodeSnippets, ftxui::Event::F12},
    }};
    return Bindings;
}

static const FWatchSidePaneBinding *
FindWatchSidePaneToggle(const ftxui::Event &InEvent)
{
    for (const FWatchSidePaneBinding &Binding : WatchSidePaneBindings())
    {
        if (Binding.mToggleEvent == InEvent)
        {
            return &Binding;
        }
    }
    return nullptr;
}

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

const FWatchPlanSummary *DocWatchApp::ResolveSelectedPlan() const
{
    if (mbActiveBlockFocused && mSelectedPlanIndex >= 0 &&
        mSelectedPlanIndex < static_cast<int>(mSnapshot.mActivePlans.size()))
    {
        return &mSnapshot.mActivePlans[static_cast<size_t>(mSelectedPlanIndex)];
    }
    if (!mbActiveBlockFocused && mSelectedNonActiveIndex >= 0 &&
        mSelectedNonActiveIndex <
            static_cast<int>(mSnapshot.mNonActivePlans.size()))
    {
        return &mSnapshot.mNonActivePlans[static_cast<size_t>(
            mSelectedNonActiveIndex)];
    }
    return nullptr;
}

const FPhaseTaxonomy *DocWatchApp::ResolveSelectedPhaseTaxonomy(
    const FWatchPlanSummary &InPlan) const
{
    if (mInteraction.mSelectedPhaseIndex < 0 ||
        mInteraction.mSelectedPhaseIndex >=
            static_cast<int>(InPlan.mPhases.size()))
    {
        return nullptr;
    }

    for (const FPhaseTaxonomy &Taxonomy : InPlan.mPhaseTaxonomies)
    {
        if (Taxonomy.mPhaseIndex == mInteraction.mSelectedPhaseIndex)
        {
            return &Taxonomy;
        }
    }
    return nullptr;
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
    PhaseListPanel PanelPhaseList;
    BlockersPanel PanelBlockers;
    NonActivePlansPanel PanelNonActive;
    ValidationFailPanel PanelValidationFail;
    ExecutionTaxonomyPanel PanelTaxonomy;
    FileManifestPanel PanelFileManifest;
    CodeSnippetPanel PanelCodeSnippet;
    PhaseDetailsPanel PanelPhaseDetails;
    PlanDetailPanel PanelPlanDetail;
    WatchStatusBar PanelStatusBar;

    auto dashboard = Renderer(
        [&]
        {
            const FWatchPlanSummary EmptyPlan;
            const FPhaseTaxonomy EmptyTaxonomy;
            const FWatchPlanSummary *rpSelectedPlan = ResolveSelectedPlan();
            const FWatchPlanSummary &SelectedPlan =
                rpSelectedPlan == nullptr ? EmptyPlan : *rpSelectedPlan;

            // Auto-select the newest in_progress phase; otherwise select the
            // newest phase so the descending detail panel opens at the top.
            if (mInteraction.mSelectedPhaseIndex < 0 &&
                !SelectedPlan.mPhases.empty())
            {
                for (int PhIdx =
                         static_cast<int>(SelectedPlan.mPhases.size()) - 1;
                     PhIdx >= 0; --PhIdx)
                {
                    if (SelectedPlan.mPhases[static_cast<size_t>(PhIdx)]
                            .mStatus == EExecutionStatus::InProgress)
                    {
                        mInteraction.mSelectedPhaseIndex = PhIdx;
                        break;
                    }
                }
                if (mInteraction.mSelectedPhaseIndex < 0)
                {
                    mInteraction.mSelectedPhaseIndex =
                        static_cast<int>(SelectedPlan.mPhases.size()) - 1;
                }
            }

            // ── RIGHT PANE (detail for selected plan) ─────────────
            auto rightRow1 = PanelPhaseList.Render(
                SelectedPlan, mInteraction.mSelectedPhaseIndex,
                mbShowPhaseMetricView, mInteraction.mScrollState.mPhaseList,
                mPanelRenderCache.mPhaseList, mSnapshotGeneration);

            // Resolve selected phase taxonomy for phase-scoped detail panels.
            const FPhaseTaxonomy *rpSelectedTaxonomy =
                ResolveSelectedPhaseTaxonomy(SelectedPlan);
            const FPhaseTaxonomy &SelectedTaxonomy =
                rpSelectedTaxonomy == nullptr ? EmptyTaxonomy
                                              : *rpSelectedTaxonomy;

            auto rightRow2 = PanelTaxonomy.Render(
                SelectedPlan, mInteraction.mSelectedPhaseIndex,
                mInteraction.mSelectedWaveIndex,
                mInteraction.mSelectedLaneIndex, mbFocusMode,
                mInteraction.mScrollState.mLanes,
                mPanelRenderCache.mExecutionTaxonomy, mSnapshotGeneration);
            // Focus mode drops the plan-detail header row and surfaces the
            // BLOCKERS + VALIDATION FAILURES row; default mode hides both
            // so the right pane stays compact for overview use.
            Elements RightPaneRows;
            if (!mbFocusMode)
            {
                RightPaneRows.push_back(PanelPlanDetail.Render(SelectedPlan));
            }
            RightPaneRows.push_back(rightRow1);
            RightPaneRows.push_back(rightRow2);
            if (mbFocusMode)
            {
                RightPaneRows.push_back(hbox({
                    PanelBlockers.Render(mSnapshot.mAllBlockers) | flex,
                    PanelValidationFail.Render(mSnapshot.mValidation) | flex,
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
                auto LeftPane = vbox({
                    PanelInventory.Render(mSnapshot.mInventory),
                    hbox({
                        PanelValidation.Render(mSnapshot.mValidation) | flex,
                        PanelLint.Render(mSnapshot.mLint) | flex,
                    }),
                    PanelActivePlans.Render(mSnapshot.mActivePlans,
                                            mSelectedPlanIndex,
                                            mInteraction.mScrollState
                                                .mActivePlans) |
                        EqualHeightFlex(),
                    PanelNonActive.Render(mSnapshot.mNonActivePlans,
                                          mSelectedNonActiveIndex,
                                          mInteraction.mScrollState
                                              .mNonActivePlans) |
                        EqualHeightFlex(),
                });
                MainRow.push_back(LeftPane |
                                  size(WIDTH, EQUAL, kNavigatorPaneWidth));
                MainRow.push_back(separator());
            }
            if (mInteraction.mSidePane != EWatchSidePane::None)
            {
                Element SidePane;
                if (mInteraction.mSidePane == EWatchSidePane::PhaseDetails)
                {
                    SidePane = PanelPhaseDetails.Render(
                        SelectedPlan, mInteraction.mSelectedPhaseIndex,
                        SelectedTaxonomy,
                        mInteraction.mScrollState.mPhaseDetails,
                        mPanelRenderCache.mPhaseDetails, mSnapshotGeneration);
                }
                else if (mInteraction.mSidePane ==
                         EWatchSidePane::FileManifest)
                {
                    SidePane = PanelFileManifest.Render(
                        SelectedPlan.mTopicKey, SelectedTaxonomy,
                        mInteraction.mScrollState.mFileManifest,
                        mPanelRenderCache.mFileManifest, mSnapshotGeneration);
                }
                else
                {
                    SidePane = PanelCodeSnippet.Render(
                        SelectedPlan, mInteraction.mSelectedPhaseIndex,
                        mInteraction.mScrollState.mCodeSnippets,
                        mPanelRenderCache.mCodeSnippets,
                        mSnapshotGeneration);
                }
                MainRow.push_back(rightPane |
                                  size(WIDTH, EQUAL,
                                       kMiddlePaneWidthWithSidePane));
                MainRow.push_back(separator());
                MainRow.push_back(SidePane | flex);
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
                ResetWatchPlanScopedScroll(mInteraction);
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
                ResetWatchPlanScopedScroll(mInteraction);
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
                ResetWatchPlanScopedScroll(mInteraction);
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
                ResetWatchPlanScopedScroll(mInteraction);
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
            const FWatchPlanSummary *rpKeySelectedPlan =
                ResolveSelectedPlan();
            const auto ResolveKeyTaxonomy =
                [this, rpKeySelectedPlan]() -> const FPhaseTaxonomy *
            {
                return rpKeySelectedPlan == nullptr
                           ? nullptr
                           : ResolveSelectedPhaseTaxonomy(*rpKeySelectedPlan);
            };

            if (InEvent == Event::Character('p'))
            {
                const int PhaseCount =
                    rpKeySelectedPlan == nullptr
                        ? 0
                        : static_cast<int>(rpKeySelectedPlan->mPhases.size());
                StepWatchPhaseSelection(mInteraction, PhaseCount, -1);
                return true;
            }
            if (InEvent == Event::Character('P'))
            {
                const int PhaseCount =
                    rpKeySelectedPlan == nullptr
                        ? 0
                        : static_cast<int>(rpKeySelectedPlan->mPhases.size());
                StepWatchPhaseSelection(mInteraction, PhaseCount, 1);
                return true;
            }

            if (InEvent == Event::Character('w'))
            {
                const FPhaseTaxonomy *rpTax = ResolveKeyTaxonomy();
                if (rpTax != nullptr && rpTax->mWaveCount > 0)
                {
                    mInteraction.mSelectedWaveIndex =
                        (mInteraction.mSelectedWaveIndex + 1) %
                        rpTax->mWaveCount;
                }
                return true;
            }
            if (InEvent == Event::Character('W'))
            {
                const FPhaseTaxonomy *rpTax = ResolveKeyTaxonomy();
                if (rpTax != nullptr && rpTax->mWaveCount > 0)
                {
                    mInteraction.mSelectedWaveIndex =
                        (mInteraction.mSelectedWaveIndex - 1 +
                         rpTax->mWaveCount) %
                        rpTax->mWaveCount;
                }
                return true;
            }
            if (InEvent == Event::Character('l'))
            {
                const FPhaseTaxonomy *rpTax = ResolveKeyTaxonomy();
                if (rpTax != nullptr && !rpTax->mLanes.empty())
                {
                    mInteraction.mSelectedLaneIndex =
                        (mInteraction.mSelectedLaneIndex + 1) %
                        static_cast<int>(rpTax->mLanes.size());
                }
                return true;
            }
            if (InEvent == Event::Character('L'))
            {
                const FPhaseTaxonomy *rpTax = ResolveKeyTaxonomy();
                if (rpTax != nullptr && !rpTax->mLanes.empty())
                {
                    const int LaneCount =
                        static_cast<int>(rpTax->mLanes.size());
                    mInteraction.mSelectedLaneIndex =
                        (mInteraction.mSelectedLaneIndex - 1 + LaneCount) %
                        LaneCount;
                }
                return true;
            }
            if (InEvent == Event::Character('d'))
            {
                mbShowPhaseMetricView = !mbShowPhaseMetricView;
                return true;
            }
            if (const FWatchSidePaneBinding *rpBinding =
                    FindWatchSidePaneToggle(InEvent))
            {
                ToggleWatchSidePane(mInteraction, rpBinding->mPane);
                return true;
            }
            if (InEvent == Event::Character('['))
            {
                return ScrollWatchSidePane(mInteraction, -1);
            }
            if (InEvent == Event::Character(']'))
            {
                return ScrollWatchSidePane(mInteraction, 1);
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
                        ++mSnapshotGeneration;
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
