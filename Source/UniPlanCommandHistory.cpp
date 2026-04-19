#include "UniPlanEnums.h"
#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ===================================================================
// Batch 4: changelog + verification + timeline + blockers
// ===================================================================

// ---------------------------------------------------------------------------
// Changelog — JSON
// ---------------------------------------------------------------------------

// Resolve phase index to human-readable label
static std::string ResolvePhaseLabel(int InPhase,
                                     const std::vector<FPhaseRecord> &InPhases)
{
    if (InPhase < 0)
        return "";
    const size_t Idx = static_cast<size_t>(InPhase);
    if (Idx < InPhases.size())
    {
        std::string Scope = InPhases[Idx].mScope;
        if (Scope.size() > 60)
            Scope = Scope.substr(0, 57) + "...";
        return "phases[" + std::to_string(InPhase) + "] " + Scope;
    }
    return std::to_string(InPhase);
}

// Sort key for changelog entries: -1 (topic-level) first, then ascending
static int PhaseSortKey(int InPhase)
{
    return InPhase; // -1 sorts before 0, 1, 2, ...
}

// Sort scope: plan < implementation < numeric ascending
// ScopeSortKey for verification entries (still string-based)
static int ScopeSortKey(const std::string &InScope)
{
    if (InScope.empty() || InScope == "plan")
        return -1;
    bool bIsDigit = !InScope.empty();
    for (char C : InScope)
    {
        if (!std::isdigit(static_cast<unsigned char>(C)))
        {
            bIsDigit = false;
            break;
        }
    }
    if (bIsDigit)
        return std::atoi(InScope.c_str());
    return 9999;
}

static int RunBundleChangelogJson(const fs::path &InRepoRoot,
                                  const FBundleChangelogOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Filter by phase
    const int PhaseFilter = InOptions.mbHasScopeFilter
                                ? std::atoi(InOptions.mScopeFilter.c_str())
                                : -2;
    std::vector<const FChangeLogEntry *> Filtered;
    for (const FChangeLogEntry &Entry : Bundle.mChangeLogs)
    {
        if (PhaseFilter != -2 && Entry.mPhase != PhaseFilter)
            continue;
        Filtered.push_back(&Entry);
    }

    // Sort: topic-level (-1) first, then ascending phase, then date desc
    std::sort(Filtered.begin(), Filtered.end(),
              [](const FChangeLogEntry *A, const FChangeLogEntry *B)
              {
                  if (A->mPhase != B->mPhase)
                      return A->mPhase < B->mPhase;
                  return A->mDate > B->mDate;
              });

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kChangelogSchemaV2, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    if (InOptions.mbHasScopeFilter)
        EmitJsonFieldInt("phase_filter", PhaseFilter);
    EmitJsonFieldSizeT("count", Filtered.size());
    std::cout << "\"entries\":[";
    for (size_t I = 0; I < Filtered.size(); ++I)
    {
        const FChangeLogEntry &Entry = *Filtered[I];
        PrintJsonSep(I);
        std::cout << "{";
        if (Entry.mPhase < 0)
            std::cout << "\"phase\":null,";
        else
            EmitJsonFieldInt("phase", Entry.mPhase);
        EmitJsonField("phase_label",
                      ResolvePhaseLabel(Entry.mPhase, Bundle.mPhases));
        EmitJsonField("date", Entry.mDate);
        EmitJsonField("change", Entry.mChange);
        EmitJsonFieldNullable("affected", Entry.mAffected);
        EmitJsonField("type", ToString(Entry.mType));
        EmitJsonField("actor", ToString(Entry.mActor), false);
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Changelog — Human
// ---------------------------------------------------------------------------

static int RunBundleChangelogHuman(const fs::path &InRepoRoot,
                                   const FBundleChangelogOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const int PhaseFilter = InOptions.mbHasScopeFilter
                                ? std::atoi(InOptions.mScopeFilter.c_str())
                                : -2;
    std::vector<const FChangeLogEntry *> Filtered;
    for (const FChangeLogEntry &Entry : Bundle.mChangeLogs)
    {
        if (PhaseFilter != -2 && Entry.mPhase != PhaseFilter)
            continue;
        Filtered.push_back(&Entry);
    }

    // Sort: topic-level (-1) first, then ascending phase, then date desc
    std::sort(Filtered.begin(), Filtered.end(),
              [](const FChangeLogEntry *A, const FChangeLogEntry *B)
              {
                  if (A->mPhase != B->mPhase)
                      return A->mPhase < B->mPhase;
                  return A->mDate > B->mDate;
              });

    std::cout << kColorBold << "Changelog" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset
              << " count=" << Filtered.size() << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Phase", "Date", "Type", "Actor", "Affected", "Change"};
    for (const FChangeLogEntry *rpEntry : Filtered)
    {
        std::string Change = rpEntry->mChange;
        if (Change.size() > 80)
            Change = Change.substr(0, 77) + "...";
        const std::string Label =
            ResolvePhaseLabel(rpEntry->mPhase, Bundle.mPhases);
        std::string PhaseDisplay = Label;
        if (PhaseDisplay.size() > 40)
            PhaseDisplay = PhaseDisplay.substr(0, 37) + "...";
        std::string Affected = rpEntry->mAffected;
        if (Affected.size() > 40)
            Affected = Affected.substr(0, 37) + "...";
        Table.AddRow({PhaseDisplay, rpEntry->mDate, ToString(rpEntry->mType),
                      ToString(rpEntry->mActor), Affected,
                      kColorDim + Change + kColorReset});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// RunBundleChangelogCommand
// ---------------------------------------------------------------------------

int RunBundleChangelogCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    // Check for "add" subcommand
    if (!InArgs.empty() && InArgs[0] == "add")
    {
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        return RunChangelogAddCommand(SubArgs, InRepoRoot);
    }
    if (!InArgs.empty() && InArgs[0] == "set")
    {
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        return RunChangelogSetCommand(SubArgs, InRepoRoot);
    }
    if (!InArgs.empty() && InArgs[0] == "remove")
    {
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        return RunChangelogRemoveCommand(SubArgs, InRepoRoot);
    }

    const FBundleChangelogOptions Options = ParseBundleChangelogOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleChangelogHuman(RepoRoot, Options);
    return RunBundleChangelogJson(RepoRoot, Options);
}

// ---------------------------------------------------------------------------
// Verification — JSON
// ---------------------------------------------------------------------------

static int
RunBundleVerificationJson(const fs::path &InRepoRoot,
                          const FBundleVerificationOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const int PhaseFilter = InOptions.mbHasScopeFilter
                                ? std::atoi(InOptions.mScopeFilter.c_str())
                                : -2;
    std::vector<const FVerificationEntry *> Filtered;
    for (const FVerificationEntry &Entry : Bundle.mVerifications)
    {
        if (PhaseFilter != -2 && Entry.mPhase != PhaseFilter)
            continue;
        Filtered.push_back(&Entry);
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kVerificationSchemaV2, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    if (InOptions.mbHasScopeFilter)
        EmitJsonFieldInt("phase_filter", PhaseFilter);
    EmitJsonFieldSizeT("count", Filtered.size());
    std::cout << "\"entries\":[";
    for (size_t I = 0; I < Filtered.size(); ++I)
    {
        const FVerificationEntry &Entry = *Filtered[I];
        PrintJsonSep(I);
        std::cout << "{";
        if (Entry.mPhase < 0)
            std::cout << "\"phase\":null,";
        else
            EmitJsonFieldInt("phase", Entry.mPhase);
        EmitJsonField("date", Entry.mDate);
        EmitJsonField("check", Entry.mCheck);
        EmitJsonFieldNullable("result", Entry.mResult);
        EmitJsonFieldNullable("detail", Entry.mDetail, false);
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Verification — Human
// ---------------------------------------------------------------------------

static int
RunBundleVerificationHuman(const fs::path &InRepoRoot,
                           const FBundleVerificationOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const int VPhaseFilter = InOptions.mbHasScopeFilter
                                 ? std::atoi(InOptions.mScopeFilter.c_str())
                                 : -2;
    std::vector<const FVerificationEntry *> Filtered;
    for (const FVerificationEntry &Entry : Bundle.mVerifications)
    {
        if (VPhaseFilter != -2 && Entry.mPhase != VPhaseFilter)
            continue;
        Filtered.push_back(&Entry);
    }

    std::cout << kColorBold << "Verification" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset
              << " count=" << Filtered.size() << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Phase", "Date", "Check", "Result", "Detail"};
    for (const FVerificationEntry *rpEntry : Filtered)
    {
        std::string Check = rpEntry->mCheck;
        if (Check.size() > 60)
            Check = Check.substr(0, 57) + "...";
        std::string Detail = rpEntry->mDetail;
        if (Detail.size() > 60)
            Detail = Detail.substr(0, 57) + "...";
        const std::string PhaseDisplay =
            rpEntry->mPhase < 0 ? "(topic)" : std::to_string(rpEntry->mPhase);
        Table.AddRow({PhaseDisplay, rpEntry->mDate,
                      kColorDim + Check + kColorReset, rpEntry->mResult,
                      Detail});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// RunBundleVerificationCommand
// ---------------------------------------------------------------------------

int RunBundleVerificationCommand(const std::vector<std::string> &InArgs,
                                 const std::string &InRepoRoot)
{
    // Check for "add" subcommand
    if (!InArgs.empty() && InArgs[0] == "add")
    {
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        return RunVerificationAddCommand(SubArgs, InRepoRoot);
    }
    if (!InArgs.empty() && InArgs[0] == "set")
    {
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        return RunVerificationSetCommand(SubArgs, InRepoRoot);
    }

    const FBundleVerificationOptions Options =
        ParseBundleVerificationOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleVerificationHuman(RepoRoot, Options);
    return RunBundleVerificationJson(RepoRoot, Options);
}

// ---------------------------------------------------------------------------
// Timeline — JSON (changelogs + verifications sorted by date)
// ---------------------------------------------------------------------------

static int RunBundleTimelineJson(const fs::path &InRepoRoot,
                                 const FBundleTimelineOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Merge changelogs and verifications into timeline entries
    struct FTimelineEntry
    {
        std::string mDate;
        std::string mKind; // "changelog" or "verification"
        int mPhase;        // phase index or -1 for topic-level
        std::string mText;
        std::string mType;
    };

    std::vector<FTimelineEntry> Entries;
    for (const FChangeLogEntry &CL : Bundle.mChangeLogs)
    {
        if (!InOptions.mSince.empty() && CL.mDate < InOptions.mSince)
            continue;
        if (InOptions.mbHasPhaseFilter && CL.mPhase != InOptions.mPhaseFilter)
            continue;
        Entries.push_back(
            {CL.mDate, "changelog", CL.mPhase, CL.mChange, ToString(CL.mType)});
    }
    for (const FVerificationEntry &VE : Bundle.mVerifications)
    {
        if (!InOptions.mSince.empty() && VE.mDate < InOptions.mSince)
            continue;
        if (InOptions.mbHasPhaseFilter && VE.mPhase != InOptions.mPhaseFilter)
            continue;
        Entries.push_back({VE.mDate, "verification", VE.mPhase, VE.mCheck, ""});
    }

    // Sort by date descending (newest first)
    std::sort(Entries.begin(), Entries.end(),
              [](const FTimelineEntry &A, const FTimelineEntry &B)
              { return A.mDate > B.mDate; });

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kTimelineSchema, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    if (!InOptions.mSince.empty())
        EmitJsonField("since", InOptions.mSince);
    if (InOptions.mbHasPhaseFilter)
        EmitJsonFieldInt("phase_filter", InOptions.mPhaseFilter);
    EmitJsonFieldSizeT("count", Entries.size());
    std::cout << "\"entries\":[";
    for (size_t I = 0; I < Entries.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("date", Entries[I].mDate);
        EmitJsonField("kind", Entries[I].mKind);
        if (Entries[I].mPhase < 0)
            std::cout << "\"phase\":null,";
        else
            EmitJsonFieldInt("phase", Entries[I].mPhase);
        EmitJsonField("phase_label",
                      ResolvePhaseLabel(Entries[I].mPhase, Bundle.mPhases));
        EmitJsonFieldNullable("text", Entries[I].mText);
        EmitJsonFieldNullable("type", Entries[I].mType, false);
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Timeline — Human
// ---------------------------------------------------------------------------

static int RunBundleTimelineHuman(const fs::path &InRepoRoot,
                                  const FBundleTimelineOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    struct FTimelineEntry
    {
        std::string mDate;
        std::string mKind;
        int mPhase; // phase index or -1 for topic-level
        std::string mType;
        std::string mText;
    };

    std::vector<FTimelineEntry> Entries;
    for (const FChangeLogEntry &CL : Bundle.mChangeLogs)
    {
        if (!InOptions.mSince.empty() && CL.mDate < InOptions.mSince)
            continue;
        if (InOptions.mbHasPhaseFilter && CL.mPhase != InOptions.mPhaseFilter)
            continue;
        Entries.push_back(
            {CL.mDate, "changelog", CL.mPhase, ToString(CL.mType), CL.mChange});
    }
    for (const FVerificationEntry &VE : Bundle.mVerifications)
    {
        if (!InOptions.mSince.empty() && VE.mDate < InOptions.mSince)
            continue;
        if (InOptions.mbHasPhaseFilter && VE.mPhase != InOptions.mPhaseFilter)
            continue;
        Entries.push_back(
            {VE.mDate, "verification", VE.mPhase, VE.mResult, VE.mCheck});
    }
    std::sort(Entries.begin(), Entries.end(),
              [](const FTimelineEntry &A, const FTimelineEntry &B)
              { return A.mDate > B.mDate; });

    std::cout << kColorBold << "Timeline" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset
              << " count=" << Entries.size() << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Date", "Kind", "Phase", "Type/Result", "Text"};
    for (const FTimelineEntry &E : Entries)
    {
        std::string Text = E.mText;
        if (Text.size() > 60)
            Text = Text.substr(0, 57) + "...";
        const std::string PhaseDisplay =
            E.mPhase < 0 ? "(topic)" : std::to_string(E.mPhase);
        Table.AddRow({E.mDate, E.mKind, PhaseDisplay, E.mType,
                      kColorDim + Text + kColorReset});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// RunBundleTimelineCommand
// ---------------------------------------------------------------------------

int RunBundleTimelineCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FBundleTimelineOptions Options = ParseBundleTimelineOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleTimelineHuman(RepoRoot, Options);
    return RunBundleTimelineJson(RepoRoot, Options);
}

// ---------------------------------------------------------------------------
// Blockers — JSON
// ---------------------------------------------------------------------------

static int RunBundleBlockersJson(const fs::path &InRepoRoot,
                                 const FBundleBlockersOptions &InOptions)
{
    std::vector<std::string> Warnings;
    std::vector<FTopicBundle> Bundles;

    if (!InOptions.mTopic.empty())
    {
        FTopicBundle Bundle;
        std::string Error;
        if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
        {
            std::cerr << Error << "\n";
            return 1;
        }
        Bundles.push_back(std::move(Bundle));
    }
    else
    {
        Bundles = LoadAllBundles(InRepoRoot, Warnings);
    }

    std::vector<BlockerItem> BlockerEntries;
    for (const FTopicBundle &Bundle : Bundles)
    {
        std::vector<BlockerItem> Emitted = CollectBundleBlockers(Bundle);
        BlockerEntries.insert(BlockerEntries.end(),
                              std::make_move_iterator(Emitted.begin()),
                              std::make_move_iterator(Emitted.end()));
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kBlockersSchema, UTC, InRepoRoot.string());
    EmitJsonFieldSizeT("count", BlockerEntries.size());
    std::cout << "\"blockers\":[";
    for (size_t I = 0; I < BlockerEntries.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("topic", BlockerEntries[I].mTopicKey);
        EmitJsonFieldSizeT(
            "phase_index",
            static_cast<size_t>(BlockerEntries[I].mPhaseIndex));
        EmitJsonField("status", BlockerEntries[I].mStatus);
        EmitJsonField("kind", BlockerEntries[I].mKind);
        EmitJsonField("scope", BlockerEntries[I].mNotes);
        EmitJsonField("blockers", BlockerEntries[I].mAction, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Blockers — Human
// ---------------------------------------------------------------------------

static int RunBundleBlockersHuman(const fs::path &InRepoRoot,
                                  const FBundleBlockersOptions &InOptions)
{
    std::vector<std::string> Warnings;
    std::vector<FTopicBundle> Bundles;

    if (!InOptions.mTopic.empty())
    {
        FTopicBundle Bundle;
        std::string Error;
        if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
        {
            std::cerr << Error << "\n";
            return 1;
        }
        Bundles.push_back(std::move(Bundle));
    }
    else
    {
        Bundles = LoadAllBundles(InRepoRoot, Warnings);
    }

    std::vector<BlockerItem> BlockerEntries;
    for (const FTopicBundle &Bundle : Bundles)
    {
        std::vector<BlockerItem> Emitted = CollectBundleBlockers(Bundle);
        BlockerEntries.insert(BlockerEntries.end(),
                              std::make_move_iterator(Emitted.begin()),
                              std::make_move_iterator(Emitted.end()));
    }

    std::cout << kColorBold << "Blockers" << kColorReset
              << " count=" << BlockerEntries.size() << "\n\n";

    if (BlockerEntries.empty())
    {
        std::cout << kColorDim << "No blocked phases." << kColorReset << "\n";
        return 0;
    }

    HumanTable Table;
    Table.mHeaders = {"Topic", "Phase", "Status", "Scope", "Blockers"};
    for (const BlockerItem &E : BlockerEntries)
    {
        std::string Scope = E.mNotes;
        if (Scope.size() > 40)
            Scope = Scope.substr(0, 37) + "...";
        Table.AddRow({E.mTopicKey, std::to_string(E.mPhaseIndex), E.mStatus,
                      Scope, E.mAction});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// RunBundleBlockersCommand
// ---------------------------------------------------------------------------

int RunBundleBlockersCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FBundleBlockersOptions Options = ParseBundleBlockersOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleBlockersHuman(RepoRoot, Options);
    return RunBundleBlockersJson(RepoRoot, Options);
}


} // namespace UniPlan
