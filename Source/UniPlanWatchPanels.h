#pragma once

#include "UniPlanWatchScroll.h"
#include "UniPlanWatchSnapshot.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <string>
#include <vector>

namespace UniPlan
{

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

class PhaseDetailPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex, bool InMetricView,
                          FWatchScrollRegionState &InOutScrollState) const;
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
};

class FileManifestPanel
{
  public:
    ftxui::Element Render(const FPhaseTaxonomy &InTaxonomy,
                          FWatchScrollRegionState &InOutScrollState) const;
};

class CodeSnippetPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex,
                          FWatchScrollRegionState &InOutScrollState) const;
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
