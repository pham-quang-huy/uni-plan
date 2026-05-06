#pragma once

#include "UniPlanWatchScroll.h"

#include <cstdint>

namespace UniPlan
{

enum class EWatchSidePane : uint8_t
{
    None,
    PhaseDetails,
    FileManifest,
    CodeSnippets
};

struct FWatchInteractionState
{
    FWatchScrollState mScrollState{};
    int mSelectedPhaseIndex = -1;
    int mSelectedWaveIndex = -1;
    int mSelectedLaneIndex = -1;
    EWatchSidePane mSidePane = EWatchSidePane::None;
};

FWatchScrollRegionState *
ResolveWatchSidePaneScroll(FWatchScrollState &InOutScrollState,
                           EWatchSidePane InPane);

void ResetWatchSidePaneScroll(FWatchScrollState &InOutScrollState);
void ResetWatchSelectedPhaseDependentScroll(
    FWatchInteractionState &InOutState);
void ResetWatchPhaseScopedScroll(FWatchInteractionState &InOutState);
void ResetWatchPlanScopedScroll(FWatchInteractionState &InOutState);
void StepWatchPhaseSelection(FWatchInteractionState &InOutState,
                             int InPhaseCount, int InDelta);
void ToggleWatchSidePane(FWatchInteractionState &InOutState,
                         EWatchSidePane InPane);
bool ScrollWatchSidePane(FWatchInteractionState &InOutState, int InDelta);

} // namespace UniPlan
