#include "UniPlanWatchScroll.h"
#include "UniPlanWatchScreenDraw.h"

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/util/autoreset.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace UniPlan
{

namespace
{

static void DrawDimText(ftxui::Screen &InScreen, const int InX,
                        const int InY, const std::string &InText)
{
    DrawWatchScreenText(
        InScreen, InX, InY, InText,
        FWatchScreenTextStyle{ftxui::Color::GrayDark, ftxui::Color::Default,
                              true, false, false});
}

class ScrollFrameNode : public ftxui::Node
{
  public:
    ScrollFrameNode(ftxui::Element InChild,
                    FWatchScrollRegionState &InOutScrollState)
        : Node(ftxui::Elements{std::move(InChild)}),
          rpScrollState(&InOutScrollState)
    {
    }

    void ComputeRequirement() override
    {
        children_[0]->ComputeRequirement();
        requirement_ = children_[0]->requirement();
        mContentHeight = std::max(0, requirement_.min_y);
        requirement_.min_y = 0;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_y = 1;
    }

    void SetBox(ftxui::Box InBox) override
    {
        Node::SetBox(InBox);

        const int ExternalHeight =
            std::max(0, InBox.y_max - InBox.y_min + 1);
        mbHasScrollIndicators = mContentHeight > ExternalHeight;
        mViewportHeight =
            mbHasScrollIndicators ? std::max(0, ExternalHeight - 2)
                                  : ExternalHeight;
        mViewportBox = InBox;
        if (mbHasScrollIndicators)
        {
            mViewportBox.y_min = InBox.y_min + 1;
            mViewportBox.y_max = InBox.y_max - 1;
        }

        const int MaxOffset =
            mViewportHeight > 0
                ? std::max(0, mContentHeight - mViewportHeight)
                : 0;
        int ScrollOffset = std::clamp(rpScrollState->mOffset, 0, MaxOffset);
        if (mViewportHeight > 0 && requirement_.focused.enabled)
        {
            const ftxui::Box &FocusedBox = requirement_.focused.box;
            if (FocusedBox.y_min < ScrollOffset)
            {
                ScrollOffset = FocusedBox.y_min;
            }
            else if (FocusedBox.y_max >= ScrollOffset + mViewportHeight)
            {
                ScrollOffset = FocusedBox.y_max - mViewportHeight + 1;
            }
            ScrollOffset = std::clamp(ScrollOffset, 0, MaxOffset);
        }
        rpScrollState->mOffset = ScrollOffset;
        rpScrollState->mMaxOffset = MaxOffset;

        ftxui::Box ChildBox = mViewportBox;
        const int InternalHeight = std::max(mContentHeight, mViewportHeight);
        ChildBox.y_min = mViewportBox.y_min - ScrollOffset;
        ChildBox.y_max = ChildBox.y_min + InternalHeight - 1;
        children_[0]->SetBox(ChildBox);
    }

    void Render(ftxui::Screen &InScreen) override
    {
        if (mbHasScrollIndicators)
        {
            if (rpScrollState->mOffset > 0)
            {
                DrawScrollIndicator(InScreen, box_.y_min, "\xe2\x86\x91",
                                    rpScrollState->mOffset, "above");
            }
        }

        {
            const ftxui::AutoReset<ftxui::Box> Stencil(
                &InScreen.stencil,
                ftxui::Box::Intersection(mViewportBox, InScreen.stencil));
            children_[0]->Render(InScreen);
        }

        if (mbHasScrollIndicators)
        {
            const int Below =
                std::max(0,
                         rpScrollState->mMaxOffset - rpScrollState->mOffset);
            if (Below > 0)
            {
                DrawScrollIndicator(InScreen, box_.y_max, "\xe2\x86\x93",
                                    Below, "below");
            }
        }
    }

  private:
    void DrawScrollIndicator(ftxui::Screen &InScreen, const int InY,
                             const std::string &InArrow, const int InCount,
                             const std::string &InLabel) const
    {
        DrawDimText(InScreen, box_.x_min, InY,
                    InArrow + " " + std::to_string(InCount) + " " +
                        InLabel);
    }

    FWatchScrollRegionState *rpScrollState;
    ftxui::Box mViewportBox;
    int mContentHeight = 0;
    int mViewportHeight = 0;
    bool mbHasScrollIndicators = false;
};

} // namespace

ftxui::Element ScrollFrame(ftxui::Element InContent,
                           FWatchScrollRegionState &InOutScrollState)
{
    return std::make_shared<ScrollFrameNode>(
        std::move(InContent) | ftxui::vscroll_indicator, InOutScrollState);
}

} // namespace UniPlan
