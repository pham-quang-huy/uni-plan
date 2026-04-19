#include "UniPlanEnums.h"
#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJSONLineIndex.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Validate — validates .Plan.json against schema constraints
// ---------------------------------------------------------------------------

// Resolve mLine for every ValidateCheck by consulting the raw JSON text of
// the owning bundle. Each bundle's text is scanned at most once.
static void ResolveIssueLines(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &InOutChecks)
{
    std::unordered_map<std::string, FJsonLineIndex> IndexByTopic;
    const auto GetIndex =
        [&](const std::string &InTopic) -> const FJsonLineIndex *
    {
        auto It = IndexByTopic.find(InTopic);
        if (It != IndexByTopic.end())
            return &It->second;
        for (const FTopicBundle &B : InBundles)
        {
            if (B.mTopicKey != InTopic)
                continue;
            if (B.mBundlePath.empty())
                break;
            std::ifstream Stream(B.mBundlePath);
            if (!Stream)
                break;
            const std::string Text((std::istreambuf_iterator<char>(Stream)),
                                   std::istreambuf_iterator<char>());
            FJsonLineIndex Index;
            Index.Build(Text);
            It = IndexByTopic.emplace(InTopic, std::move(Index)).first;
            return &It->second;
        }
        return nullptr;
    };

    for (ValidateCheck &C : InOutChecks)
    {
        if (C.mLine >= 0)
            continue;
        if (C.mTopic.empty() || C.mPath.empty())
            continue;
        const FJsonLineIndex *rpIndex = GetIndex(C.mTopic);
        if (rpIndex == nullptr)
            continue;
        C.mLine = rpIndex->LineFor(C.mPath);
    }
}

static int RunBundleValidateJson(const fs::path &InRepoRoot,
                                 const FBundleValidateOptions &InOptions)
{
    std::vector<std::string> BundleWarnings;
    // Always load all bundles so cross-topic evaluators (e.g.
    // topic_ref_integrity) can resolve references against the full
    // topic-key registry, even under --topic filtering. Scoping is
    // applied to the emitted output below rather than to the loaded
    // bundle set.
    std::vector<FTopicBundle> Bundles =
        LoadAllBundles(InRepoRoot, BundleWarnings);

    if (!InOptions.mTopic.empty())
    {
        bool bFound = false;
        for (const FTopicBundle &B : Bundles)
        {
            if (B.mTopicKey == InOptions.mTopic)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            const std::string UTC = GetUtcNow();
            PrintJsonHeader(kValidateSchema, UTC, InRepoRoot.string());
            EmitJsonField("topic", InOptions.mTopic);
            EmitJsonFieldBool("valid", false);
            std::cout << "\"issues\":[{";
            EmitJsonField("id", "load_failure");
            EmitJsonField("severity", "error_major");
            EmitJsonFieldBool("ok", false);
            EmitJsonField("detail",
                          "topic not found in repo: " + InOptions.mTopic,
                          false);
            std::cout << "}],";
            PrintJsonClose(BundleWarnings);
            return 1;
        }
    }

    std::vector<ValidateCheck> Checks = ValidateAllBundles(Bundles);
    ResolveIssueLines(Bundles, Checks);

    // When --topic scopes the output, drop checks for other topics so
    // the caller sees only their target topic's issues while keeping
    // the evaluator registry complete.
    if (!InOptions.mTopic.empty())
    {
        Checks.erase(std::remove_if(Checks.begin(), Checks.end(),
                                    [&](const ValidateCheck &C)
                                    { return C.mTopic != InOptions.mTopic; }),
                     Checks.end());
    }

    int ErrorMajorCount = 0;
    int ErrorMinorCount = 0;
    int WarningCount = 0;
    bool bValid = true;
    for (const ValidateCheck &C : Checks)
    {
        if (!C.mbOk)
        {
            if (C.mSeverity == EValidationSeverity::ErrorMajor)
            {
                ErrorMajorCount++;
                bValid = false;
            }
            else if (C.mSeverity == EValidationSeverity::ErrorMinor)
                ErrorMinorCount++;
            else
                WarningCount++;
        }
    }

    // --strict promotes ErrorMinor and Warning into the bValid gate.
    if (InOptions.mbStrict && (ErrorMinorCount > 0 || WarningCount > 0))
        bValid = false;

    const size_t TargetCount =
        InOptions.mTopic.empty() ? Bundles.size() : size_t{1};

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kValidateSchema, UTC, InRepoRoot.string());
    if (!InOptions.mTopic.empty())
        EmitJsonField("topic", InOptions.mTopic);
    EmitJsonFieldSizeT("bundle_count", TargetCount);
    EmitJsonFieldBool("valid", bValid);
    EmitJsonFieldInt("error_major", ErrorMajorCount);
    EmitJsonFieldInt("error_minor", ErrorMinorCount);
    EmitJsonFieldInt("warnings", WarningCount);

    // Only emit failures
    std::cout << "\"issues\":[";
    bool bFirst = true;
    for (const ValidateCheck &C : Checks)
    {
        if (C.mbOk)
            continue;
        if (!bFirst)
            std::cout << ",";
        bFirst = false;
        std::cout << "{";
        EmitJsonField("id", C.mID);
        EmitJsonField("severity", ToString(C.mSeverity));
        EmitJsonFieldNullable("topic", C.mTopic);
        EmitJsonFieldNullable("path", C.mPath);
        if (C.mLine > 0)
            EmitJsonFieldInt("line", C.mLine);
        else
            std::cout << "\"line\":null,";
        EmitJsonField("detail", C.mDetail, false);
        std::cout << "}";
    }
    std::cout << "],";

    // summary — aggregate structural stats across the loaded bundle set.
    // Consumers use this to ask "which phases look thin" or "which
    // manifest paths are missing" in one call instead of looping
    // `phase get` per phase. Bypasses the raw-JSON-read temptation
    // that led to v0.70.x violations.
    std::cout << "\"summary\":{";
    EmitJsonFieldSizeT("topic_count", TargetCount);
    std::cout << "\"topics\":[";
    bool bTopicEmitted = false;
    for (size_t BI = 0; BI < Bundles.size(); ++BI)
    {
        const FTopicBundle &B = Bundles[BI];
        // Under --topic scope, the summary only reports the target
        // topic; other bundles are loaded solely for cross-topic
        // integrity checks.
        if (!InOptions.mTopic.empty() && B.mTopicKey != InOptions.mTopic)
            continue;
        if (bTopicEmitted)
            std::cout << ",";
        bTopicEmitted = true;
        std::cout << "{";
        EmitJsonField("topic", B.mTopicKey);
        EmitJsonFieldSizeT("phase_count", B.mPhases.size());

        // Status distribution
        size_t cNotStarted = 0, cInProgress = 0, cCompleted = 0, cBlocked = 0;
        for (const FPhaseRecord &P : B.mPhases)
        {
            switch (P.mLifecycle.mStatus)
            {
            case EExecutionStatus::NotStarted:
                ++cNotStarted;
                break;
            case EExecutionStatus::InProgress:
                ++cInProgress;
                break;
            case EExecutionStatus::Completed:
                ++cCompleted;
                break;
            case EExecutionStatus::Blocked:
                ++cBlocked;
                break;
            }
        }
        std::cout << "\"status_distribution\":{";
        EmitJsonFieldSizeT("not_started", cNotStarted);
        EmitJsonFieldSizeT("in_progress", cInProgress);
        EmitJsonFieldSizeT("completed", cCompleted);
        EmitJsonFieldSizeT("blocked", cBlocked, false);
        std::cout << "},";

        // Per-phase stats
        std::cout << "\"phases\":[";
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            PrintJsonSep(PI);
            std::cout << "{";
            EmitJsonFieldSizeT("index", PI);
            EmitJsonField("status", ToString(P.mLifecycle.mStatus));
            EmitJsonFieldSizeT("scope_chars", P.mScope.size());
            EmitJsonFieldSizeT("output_chars", P.mOutput.size());
            // Unified design-depth measure (v0.80.0): same signal used
            // by `legacy-gap` (`v4_design_chars`) and the watch TUI
            // `Design` column. Shared helper in UniPlanTopicTypes.h.
            // Includes scope + output + every design-material field so
            // agents can filter "hollow" phases consistently across
            // commands: `design_chars < 4000` (≈ 50 lines) is the
            // hollow threshold everywhere. Prior v0.79.0 computation
            // excluded scope + output and produced a smaller number —
            // the v0.80.0 sync raises reported values slightly but
            // aligns the semantic.
            EmitJsonFieldSizeT("design_chars", ComputePhaseDesignChars(P));
            EmitJsonFieldSizeT("jobs_count", P.mJobs.size());
            EmitJsonFieldSizeT("testing_count", P.mTesting.size());
            EmitJsonFieldSizeT("file_manifest_count", P.mFileManifest.size());
            size_t Missing = 0;
            for (const FFileManifestItem &FM : P.mFileManifest)
            {
                if (!FM.mFilePath.empty() &&
                    !ManifestPathExists(InRepoRoot, FM.mFilePath))
                    ++Missing;
            }
            EmitJsonFieldSizeT("file_manifest_missing", Missing, false);
            std::cout << "}";
        }
        std::cout << "]}";
    }
    std::cout << "]},";

    PrintJsonClose(BundleWarnings);
    return bValid ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Validate — Human
// ---------------------------------------------------------------------------

static int RunBundleValidateHuman(const fs::path &InRepoRoot,
                                  const FBundleValidateOptions &InOptions)
{
    std::vector<std::string> BundleWarnings;
    // See RunBundleValidateJson for the full-load rationale: cross-topic
    // evaluators need the complete topic-key registry even under
    // --topic scope.
    std::vector<FTopicBundle> Bundles =
        LoadAllBundles(InRepoRoot, BundleWarnings);

    if (!InOptions.mTopic.empty())
    {
        bool bFound = false;
        for (const FTopicBundle &B : Bundles)
        {
            if (B.mTopicKey == InOptions.mTopic)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            std::cerr << kColorRed << "FAIL" << kColorReset << " "
                      << InOptions.mTopic << ": topic not found in repo\n";
            return 1;
        }
    }

    std::vector<ValidateCheck> Checks = ValidateAllBundles(Bundles);
    ResolveIssueLines(Bundles, Checks);

    // Filter checks to target topic when --topic scopes the output.
    if (!InOptions.mTopic.empty())
    {
        Checks.erase(std::remove_if(Checks.begin(), Checks.end(),
                                    [&](const ValidateCheck &C)
                                    { return C.mTopic != InOptions.mTopic; }),
                     Checks.end());
    }

    int ErrorMajorCount = 0;
    int ErrorMinorCount = 0;
    int WarningCount = 0;
    for (const ValidateCheck &C : Checks)
    {
        if (!C.mbOk)
        {
            if (C.mSeverity == EValidationSeverity::ErrorMajor)
                ErrorMajorCount++;
            else if (C.mSeverity == EValidationSeverity::ErrorMinor)
                ErrorMinorCount++;
            else
                WarningCount++;
        }
    }

    const int TotalFailed = ErrorMajorCount + ErrorMinorCount + WarningCount;
    const size_t TargetCount =
        InOptions.mTopic.empty() ? Bundles.size() : size_t{1};
    std::cout << kColorBold << "Validate" << kColorReset << "  " << TargetCount
              << " bundles";
    if (TotalFailed == 0)
    {
        std::cout << "  " << kColorGreen << "PASS" << kColorReset << "\n";
    }
    else
    {
        std::cout << "  " << kColorRed << TotalFailed << " issues"
                  << kColorReset << " (";
        if (ErrorMajorCount > 0)
            std::cout << kColorRed << ErrorMajorCount << " major"
                      << kColorReset;
        if (ErrorMinorCount > 0)
        {
            if (ErrorMajorCount > 0)
                std::cout << ", ";
            std::cout << kColorYellow << ErrorMinorCount << " minor"
                      << kColorReset;
        }
        if (WarningCount > 0)
        {
            if (ErrorMajorCount > 0 || ErrorMinorCount > 0)
                std::cout << ", ";
            std::cout << kColorDim << WarningCount << " warning" << kColorReset;
        }
        std::cout << ")\n";
    }
    std::cout << "\n";

    // Only show failures
    if (TotalFailed > 0)
    {
        HumanTable Table;
        Table.mHeaders = {"Severity", "Topic", "Line", "Path", "Detail"};
        for (const ValidateCheck &C : Checks)
        {
            if (C.mbOk)
                continue;
            std::string Sev;
            if (C.mSeverity == EValidationSeverity::ErrorMajor)
                Sev = kColorRed + std::string("ERROR") + kColorReset;
            else if (C.mSeverity == EValidationSeverity::ErrorMinor)
                Sev = kColorYellow + std::string("error") + kColorReset;
            else
                Sev = kColorDim + std::string("warn") + kColorReset;

            std::string Detail = C.mDetail;
            if (Detail.size() > 50)
                Detail = Detail.substr(0, 47) + "...";
            std::string Path = C.mPath;
            if (Path.size() > 35)
                Path = Path.substr(0, 32) + "...";
            const std::string LineCell =
                C.mLine > 0 ? std::to_string(C.mLine) : "-";
            Table.AddRow({Sev, C.mTopic, LineCell,
                          kColorDim + Path + kColorReset, Detail});
        }
        Table.Print();
    }

    const bool bStrictFail =
        InOptions.mbStrict && (ErrorMinorCount > 0 || WarningCount > 0);
    return (ErrorMajorCount > 0 || bStrictFail) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// RunBundleValidateCommand
// ---------------------------------------------------------------------------

int RunBundleValidateCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FBundleValidateOptions Options = ParseBundleValidateOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleValidateHuman(RepoRoot, Options);
    return RunBundleValidateJson(RepoRoot, Options);
}

} // namespace UniPlan
