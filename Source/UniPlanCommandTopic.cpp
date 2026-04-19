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
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// EmitValidationCommandsJson — write a typed validation_commands array
// as JSON: [{"platform":"...","command":"...","description":"..."}, ...]
// Emits an empty array when the input vector is empty (never null).
// ---------------------------------------------------------------------------

void EmitValidationCommandsJson(
    const char *InName, const std::vector<FValidationCommand> &InCommands,
    bool InTrailingComma)
{
    std::cout << "\"" << InName << "\":[";
    for (size_t I = 0; I < InCommands.size(); ++I)
    {
        if (I > 0)
            std::cout << ",";
        const FValidationCommand &C = InCommands[I];
        std::cout << "{";
        EmitJsonField("platform", ToString(C.mPlatform));
        EmitJsonField("command", C.mCommand);
        EmitJsonField("description", C.mDescription, false);
        std::cout << "}";
    }
    std::cout << "]";
    if (InTrailingComma)
        std::cout << ",";
}

// ---------------------------------------------------------------------------
// EmitDependenciesJson — write a typed dependencies array as JSON:
//   [{"kind":"bundle","topic":"X","phase":null,"path":"","note":"..."}, ...]
// Emits an empty array when the input vector is empty (never null).
// ---------------------------------------------------------------------------

void EmitDependenciesJson(const char *InName,
                          const std::vector<FBundleReference> &InDeps,
                          bool InTrailingComma)
{
    std::cout << "\"" << InName << "\":[";
    for (size_t I = 0; I < InDeps.size(); ++I)
    {
        if (I > 0)
            std::cout << ",";
        const FBundleReference &R = InDeps[I];
        std::cout << "{";
        EmitJsonField("kind", ToString(R.mKind));
        EmitJsonField("topic", R.mTopic);
        if (R.mPhase < 0)
            std::cout << "\"phase\":null,";
        else
            std::cout << "\"phase\":" << R.mPhase << ",";
        EmitJsonField("path", R.mPath);
        EmitJsonField("note", R.mNote, false);
        std::cout << "}";
    }
    std::cout << "]";
    if (InTrailingComma)
        std::cout << ",";
}
// ---------------------------------------------------------------------------
// Topic List — JSON output
// ---------------------------------------------------------------------------

static int RunTopicListJson(const fs::path &InRepoRoot,
                            const FTopicListOptions &InOptions)
{
    std::vector<std::string> Warnings;
    std::vector<FTopicBundle> Bundles = LoadAllBundles(InRepoRoot, Warnings);

    // Status filter
    const bool bFilter = (InOptions.mStatus != "all");
    if (bFilter)
    {
        Bundles.erase(std::remove_if(
                          Bundles.begin(), Bundles.end(),
                          [&](const FTopicBundle &B)
                          { return ToString(B.mStatus) != InOptions.mStatus; }),
                      Bundles.end());
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kTopicListSchema, UTC, InRepoRoot.string());
    if (bFilter)
        EmitJsonField("status_filter", InOptions.mStatus);
    EmitJsonFieldSizeT("count", Bundles.size());
    std::cout << "\"topics\":[";
    for (size_t I = 0; I < Bundles.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("topic", Bundles[I].mTopicKey);
        EmitJsonField("status", ToString(Bundles[I].mStatus));
        EmitJsonField("title", Bundles[I].mMetadata.mTitle);
        EmitJsonFieldSizeT("phase_count", Bundles[I].mPhases.size(), false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Topic List — Human output
// ---------------------------------------------------------------------------

static int RunTopicListHuman(const fs::path &InRepoRoot,
                             const FTopicListOptions &InOptions)
{
    std::vector<std::string> Warnings;
    std::vector<FTopicBundle> Bundles = LoadAllBundles(InRepoRoot, Warnings);

    const bool bFilter = (InOptions.mStatus != "all");
    if (bFilter)
    {
        Bundles.erase(std::remove_if(
                          Bundles.begin(), Bundles.end(),
                          [&](const FTopicBundle &B)
                          { return ToString(B.mStatus) != InOptions.mStatus; }),
                      Bundles.end());
    }

    std::cout << kColorBold << "Topics" << kColorReset;
    if (bFilter)
        std::cout << " [" << kColorYellow << "status=" << InOptions.mStatus
                  << kColorReset << "]";
    std::cout << " count=" << kColorOrange << Bundles.size() << kColorReset
              << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Topic", "Status", "Phases", "Title"};
    for (const FTopicBundle &B : Bundles)
    {
        Table.AddRow({B.mTopicKey, ColorizeStatus(ToString(B.mStatus)),
                      std::to_string(B.mPhases.size()), B.mMetadata.mTitle});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// Topic Get — JSON output
// ---------------------------------------------------------------------------

static int RunTopicGetJson(const fs::path &InRepoRoot,
                           const FTopicGetOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Section filter: if the options carry a non-empty `mSections`, only
    // the named sections are emitted. Identity fields (topic, status,
    // title, phase_count) are always emitted. Empty = emit all
    // (backward-compatible default). Added v0.84.0.
    const std::set<std::string> Want(InOptions.mSections.begin(),
                                     InOptions.mSections.end());
    const bool bAll = Want.empty();
    const auto Wants = [&](const char *InName) -> bool
    { return bAll || Want.count(InName) > 0; };

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kTopicGetSchema, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    EmitJsonField("status", ToString(Bundle.mStatus));
    const FPlanMetadata &Meta = Bundle.mMetadata;
    EmitJsonField("title", Meta.mTitle);
    if (Wants("summary"))
        EmitJsonFieldNullable("summary", Meta.mSummary);
    if (Wants("goals"))
        EmitJsonFieldNullable("goals", Meta.mGoals);
    if (Wants("non_goals"))
        EmitJsonFieldNullable("non_goals", Meta.mNonGoals);
    if (Wants("risks"))
        EmitJsonFieldNullable("risks", Meta.mRisks);
    if (Wants("acceptance_criteria"))
        EmitJsonFieldNullable("acceptance_criteria", Meta.mAcceptanceCriteria);
    if (Wants("problem_statement"))
        EmitJsonFieldNullable("problem_statement", Meta.mProblemStatement);
    if (Wants("validation_commands"))
        EmitValidationCommandsJson("validation_commands",
                                   Meta.mValidationCommands);
    if (Wants("baseline_audit"))
        EmitJsonFieldNullable("baseline_audit", Meta.mBaselineAudit);
    if (Wants("execution_strategy"))
        EmitJsonFieldNullable("execution_strategy", Meta.mExecutionStrategy);
    if (Wants("locked_decisions"))
        EmitJsonFieldNullable("locked_decisions", Meta.mLockedDecisions);
    if (Wants("source_references"))
        EmitJsonFieldNullable("source_references", Meta.mSourceReferences);
    if (Wants("dependencies"))
        EmitDependenciesJson("dependencies", Meta.mDependencies);
    if (Wants("next_actions"))
        EmitJsonFieldNullable("next_actions", Bundle.mNextActions);
    EmitJsonFieldSizeT("phase_count", Bundle.mPhases.size());

    if (Wants("phases"))
    {
        // Phase summary — compact index/status/scope
        std::cout << "\"phase_summary\":[";
        for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
        {
            const FPhaseRecord &Phase = Bundle.mPhases[I];
            PrintJsonSep(I);
            std::cout << "{";
            EmitJsonFieldSizeT("index", I);
            EmitJsonField("status", ToString(Phase.mLifecycle.mStatus));
            // Truncate scope to 120 chars for compactness
            std::string Scope = Phase.mScope;
            if (Scope.size() > 120)
                Scope = Scope.substr(0, 117) + "...";
            EmitJsonField("scope", Scope, false);
            std::cout << "}";
        }
        std::cout << "],";
    }

    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Topic Get — Human output
// ---------------------------------------------------------------------------

static int RunTopicGetHuman(const fs::path &InRepoRoot,
                            const FTopicGetOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // v0.84.0: honor --sections filter in human mode too. Identity
    // header + phase count are always shown; per-section blocks are
    // gated by the filter set.
    const std::set<std::string> Want(InOptions.mSections.begin(),
                                     InOptions.mSections.end());
    const bool bAll = Want.empty();
    const auto Wants = [&](const char *InName) -> bool
    { return bAll || Want.count(InName) > 0; };

    std::cout << kColorBold << Bundle.mTopicKey << kColorReset
              << "  status=" << ColorizeStatus(ToString(Bundle.mStatus))
              << "  phases=" << kColorOrange << Bundle.mPhases.size()
              << kColorReset << "\n";
    std::cout << kColorDim << Bundle.mMetadata.mTitle << kColorReset << "\n\n";

    auto PrintField = [](const char *InLabel, const std::string &InVal)
    {
        if (!InVal.empty())
        {
            std::cout << kColorBold << InLabel << kColorReset << "\n"
                      << kColorDim << InVal << kColorReset << "\n\n";
        }
    };

    if (Wants("summary"))
        PrintField("Summary", Bundle.mMetadata.mSummary);
    if (Wants("goals"))
        PrintField("Goals", Bundle.mMetadata.mGoals);
    if (Wants("non_goals"))
        PrintField("Non-Goals", Bundle.mMetadata.mNonGoals);
    if (Wants("risks"))
        PrintField("Risks", Bundle.mMetadata.mRisks);
    if (Wants("acceptance_criteria"))
        PrintField("Acceptance Criteria", Bundle.mMetadata.mAcceptanceCriteria);
    if (Wants("problem_statement"))
        PrintField("Problem Statement", Bundle.mMetadata.mProblemStatement);
    if (Wants("baseline_audit"))
        PrintField("Baseline Audit", Bundle.mMetadata.mBaselineAudit);
    if (Wants("execution_strategy"))
        PrintField("Execution Strategy", Bundle.mMetadata.mExecutionStrategy);
    if (Wants("locked_decisions"))
        PrintField("Locked Decisions", Bundle.mMetadata.mLockedDecisions);
    if (Wants("source_references"))
        PrintField("Source References", Bundle.mMetadata.mSourceReferences);
    if (Wants("next_actions"))
        PrintField("Next Actions", Bundle.mNextActions);

    // Validation commands table
    if (Wants("validation_commands") &&
        !Bundle.mMetadata.mValidationCommands.empty())
    {
        std::cout << kColorBold << "Validation Commands" << kColorReset << "\n";
        HumanTable VCTable;
        VCTable.mHeaders = {"Platform", "Command", "Description"};
        for (const FValidationCommand &C : Bundle.mMetadata.mValidationCommands)
        {
            VCTable.AddRow({ToString(C.mPlatform), C.mCommand, C.mDescription});
        }
        VCTable.Print();
        std::cout << "\n";
    }

    // Dependencies table
    if (Wants("dependencies") && !Bundle.mMetadata.mDependencies.empty())
    {
        std::cout << kColorBold << "Dependencies" << kColorReset << "\n";
        HumanTable DepTable;
        DepTable.mHeaders = {"Kind", "Topic", "Phase", "Path", "Note"};
        for (const FBundleReference &R : Bundle.mMetadata.mDependencies)
        {
            const std::string PhaseCell =
                R.mPhase < 0 ? "" : std::to_string(R.mPhase);
            DepTable.AddRow(
                {ToString(R.mKind), R.mTopic, PhaseCell, R.mPath, R.mNote});
        }
        DepTable.Print();
        std::cout << "\n";
    }

    // Phase table
    if (Wants("phases"))
    {
        std::cout << kColorBold << "Phases" << kColorReset << "\n";
        HumanTable Table;
        Table.mHeaders = {"Index", "Status", "Scope"};
        for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
        {
            const FPhaseRecord &Phase = Bundle.mPhases[I];
            std::string Scope = Phase.mScope;
            if (Scope.size() > 80)
                Scope = Scope.substr(0, 77) + "...";
            Table.AddRow({std::to_string(I),
                          ColorizeStatus(ToString(Phase.mLifecycle.mStatus)),
                          kColorDim + Scope + kColorReset});
        }
        Table.Print();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// RunTopicCommand — dispatch topic list / topic get
// ---------------------------------------------------------------------------

int RunTopicCommand(const std::vector<std::string> &InArgs,
                    const std::string &InRepoRoot)
{
    // 3-prologue --help handling (v0.85.0). See DispatchSubcommand<N> in
    // UniPlanCommandDispatch.cpp for the canonical pattern this mirrors.
    if (InArgs.empty())
    {
        PrintCommandUsage(std::cout, "topic");
        return 0;
    }
    const std::string Sub = InArgs[0];
    if (Sub == "--help" || Sub == "-h")
    {
        PrintCommandUsage(std::cout, "topic");
        return 0;
    }
    const std::vector<std::string> SubArgs(InArgs.begin() + 1, InArgs.end());
    if (ContainsHelpFlag(SubArgs))
    {
        PrintCommandUsage(std::cout, "topic", Sub);
        return 0;
    }

    if (Sub == "list")
    {
        const FTopicListOptions Options = ParseTopicListOptions(SubArgs);
        const fs::path RepoRoot = NormalizeRepoRootPath(
            Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
        if (Options.mbHuman)
            return RunTopicListHuman(RepoRoot, Options);
        return RunTopicListJson(RepoRoot, Options);
    }

    if (Sub == "get")
    {
        const FTopicGetOptions Options = ParseTopicGetOptions(SubArgs);
        const fs::path RepoRoot = NormalizeRepoRootPath(
            Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
        if (Options.mbHuman)
            return RunTopicGetHuman(RepoRoot, Options);
        return RunTopicGetJson(RepoRoot, Options);
    }

    if (Sub == "set")
    {
        return RunTopicSetCommand(SubArgs, InRepoRoot);
    }

    // Semantic topic commands
    if (Sub == "start")
        return RunTopicStartCommand(SubArgs, InRepoRoot);
    if (Sub == "complete")
        return RunTopicCompleteCommand(SubArgs, InRepoRoot);
    if (Sub == "block")
        return RunTopicBlockCommand(SubArgs, InRepoRoot);
    if (Sub == "status")
        return RunTopicStatusCommand(SubArgs, InRepoRoot);

    throw UsageError("Unknown topic subcommand: " + Sub +
                     ". Expected: list, get, set, start, complete, block, "
                     "status");
}

} // namespace UniPlan
