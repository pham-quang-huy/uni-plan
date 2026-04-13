#include "UniPlanDocumentStore.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJson.h"
#include "UniPlanMutation.h"
#include "UniPlanTypes.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// evidence add — append changelog/verification entry to sidecar
// ---------------------------------------------------------------------------

static int RunEvidenceAdd(const std::string &InRepoRoot,
                          const std::string &InTopic,
                          const std::string &InDocClass,
                          const std::string &InPhaseKey,
                          const std::string &InType, const std::string &InDate,
                          const std::string &InUpdate)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);
    const Inventory Inv = BuildInventory(RepoRoot.string(), true, "", false);

    const std::string TopicKey = ResolveTopicKeyFromInventory(Inv, InTopic);
    if (TopicKey.empty())
    {
        std::cout << "{\"ok\":false,\"error\":\"Topic not "
                     "found\"}\n";
        return 1;
    }

    // Find the sidecar
    std::string SidecarPath;
    for (const SidecarRecord &SC : Inv.mSidecars)
    {
        if (SC.mTopicKey != TopicKey)
            continue;
        const std::string DocKindLower = ToLower(SC.mDocKind);
        if (InType == "changelog" && DocKindLower != "changelog")
            continue;
        if (InType == "verification" && DocKindLower != "verification")
            continue;
        const std::string OwnerLower = ToLower(SC.mOwnerKind);
        if (OwnerLower != InDocClass)
            continue;
        if (InDocClass == "playbook" && SC.mPhaseKey != InPhaseKey)
            continue;
        SidecarPath = SC.mPath;
        break;
    }

    if (SidecarPath.empty())
    {
        std::cout << "{\"ok\":false,\"error\":\"Sidecar not "
                     "found\"}\n";
        return 1;
    }

    // Load sidecar, append entry to the entries section
    FDocument Doc;
    std::string Error;
    if (!TryLoadDocument(RepoRoot, SidecarPath, Doc, Error))
    {
        std::cout << "{\"ok\":false,\"error\":" << JsonQuote(Error) << "}\n";
        return 1;
    }

    // Find or create entries section
    auto &Entries = Doc.mSections["entries"];
    if (Entries.mSectionID.empty())
    {
        Entries.mSectionID = "entries";
        Entries.mHeading = "entries";
        Entries.mLevel = 2;
    }

    // Append as a table row if there's a table, else create one
    if (Entries.mTables.empty())
    {
        FStructuredTable Table;
        Table.mTableID = 0;
        Table.mSectionID = "entries";
        Table.mHeaders = {"Date", "Update", "Evidence"};
        Entries.mTables.push_back(std::move(Table));
    }

    FStructuredTable &Table = Entries.mTables[0];
    std::vector<FTableCell> Row;
    Row.push_back(FTableCell{InDate});
    Row.push_back(FTableCell{InUpdate});
    Row.push_back(FTableCell{""});
    Table.mRows.push_back(std::move(Row));

    if (!TrySaveDocument(RepoRoot, Doc, Error))
    {
        std::cout << "{\"ok\":false,\"error\":" << JsonQuote(Error) << "}\n";
        return 1;
    }

    std::cout << "{\"ok\":true,\"sidecar_path\":" << JsonQuote(SidecarPath)
              << ",\"entries\":" << static_cast<int>(Table.mRows.size())
              << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// tag set/get/query — file-based tag system using Docs/.tags.json
// ---------------------------------------------------------------------------

static fs::path GetTagFilePath(const fs::path &InRepoRoot)
{
    return InRepoRoot / "Docs" / ".tags.json";
}

static JsonValue LoadTagFile(const fs::path &InTagPath)
{
    std::ifstream Stream(InTagPath);
    if (!Stream.is_open())
    {
        JsonValue Root = JsonValue::object();
        Root["schema"] = "uni-plan-tags-v1";
        Root["entities"] = JsonValue::object();
        return Root;
    }
    try
    {
        return JsonValue::parse(Stream);
    }
    catch (...)
    {
        JsonValue Root = JsonValue::object();
        Root["schema"] = "uni-plan-tags-v1";
        Root["entities"] = JsonValue::object();
        return Root;
    }
}

static bool SaveTagFile(const fs::path &InTagPath, const JsonValue &InRoot,
                        std::string &OutError)
{
    std::error_code DirError;
    fs::create_directories(InTagPath.parent_path(), DirError);
    if (DirError)
    {
        OutError = "Failed to create directory: " + DirError.message();
        return false;
    }
    std::ofstream Stream(InTagPath, std::ios::out | std::ios::trunc);
    if (!Stream.is_open())
    {
        OutError = "Failed to open tag file for writing";
        return false;
    }
    Stream << InRoot.dump(2) << "\n";
    Stream.close();
    if (Stream.fail())
    {
        OutError = "Write failed for tag file";
        return false;
    }
    return true;
}

static int RunTagSet(const fs::path &InRepoRoot, const std::string &InTarget,
                     const std::string &InTagsStr)
{
    if (InTarget.empty())
    {
        std::cout << "{\"ok\":false,\"error\":\"--target is "
                     "required\"}\n";
        return 2;
    }

    const fs::path TagPath = GetTagFilePath(InRepoRoot);
    JsonValue Root = LoadTagFile(TagPath);

    // Parse comma-separated tags
    JsonValue TagArray = JsonValue::array();
    std::istringstream Stream(InTagsStr);
    std::string Tag;
    while (std::getline(Stream, Tag, ','))
    {
        const std::string Trimmed = Trim(Tag);
        if (!Trimmed.empty())
        {
            TagArray.push_back(ToLower(Trimmed));
        }
    }

    Root["entities"][InTarget]["tags"] = TagArray;
    Root["entities"][InTarget]["updated_utc"] = GetUtcNow();

    std::string Error;
    if (!SaveTagFile(TagPath, Root, Error))
    {
        std::cout << "{\"ok\":false,\"error\":" << JsonQuote(Error) << "}\n";
        return 1;
    }

    std::cout << "{\"ok\":true,\"target\":" << JsonQuote(InTarget)
              << ",\"tags\":" << TagArray.dump() << "}\n";
    return 0;
}

static int RunTagGet(const fs::path &InRepoRoot, const std::string &InTarget)
{
    const fs::path TagPath = GetTagFilePath(InRepoRoot);
    const JsonValue Root = LoadTagFile(TagPath);

    if (Root.contains("entities") && Root["entities"].contains(InTarget))
    {
        const auto &Entity = Root["entities"][InTarget];
        const auto Tags = Entity.value("tags", JsonValue::array());
        std::cout << "{\"target\":" << JsonQuote(InTarget)
                  << ",\"tags\":" << Tags.dump() << "}\n";
    }
    else
    {
        std::cout << "{\"target\":" << JsonQuote(InTarget) << ",\"tags\":[]}\n";
    }
    return 0;
}

static int RunTagQuery(const fs::path &InRepoRoot, const std::string &InTagsStr,
                       const std::string &InMode)
{
    const fs::path TagPath = GetTagFilePath(InRepoRoot);
    const JsonValue Root = LoadTagFile(TagPath);

    // Parse query tags
    std::vector<std::string> QueryTags;
    std::istringstream Stream(InTagsStr);
    std::string Tag;
    while (std::getline(Stream, Tag, ','))
    {
        const std::string Trimmed = Trim(ToLower(Tag));
        if (!Trimmed.empty())
        {
            QueryTags.push_back(Trimmed);
        }
    }

    const bool ModeAnd = (InMode == "and");

    std::cout << "{\"query_tags\":[";
    for (size_t Index = 0; Index < QueryTags.size(); ++Index)
    {
        if (Index > 0)
            std::cout << ",";
        std::cout << JsonQuote(QueryTags[Index]);
    }
    std::cout << "],\"mode\":" << JsonQuote(InMode) << ",\"matches\":[";

    bool First = true;
    if (Root.contains("entities") && Root["entities"].is_object())
    {
        for (const auto &Item : Root["entities"].items())
        {
            const auto Tags = Item.value().value("tags", JsonValue::array());
            std::set<std::string> TagSet;
            for (const auto &T : Tags)
            {
                if (T.is_string())
                    TagSet.insert(T.get<std::string>());
            }

            bool Matches = false;
            if (ModeAnd)
            {
                Matches = true;
                for (const std::string &QTag : QueryTags)
                {
                    if (TagSet.count(QTag) == 0)
                    {
                        Matches = false;
                        break;
                    }
                }
            }
            else
            {
                for (const std::string &QTag : QueryTags)
                {
                    if (TagSet.count(QTag) > 0)
                    {
                        Matches = true;
                        break;
                    }
                }
            }

            if (Matches)
            {
                if (!First)
                    std::cout << ",";
                std::cout << "{\"target\":" << JsonQuote(Item.key())
                          << ",\"tags\":" << Tags.dump() << "}";
                First = false;
            }
        }
    }

    std::cout << "]}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// Public entry point — parses subcommands and dispatches
// ---------------------------------------------------------------------------

int RunEvidenceCommand(const std::vector<std::string> &InArgs,
                       const bool InUseCache, const DocConfig &InConfig)
{
    if (InArgs.empty())
    {
        std::cerr << "Usage: uni-plan evidence add --topic "
                     "<topic> --for <plan|impl|playbook> "
                     "--type <changelog|verification> "
                     "--date <date> --update <text>\n";
        return 2;
    }

    std::string Subcommand = InArgs[0];
    std::string Topic, DocClass, PhaseKey, Type, Date, Update;
    std::string RepoRoot;

    for (size_t Index = 1; Index < InArgs.size(); ++Index)
    {
        const std::string &T = InArgs[Index];
        if (T == "--topic" && Index + 1 < InArgs.size())
            Topic = InArgs[++Index];
        else if (T == "--for" && Index + 1 < InArgs.size())
            DocClass = ToLower(InArgs[++Index]);
        else if (T == "--phase" && Index + 1 < InArgs.size())
            PhaseKey = InArgs[++Index];
        else if (T == "--type" && Index + 1 < InArgs.size())
            Type = ToLower(InArgs[++Index]);
        else if (T == "--date" && Index + 1 < InArgs.size())
            Date = InArgs[++Index];
        else if (T == "--update" && Index + 1 < InArgs.size())
            Update = InArgs[++Index];
        else if (T == "--repo-root" && Index + 1 < InArgs.size())
            RepoRoot = InArgs[++Index];
    }

    if (Subcommand == "add")
    {
        return RunEvidenceAdd(RepoRoot, Topic, DocClass, PhaseKey, Type, Date,
                              Update);
    }

    std::cerr << "Unknown evidence subcommand: " << Subcommand << "\n";
    return 2;
}

int RunTagCommand(const std::vector<std::string> &InArgs)
{
    if (InArgs.empty())
    {
        std::cerr << "Usage: uni-plan tag <set|get|query> "
                     "[options]\n";
        return 2;
    }

    std::string Subcommand = InArgs[0];
    std::string Target, Tags, Mode = "or", RepoRoot;

    for (size_t Index = 1; Index < InArgs.size(); ++Index)
    {
        const std::string &T = InArgs[Index];
        if (T == "--target" && Index + 1 < InArgs.size())
            Target = InArgs[++Index];
        else if (T == "--tags" && Index + 1 < InArgs.size())
            Tags = InArgs[++Index];
        else if (T == "--mode" && Index + 1 < InArgs.size())
            Mode = ToLower(InArgs[++Index]);
        else if (T == "--repo-root" && Index + 1 < InArgs.size())
            RepoRoot = InArgs[++Index];
    }

    const fs::path Root = NormalizeRepoRootPath(RepoRoot);

    if (Subcommand == "set")
    {
        return RunTagSet(Root, Target, Tags);
    }
    if (Subcommand == "get")
    {
        return RunTagGet(Root, Target);
    }
    if (Subcommand == "query")
    {
        return RunTagQuery(Root, Tags, Mode);
    }

    std::cerr << "Unknown tag subcommand: " << Subcommand << "\n";
    return 2;
}

} // namespace UniPlan
