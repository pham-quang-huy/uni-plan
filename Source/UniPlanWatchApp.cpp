#include "UniPlanWatchApp.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"
#include "UniPlanWatchPanels.h"
#include "UniPlanWatchSnapshot.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace UniPlan {

DocWatchApp::DocWatchApp(const std::string &InRepoRoot,
                         const DocConfig &InConfig)
    : mRepoRoot(InRepoRoot), mConfig(InConfig),
      mbUseCache(InConfig.mbCacheEnabled) {}

DocWatchApp::~DocWatchApp() {
  mRunning.store(false, std::memory_order_relaxed);
  if (mDataThread.joinable()) {
    mDataThread.join();
  }
}

int DocWatchApp::Run() {
  using namespace ftxui;

  auto screen = ScreenInteractive::Fullscreen();

  // Panels
  InventoryPanel PanelInventory;
  ValidationPanel PanelValidation;
  LintPanel PanelLint;
  ActivePlansPanel PanelActivePlans;
  PhaseDetailPanel PanelPhaseDetail;
  BlockersPanel PanelBlockers;
  CompletedPlansPanel PanelCompleted;
  ValidationFailPanel PanelValidationFail;
  ExecutionTaxonomyPanel PanelTaxonomy;
  FileManifestPanel PanelFileManifest;
  SchemaPanel PanelSchema;
  PlaybookChangeLogPanel PanelPBChangeLog;
  PlaybookVerificationPanel PanelPBVerification;
  PlanChangeLogPanel PanelPlanChangeLog;
  PlanVerificationPanel PanelPlanVerification;
  WatchStatusBar PanelStatusBar;

  auto dashboard = Renderer([&] {
    // Resolve selected plan from whichever block is focused
    FWatchPlanSummary SelectedPlan;
    if (mbActiveBlockFocused && mSelectedPlanIndex >= 0 &&
        mSelectedPlanIndex < static_cast<int>(mSnapshot.mActivePlans.size())) {
      SelectedPlan =
          mSnapshot.mActivePlans[static_cast<size_t>(mSelectedPlanIndex)];
    } else if (!mbActiveBlockFocused && mSelectedNonActiveIndex >= 0 &&
               mSelectedNonActiveIndex <
                   static_cast<int>(mSnapshot.mNonActivePlans.size())) {
      SelectedPlan =
          mSnapshot
              .mNonActivePlans[static_cast<size_t>(mSelectedNonActiveIndex)];
    }

    // ── LEFT PANE (navigator) ─────────────────────────────
    auto leftRow1 = PanelInventory.Render(mSnapshot.mInventory);
    auto leftRow2 = hbox({
        PanelValidation.Render(mSnapshot.mValidation) | flex,
        PanelLint.Render(mSnapshot.mLint) | flex,
    });
    auto leftRow3 =
        PanelActivePlans.Render(mSnapshot.mActivePlans, mSelectedPlanIndex);
    auto leftRow4 = PanelCompleted.Render(mSnapshot.mNonActivePlans,
                                          mSelectedNonActiveIndex);

    auto leftPane = vbox({
        leftRow1,
        leftRow2,
        leftRow3,
        leftRow4,
    });

    // Auto-select first in_progress phase if no phase is selected
    if (mSelectedPhaseIndex < 0 && !SelectedPlan.mPhases.empty()) {
      for (int PhIdx = 0; PhIdx < static_cast<int>(SelectedPlan.mPhases.size());
           ++PhIdx) {
        if (SelectedPlan.mPhases[static_cast<size_t>(PhIdx)].mStatus ==
            "in_progress") {
          mSelectedPhaseIndex = PhIdx;
          break;
        }
      }
      if (mSelectedPhaseIndex < 0) {
        mSelectedPhaseIndex = 0;
      }
    }

    // ── RIGHT PANE (detail for selected plan) ─────────────
    auto rightRow1 = PanelPhaseDetail.Render(SelectedPlan, mSelectedPhaseIndex);
    auto rightRow2 =
        PanelTaxonomy.Render(SelectedPlan, mSelectedPhaseIndex,
                             mSelectedWaveIndex, mSelectedLaneIndex);

    // Resolve selected phase taxonomy for file manifest
    FWatchPhaseTaxonomy SelectedTaxonomy;
    if (mSelectedPhaseIndex >= 0 &&
        mSelectedPhaseIndex < static_cast<int>(SelectedPlan.mPhases.size())) {
      const std::string &PhaseKey =
          SelectedPlan.mPhases[static_cast<size_t>(mSelectedPhaseIndex)]
              .mPhaseKey;
      for (const FWatchPhaseTaxonomy &T : SelectedPlan.mPhaseTaxonomies) {
        if (T.mPhaseKey == PhaseKey) {
          SelectedTaxonomy = T;
          break;
        }
      }
    }
    auto rightRow3 = PanelFileManifest.Render(SelectedTaxonomy, mFilePageIndex);
    auto rightRow4 = PanelBlockers.Render(mSnapshot.mAllBlockers);
    auto rightRow5 =
        PanelValidationFail.Render(mSnapshot.mValidation.mFailedCheckDetails);

    auto rightPane = vbox({
        rightRow1,
        rightRow2,
        rightRow3,
        hbox({
            rightRow4 | flex,
            rightRow5 | flex,
        }),
    });

    // ── Status bar ────────────────────────────────────────
    auto bar =
        PanelStatusBar.Render(kCliVersion, mSnapshot.mSnapshotAtUTC, mTickCount,
                              mSnapshot.mPollDurationMs, mSnapshot.mInventory);

    // ── Title ─────────────────────────────────────────────
    const std::string Title = " UNI-PLAN WATCH  v" + std::string(kCliVersion) +
                              "  \xe2\x97\x8f  " + mSnapshot.mRepoRoot +
                              "  \xe2\x97\x8f  " + mSnapshot.mSnapshotAtUTC +
                              " ";

    // ── Layout (2/3/4-pane with schema and/or impl) ─────
    Element MainContent;
    const bool bHasSchema = mbShowSchemaPane;
    const bool bHasImpl = mbShowImplPane;
    const int LeftWidth = (bHasSchema && bHasImpl) ? 50 : 80;

    auto implPane = vbox({
        PanelPBChangeLog.Render(SelectedPlan, mSelectedPhaseIndex),
        PanelPBVerification.Render(SelectedPlan, mSelectedPhaseIndex),
        PanelPlanChangeLog.Render(SelectedPlan),
        PanelPlanVerification.Render(SelectedPlan),
    });

    if (bHasSchema && bHasImpl) {
      auto middlePane = PanelSchema.Render(SelectedPlan, mSelectedPhaseIndex);
      MainContent = hbox({
          leftPane | size(WIDTH, EQUAL, LeftWidth),
          separator(),
          middlePane | size(WIDTH, EQUAL, 110),
          separator(),
          implPane | size(WIDTH, EQUAL, 80),
          separator(),
          rightPane | flex,
      });
    } else if (bHasSchema) {
      auto middlePane = PanelSchema.Render(SelectedPlan, mSelectedPhaseIndex);
      MainContent = hbox({
          leftPane | size(WIDTH, EQUAL, LeftWidth),
          separator(),
          middlePane | size(WIDTH, EQUAL, 110),
          separator(),
          rightPane | flex,
      });
    } else if (bHasImpl) {
      MainContent = hbox({
          leftPane | size(WIDTH, EQUAL, LeftWidth),
          separator(),
          implPane | size(WIDTH, EQUAL, 80),
          separator(),
          rightPane | flex,
      });
    } else {
      MainContent = hbox({
          leftPane | size(WIDTH, EQUAL, LeftWidth),
          separator(),
          rightPane | flex,
      });
    }

    return vbox({
               text(Title) | bold | hcenter | color(Color::Yellow),
               MainContent | flex,
               separator(),
               bar,
           }) |
           border;
  });

  // Keyboard handling
  dashboard = CatchEvent(dashboard, [this, &screen](Event InEvent) {
    if (InEvent == Event::Character('q')) {
      screen.Exit();
      return true;
    }
    if (InEvent == Event::Character('a')) {
      mbActiveBlockFocused = true;
      mSelectedNonActiveIndex = -1;
      mSelectedPhaseIndex = -1;
      mSelectedWaveIndex = -1;
      mSelectedLaneIndex = -1;
      if (!mSnapshot.mActivePlans.empty()) {
        mSelectedPlanIndex = (mSelectedPlanIndex + 1) %
                             static_cast<int>(mSnapshot.mActivePlans.size());
      }
      return true;
    }
    if (InEvent == Event::Character('A')) {
      mbActiveBlockFocused = true;
      mSelectedNonActiveIndex = -1;
      mSelectedPhaseIndex = -1;
      mSelectedWaveIndex = -1;
      mSelectedLaneIndex = -1;
      if (!mSnapshot.mActivePlans.empty()) {
        mSelectedPlanIndex = (mSelectedPlanIndex - 1 +
                              static_cast<int>(mSnapshot.mActivePlans.size())) %
                             static_cast<int>(mSnapshot.mActivePlans.size());
      }
      return true;
    }
    if (InEvent == Event::Character('n')) {
      mbActiveBlockFocused = false;
      mSelectedPlanIndex = -1;
      mSelectedPhaseIndex = -1;
      mSelectedWaveIndex = -1;
      mSelectedLaneIndex = -1;
      if (!mSnapshot.mNonActivePlans.empty()) {
        if (mSelectedNonActiveIndex < 0) {
          mSelectedNonActiveIndex = -1;
        }
        mSelectedNonActiveIndex =
            (mSelectedNonActiveIndex + 1) %
            static_cast<int>(mSnapshot.mNonActivePlans.size());
      }
      return true;
    }
    if (InEvent == Event::Character('N')) {
      mbActiveBlockFocused = false;
      mSelectedPlanIndex = -1;
      mSelectedPhaseIndex = -1;
      mSelectedWaveIndex = -1;
      mSelectedLaneIndex = -1;
      if (!mSnapshot.mNonActivePlans.empty()) {
        if (mSelectedNonActiveIndex < 0) {
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
        mSelectedPlanIndex < static_cast<int>(mSnapshot.mActivePlans.size())) {
      KeySelectedPlan =
          mSnapshot.mActivePlans[static_cast<size_t>(mSelectedPlanIndex)];
    } else if (!mbActiveBlockFocused && mSelectedNonActiveIndex >= 0 &&
               mSelectedNonActiveIndex <
                   static_cast<int>(mSnapshot.mNonActivePlans.size())) {
      KeySelectedPlan =
          mSnapshot
              .mNonActivePlans[static_cast<size_t>(mSelectedNonActiveIndex)];
    }

    if (InEvent == Event::Character('p')) {
      const int PhaseCount = static_cast<int>(KeySelectedPlan.mPhases.size());
      if (PhaseCount > 0) {
        mSelectedPhaseIndex = (mSelectedPhaseIndex + 1) % PhaseCount;
        mSelectedWaveIndex = -1;
        mSelectedLaneIndex = -1;
        mFilePageIndex = 0;
      }
      return true;
    }
    if (InEvent == Event::Character('P')) {
      const int PhaseCount = static_cast<int>(KeySelectedPlan.mPhases.size());
      if (PhaseCount > 0) {
        if (mSelectedPhaseIndex < 0) {
          mSelectedPhaseIndex = PhaseCount;
        }
        mSelectedPhaseIndex =
            (mSelectedPhaseIndex - 1 + PhaseCount) % PhaseCount;
        mSelectedLaneIndex = -1;
        mFilePageIndex = 0;
      }
      return true;
    }

    if (InEvent == Event::Character('w')) {
      const FWatchPhaseTaxonomy *rpTax = nullptr;
      if (mSelectedPhaseIndex >= 0 &&
          mSelectedPhaseIndex <
              static_cast<int>(KeySelectedPlan.mPhases.size())) {
        const std::string &PhKey =
            KeySelectedPlan.mPhases[static_cast<size_t>(mSelectedPhaseIndex)]
                .mPhaseKey;
        for (const FWatchPhaseTaxonomy &T : KeySelectedPlan.mPhaseTaxonomies) {
          if (T.mPhaseKey == PhKey) {
            rpTax = &T;
            break;
          }
        }
      }
      if (rpTax != nullptr && rpTax->mWaveCount > 0) {
        mSelectedWaveIndex = (mSelectedWaveIndex + 1) % rpTax->mWaveCount;
      }
      return true;
    }
    if (InEvent == Event::Character('W')) {
      const FWatchPhaseTaxonomy *rpTax = nullptr;
      if (mSelectedPhaseIndex >= 0 &&
          mSelectedPhaseIndex <
              static_cast<int>(KeySelectedPlan.mPhases.size())) {
        const std::string &PhKey =
            KeySelectedPlan.mPhases[static_cast<size_t>(mSelectedPhaseIndex)]
                .mPhaseKey;
        for (const FWatchPhaseTaxonomy &T : KeySelectedPlan.mPhaseTaxonomies) {
          if (T.mPhaseKey == PhKey) {
            rpTax = &T;
            break;
          }
        }
      }
      if (rpTax != nullptr && rpTax->mWaveCount > 0) {
        mSelectedWaveIndex =
            (mSelectedWaveIndex - 1 + rpTax->mWaveCount) % rpTax->mWaveCount;
      }
      return true;
    }
    if (InEvent == Event::Character('l')) {
      const FWatchPhaseTaxonomy *rpTax = nullptr;
      if (mSelectedPhaseIndex >= 0 &&
          mSelectedPhaseIndex <
              static_cast<int>(KeySelectedPlan.mPhases.size())) {
        const std::string &PhKey =
            KeySelectedPlan.mPhases[static_cast<size_t>(mSelectedPhaseIndex)]
                .mPhaseKey;
        for (const FWatchPhaseTaxonomy &T : KeySelectedPlan.mPhaseTaxonomies) {
          if (T.mPhaseKey == PhKey) {
            rpTax = &T;
            break;
          }
        }
      }
      if (rpTax != nullptr && !rpTax->mLanes.empty()) {
        mSelectedLaneIndex =
            (mSelectedLaneIndex + 1) % static_cast<int>(rpTax->mLanes.size());
      }
      return true;
    }
    if (InEvent == Event::Character('L')) {
      const FWatchPhaseTaxonomy *rpTax = nullptr;
      if (mSelectedPhaseIndex >= 0 &&
          mSelectedPhaseIndex <
              static_cast<int>(KeySelectedPlan.mPhases.size())) {
        const std::string &PhKey =
            KeySelectedPlan.mPhases[static_cast<size_t>(mSelectedPhaseIndex)]
                .mPhaseKey;
        for (const FWatchPhaseTaxonomy &T : KeySelectedPlan.mPhaseTaxonomies) {
          if (T.mPhaseKey == PhKey) {
            rpTax = &T;
            break;
          }
        }
      }
      if (rpTax != nullptr && !rpTax->mLanes.empty()) {
        const int LaneCount = static_cast<int>(rpTax->mLanes.size());
        mSelectedLaneIndex = (mSelectedLaneIndex - 1 + LaneCount) % LaneCount;
      }
      return true;
    }
    if (InEvent == Event::Character('f')) {
      const FWatchPhaseTaxonomy *rpTax = nullptr;
      if (mSelectedPhaseIndex >= 0 &&
          mSelectedPhaseIndex <
              static_cast<int>(KeySelectedPlan.mPhases.size())) {
        const std::string &PhKey =
            KeySelectedPlan.mPhases[static_cast<size_t>(mSelectedPhaseIndex)]
                .mPhaseKey;
        for (const FWatchPhaseTaxonomy &T : KeySelectedPlan.mPhaseTaxonomies) {
          if (T.mPhaseKey == PhKey) {
            rpTax = &T;
            break;
          }
        }
      }
      if (rpTax != nullptr && !rpTax->mFileManifest.empty()) {
        const int PageCount =
            (static_cast<int>(rpTax->mFileManifest.size()) + 9) / 10;
        mFilePageIndex = (mFilePageIndex + 1) % PageCount;
      }
      return true;
    }
    if (InEvent == Event::Character('F')) {
      const FWatchPhaseTaxonomy *rpTax = nullptr;
      if (mSelectedPhaseIndex >= 0 &&
          mSelectedPhaseIndex <
              static_cast<int>(KeySelectedPlan.mPhases.size())) {
        const std::string &PhKey =
            KeySelectedPlan.mPhases[static_cast<size_t>(mSelectedPhaseIndex)]
                .mPhaseKey;
        for (const FWatchPhaseTaxonomy &T : KeySelectedPlan.mPhaseTaxonomies) {
          if (T.mPhaseKey == PhKey) {
            rpTax = &T;
            break;
          }
        }
      }
      if (rpTax != nullptr && !rpTax->mFileManifest.empty()) {
        const int PageCount =
            (static_cast<int>(rpTax->mFileManifest.size()) + 9) / 10;
        mFilePageIndex = (mFilePageIndex - 1 + PageCount) % PageCount;
      }
      return true;
    }
    if (InEvent == Event::Character('s')) {
      mbShowSchemaPane = !mbShowSchemaPane;
      return true;
    }
    if (InEvent == Event::Character('i')) {
      mbShowImplPane = !mbShowImplPane;
      return true;
    }
    if (InEvent == Event::Character('r')) {
      mbForceRefresh = true;
      return true;
    }
    return false;
  });

  // Background data collection thread
  mRunning.store(true, std::memory_order_relaxed);
  mDataThread = std::thread([this, &screen] {
    while (mRunning.load(std::memory_order_relaxed)) {
      // Signature-based change detection
      if (!mbForceRefresh && mTickCount > 0) {
        uint64_t NewSignature = 0;
        std::string SignatureError;
        const fs::path RepoRoot = NormalizeRepoRootPath(mRepoRoot);
        TryComputeMarkdownCorpusSignature(RepoRoot, NewSignature,
                                          SignatureError);
        if (NewSignature == mLastSignature) {
          std::this_thread::sleep_for(std::chrono::seconds(3));
          continue;
        }
        mLastSignature = NewSignature;
      }
      mbForceRefresh = false;

      // Full snapshot rebuild
      FDocWatchSnapshot Fresh =
          BuildWatchSnapshot(mRepoRoot, mbUseCache, mConfig.mCacheDir, false);

      // Update signature for next cycle
      {
        uint64_t Sig = 0;
        std::string Err;
        const fs::path Root = NormalizeRepoRootPath(mRepoRoot);
        TryComputeMarkdownCorpusSignature(Root, Sig, Err);
        mLastSignature = Sig;
      }

      // Post snapshot to UI thread
      screen.Post([this, Snap = std::move(Fresh)]() mutable {
        mSnapshot = std::move(Snap);
        mTickCount++;
      });

      std::this_thread::sleep_for(std::chrono::seconds(3));
    }
  });

  // Main UI loop (~30 FPS)
  Loop loop(&screen, dashboard);
  while (!loop.HasQuitted()) {
    screen.RequestAnimationFrame();
    loop.RunOnce();
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }

  mRunning.store(false, std::memory_order_relaxed);
  if (mDataThread.joinable()) {
    mDataThread.join();
  }

  return 0;
}

int RunDocWatch(const std::string &InRepoRoot, const DocConfig &InConfig) {
  DocWatchApp App(InRepoRoot, InConfig);
  return App.Run();
}

} // namespace UniPlan
