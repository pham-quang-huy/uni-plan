#include "UniPlanWatchInteraction.h"

#include <array>

namespace UniPlan
{

namespace
{

struct FWatchSidePaneScrollBinding
{
    EWatchSidePane mPane = EWatchSidePane::None;
    FWatchScrollRegionState FWatchScrollState::*mpScrollState = nullptr;
};

static const std::array<FWatchSidePaneScrollBinding, 3> &
WatchSidePaneScrollBindings()
{
    static const std::array<FWatchSidePaneScrollBinding, 3> Bindings = {{
        {EWatchSidePane::PhaseDetails, &FWatchScrollState::mPhaseDetails},
        {EWatchSidePane::FileManifest, &FWatchScrollState::mFileManifest},
        {EWatchSidePane::CodeSnippets, &FWatchScrollState::mCodeSnippets},
    }};
    return Bindings;
}

} // namespace

FWatchScrollRegionState *
ResolveWatchSidePaneScroll(FWatchScrollState &InOutScrollState,
                           const EWatchSidePane InPane)
{
    for (const FWatchSidePaneScrollBinding &Binding :
         WatchSidePaneScrollBindings())
    {
        if (Binding.mPane == InPane && Binding.mpScrollState != nullptr)
        {
            return &(InOutScrollState.*(Binding.mpScrollState));
        }
    }
    return nullptr;
}

void ResetWatchSidePaneScroll(FWatchScrollState &InOutScrollState)
{
    for (const FWatchSidePaneScrollBinding &Binding :
         WatchSidePaneScrollBindings())
    {
        if (Binding.mpScrollState != nullptr)
        {
            (InOutScrollState.*(Binding.mpScrollState)).Reset();
        }
    }
}

void ResetWatchSelectedPhaseDependentScroll(
    FWatchInteractionState &InOutState)
{
    InOutState.mScrollState.mLanes.Reset();
    ResetWatchSidePaneScroll(InOutState.mScrollState);
}

void ResetWatchPhaseScopedScroll(FWatchInteractionState &InOutState)
{
    InOutState.mScrollState.mPhaseList.Reset();
    ResetWatchSelectedPhaseDependentScroll(InOutState);
}

void ResetWatchPlanScopedScroll(FWatchInteractionState &InOutState)
{
    InOutState.mSelectedPhaseIndex = -1;
    InOutState.mSelectedWaveIndex = -1;
    InOutState.mSelectedLaneIndex = -1;
    ResetWatchPhaseScopedScroll(InOutState);
}

void StepWatchPhaseSelection(FWatchInteractionState &InOutState,
                             const int InPhaseCount, const int InDelta)
{
    if (InPhaseCount <= 0 || InDelta == 0)
    {
        return;
    }

    if (InOutState.mSelectedPhaseIndex < 0)
    {
        InOutState.mSelectedPhaseIndex =
            InDelta < 0 ? InPhaseCount : -1;
    }

    InOutState.mSelectedPhaseIndex =
        (InOutState.mSelectedPhaseIndex + InDelta + InPhaseCount) %
        InPhaseCount;
    InOutState.mSelectedWaveIndex = -1;
    InOutState.mSelectedLaneIndex = -1;
    ResetWatchSelectedPhaseDependentScroll(InOutState);
}

void ToggleWatchSidePane(FWatchInteractionState &InOutState,
                         const EWatchSidePane InPane)
{
    FWatchScrollRegionState *rpScrollState =
        ResolveWatchSidePaneScroll(InOutState.mScrollState, InPane);
    if (rpScrollState == nullptr)
    {
        return;
    }

    if (InOutState.mSidePane == InPane)
    {
        InOutState.mSidePane = EWatchSidePane::None;
        return;
    }

    InOutState.mSidePane = InPane;
    rpScrollState->Reset();
}

bool ScrollWatchSidePane(FWatchInteractionState &InOutState,
                         const int InDelta)
{
    FWatchScrollRegionState *rpScrollState =
        ResolveWatchSidePaneScroll(InOutState.mScrollState,
                                   InOutState.mSidePane);
    if (rpScrollState == nullptr)
    {
        return false;
    }

    if (InDelta < 0)
    {
        if (rpScrollState->mOffset > 0)
        {
            --rpScrollState->mOffset;
        }
        return true;
    }
    if (InDelta > 0)
    {
        ++rpScrollState->mOffset;
        return true;
    }
    return false;
}

} // namespace UniPlan
