#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <iostream>
#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Human-mode helpers
// ---------------------------------------------------------------------------

static std::string Colorize(const std::string &InColor,
                            const std::string &InText)
{
    return InColor + InText + kColorReset;
}

// ---------------------------------------------------------------------------
// Cache info (human)
// ---------------------------------------------------------------------------

int RunCacheInfoHuman(const CacheInfoResult &InResult)
{
    std::cout << ColorBold("Cache Info") << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Property", "Value"};
    Table.AddRow(
        {"Cache directory", Colorize(kColorOrange, InResult.mCacheDir)});
    Table.AddRow({"Exists", InResult.mbCacheExists
                                ? Colorize(kColorGreen, "yes")
                                : Colorize(kColorDim, "no")});
    if (InResult.mbCacheExists)
    {
        Table.AddRow(
            {"Size", Colorize(kColorOrange,
                              FormatBytesHuman(InResult.mCacheSizeBytes))});
        Table.AddRow(
            {"Entries", Colorize(kColorOrange,
                                 std::to_string(InResult.mCacheEntryCount))});
    }
    Table.AddRow({"Current repo cache",
                  Colorize(kColorDim, InResult.mCurrentRepoCachePath)});
    Table.AddRow({"Current repo cached", InResult.mbCurrentRepoCacheExists
                                             ? Colorize(kColorGreen, "yes")
                                             : Colorize(kColorDim, "no")});
    Table.AddRow({"Config dir override",
                  InResult.mConfigCacheDir.empty()
                      ? Colorize(kColorDim, "(default)")
                      : Colorize(kColorOrange, InResult.mConfigCacheDir)});
    Table.AddRow({"Cache enabled", InResult.mbCacheEnabled
                                       ? Colorize(kColorGreen, "true")
                                       : Colorize(kColorYellow, "false")});
    Table.AddRow({"Cache verbose", InResult.mbCacheVerbose
                                       ? Colorize(kColorGreen, "true")
                                       : Colorize(kColorDim, "false")});
    Table.AddRow({"INI path", Colorize(kColorDim, InResult.mIniPath)});
    Table.Print();

    PrintHumanWarnings(InResult.mWarnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Cache clear (human)
// ---------------------------------------------------------------------------

int RunCacheClearHuman(const CacheClearResult &InResult)
{
    std::cout << ColorBold("Cache Clear") << "\n\n";

    if (!InResult.mbSuccess)
    {
        std::cout << Colorize(kColorRed, "Error: " + InResult.mError) << "\n";
        return 1;
    }

    if (InResult.mEntriesRemoved == 0 && InResult.mBytesFreed == 0)
    {
        std::cout << Colorize(kColorDim, "Cache is already empty.") << "\n";
    }
    else
    {
        std::cout << "Cleared "
                  << Colorize(kColorOrange,
                              std::to_string(InResult.mEntriesRemoved))
                  << " entries, freed "
                  << Colorize(kColorOrange,
                              FormatBytesHuman(InResult.mBytesFreed))
                  << ".\n";
    }

    PrintHumanWarnings(InResult.mWarnings);
    return InResult.mbSuccess ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Cache config (human)
// ---------------------------------------------------------------------------

int RunCacheConfigHuman(const CacheConfigResult &InResult)
{
    std::cout << ColorBold("Cache Config") << "\n\n";

    if (!InResult.mbSuccess)
    {
        std::cout << Colorize(kColorRed, "Error: " + InResult.mError) << "\n";
        return 1;
    }

    std::cout << "Configuration written to: "
              << Colorize(kColorDim, InResult.mIniPath) << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Setting", "Value"};
    Table.AddRow({"dir", InResult.mDir.empty()
                             ? Colorize(kColorDim, "(default)")
                             : Colorize(kColorOrange, InResult.mDir)});
    Table.AddRow({"enabled", InResult.mbEnabled
                                 ? Colorize(kColorGreen, "true")
                                 : Colorize(kColorYellow, "false")});
    Table.AddRow({"verbose", InResult.mbVerbose
                                 ? Colorize(kColorGreen, "true")
                                 : Colorize(kColorDim, "false")});
    Table.Print();

    PrintHumanWarnings(InResult.mWarnings);
    return InResult.mbSuccess ? 0 : 1;
}

} // namespace UniPlan
