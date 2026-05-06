#pragma once

#include <ftxui/dom/elements.hpp>

namespace UniPlan
{

struct FWatchScrollRegionState
{
    int mOffset = 0;
    int mMaxOffset = 0;

    void Reset()
    {
        mOffset = 0;
        mMaxOffset = 0;
    }
};

struct FWatchScrollState
{
    FWatchScrollRegionState mActivePlans;
    FWatchScrollRegionState mNonActivePlans;
    FWatchScrollRegionState mPhaseDetail;
    FWatchScrollRegionState mLanes;
    FWatchScrollRegionState mFileManifest;
    FWatchScrollRegionState mCodeSnippets;
};

ftxui::Element ScrollFrame(ftxui::Element InContent,
                           FWatchScrollRegionState &InOutScrollState);

} // namespace UniPlan
