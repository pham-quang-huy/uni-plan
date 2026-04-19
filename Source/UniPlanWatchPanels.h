#pragma once

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
                          int InSelectedIndex) const;
};

class PhaseDetailPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex) const;
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
                          int InSelectedIndex) const;
};

class DeferredPlansPanel
{
  public:
    ftxui::Element Render(const std::vector<FWatchPlanSummary> &InPlans) const;
};

class ValidationFailPanel
{
  public:
    ftxui::Element
    Render(const std::vector<ValidateCheck> &InFailedChecks) const;
};

class ExecutionTaxonomyPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex, int InSelectedWaveIndex,
                          int InSelectedLaneIndex) const;
};

class SchemaPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex) const;
};

class FileManifestPanel
{
  public:
    ftxui::Element Render(const FPhaseTaxonomy &InTaxonomy,
                          int InFilePageIndex) const;
};

class PlaybookChangeLogPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex) const;
};

class PlaybookVerificationPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan,
                          int InSelectedPhaseIndex) const;
};

class PlanChangeLogPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan) const;
};

class PlanVerificationPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan) const;
};

class ImplChangeLogPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan) const;
};

class ImplVerificationPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan) const;
};

class PlanDetailPanel
{
  public:
    ftxui::Element Render(const FWatchPlanSummary &InPlan) const;
};

class WatchStatusBar
{
  public:
    ftxui::Element Render(const std::string &InVersion,
                          const std::string &InTime, int InTick, int InPollMs,
                          const FWatchInventoryCounters &InCounters) const;
};

} // namespace UniPlan
