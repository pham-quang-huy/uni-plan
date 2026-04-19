#include "UniPlanEnums.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ===================================================================
// Tier 4: Query helpers (read-only)
// ===================================================================

// ---------------------------------------------------------------------------
// phase next — find first not_started phase with readiness report
// ---------------------------------------------------------------------------

int RunPhaseNextCommand(const std::vector<std::string> &InArgs,
                        const std::string &InRepoRoot)
{
    const FTopicGetOptions Options = ParseTopicGetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    int NextIndex = -1;
    for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
    {
        if (Bundle.mPhases[I].mLifecycle.mStatus ==
            EExecutionStatus::NotStarted)
        {
            NextIndex = static_cast<int>(I);
            break;
        }
    }

    if (NextIndex < 0)
    {
        if (Options.mbHuman)
        {
            std::cout << kColorDim << "All phases started or completed\n"
                      << kColorReset;
            return 0;
        }
        std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
        EmitJsonFieldBool("ok", true);
        EmitJsonField("topic", Options.mTopic);
        EmitJsonFieldInt("phase_index", -1);
        EmitJsonField("scope", "");
        EmitJsonFieldBool("ready", false);
        std::cout << "\"missing_fields\":[]}\n";
        return 0;
    }

    const FPhaseRecord &Phase = Bundle.mPhases[static_cast<size_t>(NextIndex)];

    // Check readiness gates
    std::vector<std::string> MissingFields;
    if (Phase.mDesign.mInvestigation.empty())
        MissingFields.push_back("investigation");
    if (Phase.mDesign.mCodeEntityContract.empty())
        MissingFields.push_back("code_entity_contract");
    if (Phase.mDesign.mBestPractices.empty())
        MissingFields.push_back("best_practices");
    if (Phase.mDesign.mMultiPlatforming.empty())
        MissingFields.push_back("multi_platforming");
    if (Phase.mTesting.empty())
        MissingFields.push_back("testing");

    const bool Ready = MissingFields.empty();

    if (Options.mbHuman)
    {
        std::cout << kColorBold << "Next phase: " << kColorReset << NextIndex
                  << " — " << Phase.mScope << "\n";
        std::cout << "Ready: "
                  << (Ready ? (std::string(kColorGreen) + "yes")
                            : (std::string(kColorRed) + "no"))
                  << kColorReset << "\n";
        if (!MissingFields.empty())
        {
            std::cout << "Missing: ";
            for (size_t I = 0; I < MissingFields.size(); ++I)
            {
                if (I > 0)
                    std::cout << ", ";
                std::cout << MissingFields[I];
            }
            std::cout << "\n";
        }
        return 0;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldInt("phase_index", NextIndex);
    EmitJsonField("scope", Phase.mScope);
    EmitJsonFieldBool("ready", Ready);
    std::cout << "\"missing_fields\":[";
    for (size_t I = 0; I < MissingFields.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << JSONQuote(MissingFields[I]);
    }
    std::cout << "]}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// phase readiness — gate-by-gate status for a specific phase
// ---------------------------------------------------------------------------

int RunPhaseReadinessCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FPhaseQueryOptions Options = ParsePhaseQueryOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    const FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    struct FGateCheck
    {
        const char *mName;
        bool mbPass;
    };

    const FGateCheck Gates[] = {
        {"investigation", !Phase.mDesign.mInvestigation.empty()},
        {"code_entity_contract", !Phase.mDesign.mCodeEntityContract.empty()},
        {"code_snippets", !Phase.mDesign.mCodeSnippets.empty()},
        {"best_practices", !Phase.mDesign.mBestPractices.empty()},
        {"multi_platforming", !Phase.mDesign.mMultiPlatforming.empty()},
        {"testing", !Phase.mTesting.empty()},
    };

    bool AllPass = true;
    for (const auto &Gate : Gates)
    {
        if (!Gate.mbPass)
            AllPass = false;
    }

    if (Options.mbHuman)
    {
        std::cout << kColorBold << "Phase " << Options.mPhaseIndex
                  << " readiness" << kColorReset << "\n";
        std::cout << "Scope: " << Phase.mScope << "\n";
        std::cout << "Ready: "
                  << (AllPass ? (std::string(kColorGreen) + "yes")
                              : (std::string(kColorRed) + "no"))
                  << kColorReset << "\n\n";

        HumanTable Table;
        Table.mHeaders = {"Gate", "Status"};
        for (const auto &Gate : Gates)
        {
            Table.AddRow(
                {Gate.mName, ColorizeStatus(Gate.mbPass ? "pass" : "fail")});
        }
        Table.Print();
        return 0;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldInt("phase_index", Options.mPhaseIndex);
    EmitJsonFieldBool("ready", AllPass);
    std::cout << "\"gates\":[";
    constexpr size_t GateCount = sizeof(Gates) / sizeof(Gates[0]);
    for (size_t I = 0; I < GateCount; ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("name", Gates[I].mName);
        EmitJsonField("status", Gates[I].mbPass ? "pass" : "fail", false);
        std::cout << "}";
    }
    std::cout << "]}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// topic status — overview of all topics with active phases
// ---------------------------------------------------------------------------

int RunTopicStatusCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    BaseOptions Options;
    const auto Remaining = ConsumeCommonOptions(InArgs, Options, false);
    for (size_t I = 0; I < Remaining.size(); ++I)
    {
        throw UsageError("Unknown option for topic status: " + Remaining[I]);
    }
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    std::vector<std::string> Warnings;
    const std::vector<FTopicBundle> Bundles =
        LoadAllBundles(RepoRoot, Warnings);

    int NotStarted = 0, InProgress = 0, Completed = 0, Blocked = 0,
        Canceled = 0;
    // Aggregate design-depth counts across every phase in every bundle,
    // using the shared hollow / thin / rich thresholds from
    // UniPlanTopicTypes.h. Same bucketing as the watch TUI Design
    // column and `legacy-gap` — agents can read one number off
    // `topic status --human` to see how much authoring debt the corpus
    // still carries.
    int HollowPhases = 0, ThinPhases = 0, RichPhases = 0;
    struct FActiveEntry
    {
        std::string mTopicKey;
        int mPhaseIndex;
        int mPhasesCompleted;
        int mPhasesTotal;
    };
    std::vector<FActiveEntry> Active;

    for (const auto &Bundle : Bundles)
    {
        switch (Bundle.mStatus)
        {
        case ETopicStatus::NotStarted:
            ++NotStarted;
            break;
        case ETopicStatus::InProgress:
            ++InProgress;
            break;
        case ETopicStatus::Completed:
            ++Completed;
            break;
        case ETopicStatus::Blocked:
            ++Blocked;
            break;
        case ETopicStatus::Canceled:
            ++Canceled;
            break;
        }

        for (const FPhaseRecord &Phase : Bundle.mPhases)
        {
            const size_t DesignChars = ComputePhaseDesignChars(Phase);
            if (DesignChars < kPhaseHollowChars)
                ++HollowPhases;
            else if (DesignChars < kPhaseRichMinChars)
                ++ThinPhases;
            else
                ++RichPhases;
        }

        if (Bundle.mStatus == ETopicStatus::InProgress)
        {
            int ActivePhase = -1;
            int PhasesCompleted = 0;
            for (size_t PI = 0; PI < Bundle.mPhases.size(); ++PI)
            {
                if (Bundle.mPhases[PI].mLifecycle.mStatus ==
                    EExecutionStatus::Completed)
                    ++PhasesCompleted;
                if (ActivePhase < 0 && Bundle.mPhases[PI].mLifecycle.mStatus ==
                                           EExecutionStatus::InProgress)
                    ActivePhase = static_cast<int>(PI);
            }
            Active.push_back({Bundle.mTopicKey, ActivePhase, PhasesCompleted,
                              static_cast<int>(Bundle.mPhases.size())});
        }
    }

    const int Total = static_cast<int>(Bundles.size());
    const int TotalPhases = HollowPhases + ThinPhases + RichPhases;

    if (Options.mbHuman)
    {
        std::cout << kColorBold << "Topic Status Overview" << kColorReset
                  << "\n";
        std::cout << "Total: " << Total << "\n\n";

        HumanTable CountTable;
        CountTable.mHeaders = {"Status", "Count"};
        CountTable.AddRow({"not_started", std::to_string(NotStarted)});
        CountTable.AddRow({"in_progress", std::to_string(InProgress)});
        CountTable.AddRow({"completed", std::to_string(Completed)});
        CountTable.AddRow({"blocked", std::to_string(Blocked)});
        CountTable.AddRow({"canceled", std::to_string(Canceled)});
        CountTable.Print();

        // Corpus-wide phase-depth summary. Same thresholds as the watch
        // `Design` column so the two views agree on what "hollow" means.
        std::cout << "\n"
                  << kColorBold << "Phase Design Depth" << kColorReset << " ("
                  << TotalPhases << " phases total)\n";
        HumanTable DepthTable;
        DepthTable.mHeaders = {"Bucket", "Count", "Threshold"};
        DepthTable.AddRow(
            {Colorize(kColorRed, "hollow"), std::to_string(HollowPhases),
             "< " + std::to_string(kPhaseHollowChars) + " chars (~50 lines)"});
        DepthTable.AddRow(
            {Colorize(kColorYellow, "thin"), std::to_string(ThinPhases),
             std::to_string(kPhaseHollowChars) + " – " +
                 std::to_string(kPhaseRichMinChars - 1) + " chars"});
        DepthTable.AddRow({Colorize(kColorGreen, "rich"),
                           std::to_string(RichPhases),
                           "≥ " + std::to_string(kPhaseRichMinChars) +
                               " chars (~200 lines)"});
        DepthTable.Print();

        if (!Active.empty())
        {
            std::cout << "\n"
                      << kColorBold << "Active Topics" << kColorReset << "\n";
            HumanTable ActiveTable;
            ActiveTable.mHeaders = {"Topic", "Phase", "Progress"};
            for (const auto &Entry : Active)
            {
                const std::string PhaseStr =
                    Entry.mPhaseIndex >= 0 ? std::to_string(Entry.mPhaseIndex)
                                           : "—";
                const std::string Progress =
                    std::to_string(Entry.mPhasesCompleted) + "/" +
                    std::to_string(Entry.mPhasesTotal);
                ActiveTable.AddRow({Entry.mTopicKey, PhaseStr, Progress});
            }
            ActiveTable.Print();
        }
        return 0;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonFieldInt("total", Total);
    std::cout << "\"counts\":{";
    EmitJsonFieldInt("not_started", NotStarted);
    EmitJsonFieldInt("in_progress", InProgress);
    EmitJsonFieldInt("completed", Completed);
    EmitJsonFieldInt("blocked", Blocked);
    EmitJsonFieldInt("canceled", Canceled, false);
    std::cout << "},\"phase_depth\":{";
    EmitJsonFieldInt("total", TotalPhases);
    EmitJsonFieldInt("hollow", HollowPhases);
    EmitJsonFieldInt("thin", ThinPhases);
    EmitJsonFieldInt("rich", RichPhases, false);
    std::cout << "},\"active\":[";
    for (size_t I = 0; I < Active.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("topic", Active[I].mTopicKey);
        EmitJsonFieldInt("phase_index", Active[I].mPhaseIndex);
        std::cout << "\"progress\":{";
        EmitJsonFieldInt("completed", Active[I].mPhasesCompleted);
        EmitJsonFieldInt("total", Active[I].mPhasesTotal, false);
        std::cout << "}}";
    }
    std::cout << "]}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// phase wave-status — per-wave job completion
// ---------------------------------------------------------------------------

int RunPhaseWaveStatusCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    const FPhaseQueryOptions Options = ParsePhaseQueryOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    const FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    // Group jobs by wave
    struct FWaveInfo
    {
        int mTotal = 0;
        int mCompleted = 0;
    };
    std::map<int, FWaveInfo> WaveMap;
    for (const auto &Job : Phase.mJobs)
    {
        FWaveInfo &Info = WaveMap[Job.mWave];
        ++Info.mTotal;
        if (Job.mStatus == EExecutionStatus::Completed)
            ++Info.mCompleted;
    }

    // Current wave = first wave with incomplete jobs
    int CurrentWave = -1;
    for (const auto &Pair : WaveMap)
    {
        if (Pair.second.mCompleted < Pair.second.mTotal)
        {
            CurrentWave = Pair.first;
            break;
        }
    }

    if (Options.mbHuman)
    {
        std::cout << kColorBold << "Phase " << Options.mPhaseIndex
                  << " wave status" << kColorReset << "\n";
        if (CurrentWave >= 0)
            std::cout << "Current wave: W" << CurrentWave << "\n";
        else
            std::cout << "All waves complete\n";
        std::cout << "\n";

        HumanTable Table;
        Table.mHeaders = {"Wave", "Done", "Total", "Status"};
        for (const auto &Pair : WaveMap)
        {
            const std::string WaveLabel = "W" + std::to_string(Pair.first);
            const std::string Done = std::to_string(Pair.second.mCompleted);
            const std::string Total = std::to_string(Pair.second.mTotal);
            const bool AllDone = Pair.second.mCompleted == Pair.second.mTotal;
            const std::string Status =
                ColorizeStatus(AllDone ? "completed" : "in_progress");
            Table.AddRow({WaveLabel, Done, Total, Status});
        }
        Table.Print();
        return 0;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldInt("phase_index", Options.mPhaseIndex);
    EmitJsonFieldInt("current_wave", CurrentWave);
    std::cout << "\"waves\":[";
    size_t WI = 0;
    for (const auto &Pair : WaveMap)
    {
        PrintJsonSep(WI++);
        std::cout << "{";
        EmitJsonFieldInt("wave", Pair.first);
        EmitJsonFieldInt("total", Pair.second.mTotal);
        EmitJsonFieldInt("completed", Pair.second.mCompleted, false);
        std::cout << "}";
    }
    std::cout << "]}\n";
    return 0;
}

} // namespace UniPlan
