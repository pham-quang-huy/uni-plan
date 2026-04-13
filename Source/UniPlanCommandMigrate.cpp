#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanMigrate.h"
#include "UniPlanTypes.h"

#include <iostream>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// migrate md-to-json — convert .md documents to .json
// ---------------------------------------------------------------------------

static int RunMigrateToJsonSingleDoc(const fs::path &InRepoRoot,
                                     const std::string &InDocPath)
{
    std::string JsonPath;
    std::string Error;
    if (!TryMigrateFileToJson(InRepoRoot, InDocPath, JsonPath, Error))
    {
        std::cout << "{\"schema\":\"uni-plan-migrate-v1\""
                  << ",\"ok\":false"
                  << ",\"error\":" << JsonQuote(Error) << "}\n";
        return 1;
    }

    std::cout << "{\"schema\":\"uni-plan-migrate-v1\""
              << ",\"ok\":true"
              << ",\"source\":" << JsonQuote(InDocPath)
              << ",\"output\":" << JsonQuote(JsonPath) << "}\n";
    return 0;
}

static int RunMigrateToJsonTopic(const fs::path &InRepoRoot,
                                 const std::string &InTopic,
                                 const bool InUseCache,
                                 const DocConfig &InConfig)
{
    const Inventory Inv =
        BuildInventory(InRepoRoot.string(), InUseCache, InConfig.mCacheDir,
                       InConfig.mbCacheVerbose);

    const std::string TopicKey = ResolveTopicKeyFromInventory(Inv, InTopic);
    if (TopicKey.empty())
    {
        std::cout << "{\"schema\":\"uni-plan-migrate-v1\""
                  << ",\"ok\":false"
                  << ",\"error\":\"Topic not found: " << InTopic << "\"}\n";
        return 1;
    }

    const FMigrateTopicResult Result =
        MigrateTopicToJson(InRepoRoot, Inv, TopicKey);

    std::cout << "{\"schema\":\"uni-plan-migrate-v1\""
              << ",\"ok\":" << (Result.mFailed == 0 ? "true" : "false")
              << ",\"topic_key\":" << JsonQuote(Result.mTopicKey)
              << ",\"converted\":" << Result.mConverted
              << ",\"failed\":" << Result.mFailed;

    std::cout << ",\"converted_paths\":[";
    for (size_t Index = 0; Index < Result.mConvertedPaths.size(); ++Index)
    {
        if (Index > 0)
        {
            std::cout << ",";
        }
        std::cout << JsonQuote(Result.mConvertedPaths[Index]);
    }
    std::cout << "]";

    if (!Result.mErrors.empty())
    {
        std::cout << ",\"errors\":[";
        for (size_t Index = 0; Index < Result.mErrors.size(); ++Index)
        {
            if (Index > 0)
            {
                std::cout << ",";
            }
            std::cout << JsonQuote(Result.mErrors[Index]);
        }
        std::cout << "]";
    }

    std::cout << "}\n";
    return Result.mFailed > 0 ? 1 : 0;
}

static int RunMigrateToJsonAll(const fs::path &InRepoRoot,
                               const bool InUseCache, const DocConfig &InConfig)
{
    const Inventory Inv =
        BuildInventory(InRepoRoot.string(), InUseCache, InConfig.mCacheDir,
                       InConfig.mbCacheVerbose);

    int TotalConverted = 0;
    int TotalFailed = 0;
    std::vector<std::string> AllErrors;

    // Collect unique topic keys
    std::set<std::string> TopicKeys;
    for (const DocumentRecord &Plan : Inv.mPlans)
    {
        TopicKeys.insert(Plan.mTopicKey);
    }
    for (const DocumentRecord &Impl : Inv.mImplementations)
    {
        TopicKeys.insert(Impl.mTopicKey);
    }

    for (const std::string &Key : TopicKeys)
    {
        const FMigrateTopicResult Result =
            MigrateTopicToJson(InRepoRoot, Inv, Key);
        TotalConverted += Result.mConverted;
        TotalFailed += Result.mFailed;
        for (const std::string &Err : Result.mErrors)
        {
            AllErrors.push_back(Err);
        }
    }

    std::cout << "{\"schema\":\"uni-plan-migrate-v1\""
              << ",\"ok\":" << (TotalFailed == 0 ? "true" : "false")
              << ",\"converted\":" << TotalConverted
              << ",\"failed\":" << TotalFailed
              << ",\"topic_count\":" << static_cast<int>(TopicKeys.size());

    if (!AllErrors.empty())
    {
        std::cout << ",\"errors\":[";
        for (size_t Index = 0; Index < AllErrors.size(); ++Index)
        {
            if (Index > 0)
            {
                std::cout << ",";
            }
            std::cout << JsonQuote(AllErrors[Index]);
        }
        std::cout << "]";
    }

    std::cout << "}\n";
    return TotalFailed > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// migrate status — show migration progress
// ---------------------------------------------------------------------------

static int RunMigrateStatus(const fs::path &InRepoRoot)
{
    const FMigrateStatusResult Status = ComputeMigrateStatus(InRepoRoot);

    std::cout << "{\"schema\":\"uni-plan-migrate-status-v1\""
              << ",\"total\":" << Status.mTotalDocuments
              << ",\"json\":" << Status.mJsonDocuments
              << ",\"markdown\":" << Status.mMarkdownDocuments << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

int RunMigrateJson(const MigrateOptions &InOptions, const bool InUseCache,
                   const DocConfig &InConfig)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InOptions.mRepoRoot);

    if (InOptions.mSubcommand == "md-to-json")
    {
        if (!InOptions.mDocPath.empty())
        {
            return RunMigrateToJsonSingleDoc(RepoRoot, InOptions.mDocPath);
        }
        if (!InOptions.mTopic.empty())
        {
            return RunMigrateToJsonTopic(RepoRoot, InOptions.mTopic, InUseCache,
                                         InConfig);
        }
        if (InOptions.mbAll)
        {
            return RunMigrateToJsonAll(RepoRoot, InUseCache, InConfig);
        }
        std::cerr << "migrate md-to-json requires --doc, --topic, "
                     "or --all\n";
        return 2;
    }

    if (InOptions.mSubcommand == "status")
    {
        return RunMigrateStatus(RepoRoot);
    }

    std::cerr << "Unknown migrate subcommand: " << InOptions.mSubcommand
              << "\nSupported: md-to-json, status\n";
    return 2;
}

} // namespace UniPlan
