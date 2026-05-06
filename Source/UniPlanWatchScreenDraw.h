#pragma once

#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace UniPlan
{

struct FWatchScreenTextStyle
{
    ftxui::Color mForeground = ftxui::Color::Default;
    ftxui::Color mBackground = ftxui::Color::Default;
    bool mbDim = false;
    bool mbBold = false;
    bool mbInverted = false;
};

inline bool IsWatchDrawableCell(const ftxui::Screen &InScreen,
                                const int InX, const int InY)
{
    return InX >= 0 && InX < InScreen.dimx() && InY >= 0 &&
           InY < InScreen.dimy() && InX >= InScreen.stencil.x_min &&
           InX <= InScreen.stencil.x_max && InY >= InScreen.stencil.y_min &&
           InY <= InScreen.stencil.y_max;
}

inline bool IsWatchControlGlyph(const std::string &InGlyph)
{
    for (const unsigned char Character : InGlyph)
    {
        if (Character < 0x20 || Character == 0x7f)
        {
            return true;
        }
    }
    return false;
}

inline std::string WatchScreenGlyph(const std::string &InGlyph)
{
    return IsWatchControlGlyph(InGlyph) ? " " : InGlyph;
}

inline void FillWatchScreenBackground(ftxui::Screen &InScreen,
                                      const int InXMin,
                                      const int InXMax, const int InY,
                                      const ftxui::Color InBackground)
{
    for (int X = InXMin; X <= InXMax; ++X)
    {
        if (IsWatchDrawableCell(InScreen, X, InY))
        {
            InScreen.PixelAt(X, InY).background_color = InBackground;
        }
    }
}

inline void DrawWatchScreenText(ftxui::Screen &InScreen, const int InX,
                                const int InY, const int InMaxCells,
                                const std::string &InText,
                                const FWatchScreenTextStyle &InStyle)
{
    if (InMaxCells <= 0)
    {
        return;
    }

    int X = InX;
    const int EndX = InX + InMaxCells - 1;
    for (const std::string &Glyph : ftxui::Utf8ToGlyphs(InText))
    {
        if (X > EndX)
        {
            break;
        }
        if (IsWatchDrawableCell(InScreen, X, InY))
        {
            ftxui::Pixel &Cell = InScreen.PixelAt(X, InY);
            Cell.character = WatchScreenGlyph(Glyph);
            Cell.foreground_color = InStyle.mForeground;
            Cell.background_color = InStyle.mBackground;
            Cell.dim = InStyle.mbDim;
            Cell.bold = InStyle.mbBold;
            Cell.inverted = InStyle.mbInverted;
        }
        ++X;
    }
}

inline void DrawWatchScreenText(ftxui::Screen &InScreen, const int InX,
                                const int InY, const std::string &InText,
                                const FWatchScreenTextStyle &InStyle)
{
    DrawWatchScreenText(InScreen, InX, InY,
                        std::max(0, InScreen.stencil.x_max - InX + 1),
                        InText, InStyle);
}

} // namespace UniPlan
