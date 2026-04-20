#include "UniPlanCommandHelp.h"
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

    // Filter by phase. Capture the original storage index (position in
    // Bundle.mChangeLogs) alongside each pointer so downstream consumers
    // can target the row they see via `changelog set --index` / `remove
    // --index` (v0.95.0+ — these indices are the stable mutation target;
    // render order ≠ storage order after the sort below). Storage-order
    // identity was the root cause of the pre-v0.95.0 drift bug where
    // agents running repair loops would `set --index N` expecting the
    // N-th rendered row and instead mutate whichever row happened to
    // sit at position N in the underlying vector.
    const int PhaseFilter = InOptions.mbHasScopeFilter
                                ? std::atoi(InOptions.mScopeFilter.c_str())
                                : -2;
    std::vector<std::pair<size_t, const FChangeLogEntry *>> Filtered;
    for (size_t I = 0; I < Bundle.mChangeLogs.size(); ++I)
    {
        const FChangeLogEntry &Entry = Bundle.mChangeLogs[I];
        if (PhaseFilter != -2 && Entry.mPhase != PhaseFilter)
            continue;
        Filtered.emplace_back(I, &Entry);
    }

    // Sort: topic-level (-1) first, then ascending phase, then date desc.
    // `first` (storage index) rides along unchanged so the emitted
    // `index` field stays bound to its source row regardless of sort.
    std::sort(Filtered.begin(), Filtered.end(),
              [](const std::pair<size_t, const FChangeLogEntry *> &A,
                 const std::pair<size_t, const FChangeLogEntry *> &B)
              {
                  if (A.second->mPhase != B.second->mPhase)
                      return A.second->mPhase < B.second->mPhase;
                  return A.second->mDate > B.second->mDate;
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
        const size_t StorageIndex = Filtered[I].first;
        const FChangeLogEntry &Entry = *Filtered[I].second;
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonFieldSizeT("index", StorageIndex);
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
    std::vector<std::pair<size_t, const FChangeLogEntry *>> Filtered;
    for (size_t I = 0; I < Bundle.mChangeLogs.size(); ++I)
    {
        const FChangeLogEntry &Entry = Bundle.mChangeLogs[I];
        if (PhaseFilter != -2 && Entry.mPhase != PhaseFilter)
            continue;
        Filtered.emplace_back(I, &Entry);
    }

    // Sort: topic-level (-1) first, then ascending phase, then date desc.
    // The `Idx` column below preserves storage-order identity so operators
    // reading the table can cite it directly to `changelog set --index`.
    std::sort(Filtered.begin(), Filtered.end(),
              [](const std::pair<size_t, const FChangeLogEntry *> &A,
                 const std::pair<size_t, const FChangeLogEntry *> &B)
              {
                  if (A.second->mPhase != B.second->mPhase)
                      return A.second->mPhase < B.second->mPhase;
                  return A.second->mDate > B.second->mDate;
              });

    std::cout << kColorBold << "Changelog" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset
              << " count=" << Filtered.size() << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Idx",   "Phase",    "Date",  "Type",
                      "Actor", "Affected", "Change"};
    for (const std::pair<size_t, const FChangeLogEntry *> &Row : Filtered)
    {
        const size_t StorageIndex = Row.first;
        const FChangeLogEntry *rpEntry = Row.second;
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
        Table.AddRow({std::to_string(StorageIndex), PhaseDisplay,
                      rpEntry->mDate, ToString(rpEntry->mType),
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
    // 3-prologue --help handling (v0.85.0). changelog's "default"
    // subcommand is the query (no subcommand token), so an explicit
    // --help at the group level triggers group help; per-subcommand
    // --help (changelog add/set/remove --help) also routes here.
    if (!InArgs.empty() && (InArgs[0] == "--help" || InArgs[0] == "-h"))
    {
        PrintCommandUsage(std::cout, "changelog");
        return 0;
    }
    if (!InArgs.empty() &&
        (InArgs[0] == "add" || InArgs[0] == "set" || InArgs[0] == "remove"))
    {
        const std::string Sub = InArgs[0];
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        if (ContainsHelpFlag(SubArgs))
        {
            PrintCommandUsage(std::cout, "changelog", Sub);
            return 0;
        }
    }
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

    // Capture the original storage index alongside each filtered entry
    // — mirrors the changelog query contract (v0.95.0+). `verification
    // set --index N` / `verification remove --index N` target the
    // storage index in Bundle.mVerifications; the emitted `index` field
    // here is that stable target regardless of filter.
    const int PhaseFilter = InOptions.mbHasScopeFilter
                                ? std::atoi(InOptions.mScopeFilter.c_str())
                                : -2;
    std::vector<std::pair<size_t, const FVerificationEntry *>> Filtered;
    for (size_t I = 0; I < Bundle.mVerifications.size(); ++I)
    {
        const FVerificationEntry &Entry = Bundle.mVerifications[I];
        if (PhaseFilter != -2 && Entry.mPhase != PhaseFilter)
            continue;
        Filtered.emplace_back(I, &Entry);
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
        const size_t StorageIndex = Filtered[I].first;
        const FVerificationEntry &Entry = *Filtered[I].second;
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonFieldSizeT("index", StorageIndex);
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
    std::vector<std::pair<size_t, const FVerificationEntry *>> Filtered;
    for (size_t I = 0; I < Bundle.mVerifications.size(); ++I)
    {
        const FVerificationEntry &Entry = Bundle.mVerifications[I];
        if (VPhaseFilter != -2 && Entry.mPhase != VPhaseFilter)
            continue;
        Filtered.emplace_back(I, &Entry);
    }

    std::cout << kColorBold << "Verification" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset
              << " count=" << Filtered.size() << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Idx", "Phase", "Date", "Check", "Result", "Detail"};
    for (const std::pair<size_t, const FVerificationEntry *> &Row : Filtered)
    {
        const size_t StorageIndex = Row.first;
        const FVerificationEntry *rpEntry = Row.second;
        std::string Check = rpEntry->mCheck;
        if (Check.size() > 60)
            Check = Check.substr(0, 57) + "...";
        std::string Detail = rpEntry->mDetail;
        if (Detail.size() > 60)
            Detail = Detail.substr(0, 57) + "...";
        const std::string PhaseDisplay =
            rpEntry->mPhase < 0 ? "(topic)" : std::to_string(rpEntry->mPhase);
        Table.AddRow({std::to_string(StorageIndex), PhaseDisplay,
                      rpEntry->mDate, kColorDim + Check + kColorReset,
                      rpEntry->mResult, Detail});
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
    // 3-prologue --help handling (v0.85.0). Mirrors changelog above.
    if (!InArgs.empty() && (InArgs[0] == "--help" || InArgs[0] == "-h"))
    {
        PrintCommandUsage(std::cout, "verification");
        return 0;
    }
    if (!InArgs.empty() && (InArgs[0] == "add" || InArgs[0] == "set"))
    {
        const std::string Sub = InArgs[0];
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        if (ContainsHelpFlag(SubArgs))
        {
            PrintCommandUsage(std::cout, "verification", Sub);
            return 0;
        }
    }
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
    if (ContainsHelpFlag(InArgs))
    {
        PrintCommandUsage(std::cout, "timeline");
        return 0;
    }
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
        EmitJsonFieldSizeT("phase_index",
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
    if (ContainsHelpFlag(InArgs))
    {
        PrintCommandUsage(std::cout, "blockers");
        return 0;
    }
    const FBundleBlockersOptions Options = ParseBundleBlockersOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleBlockersHuman(RepoRoot, Options);
    return RunBundleBlockersJson(RepoRoot, Options);
}

} // namespace UniPlan
