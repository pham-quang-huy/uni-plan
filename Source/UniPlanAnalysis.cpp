#include "UniPlanTypes.h"
#include "UniPlanDocumentStore.h"
#include "UniPlanHelpers.h"
#include "UniPlanForwardDecls.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

std::string EnsureGraphNode(const std::string &InRelativePath,
                            std::map<std::string, GraphNode> &InOutNodesById)
{
    std::string TopicKey;
    std::string PhaseKey;
    std::string OwnerKind;
    std::string DocKind;
    const std::string NodeType = ClassifyRelativeMarkdownPath(
        InRelativePath, &TopicKey, &PhaseKey, &OwnerKind, &DocKind);
    const std::string NodeId = BuildGraphNodeId(NodeType, InRelativePath);
    if (InOutNodesById.count(NodeId) == 0)
    {
        GraphNode Node;
        Node.mId = NodeId;
        Node.mType = NodeType;
        Node.mPath = InRelativePath;
        Node.mTopicKey = TopicKey;
        Node.mPhaseKey = PhaseKey;
        Node.mOwnerKind = OwnerKind;
        Node.mDocKind = DocKind;
        InOutNodesById.emplace(NodeId, std::move(Node));
    }
    return NodeId;
}

std::set<std::string>
CollectPlanPathReferences(const fs::path &InRepoRoot,
                          const std::string &InRelativePath,
                          std::vector<std::string> &OutWarnings)
{
    const fs::path AbsolutePath = InRepoRoot / fs::path(InRelativePath);
    std::string Text;
    std::string ReadError;
    if (!TryReadFileText(AbsolutePath, Text, ReadError))
    {
        AddWarning(OutWarnings, "Unable to parse plan references in '" +
                                    InRelativePath + "': " + ReadError);
        return {};
    }

    std::set<std::string> Result;
    const std::set<std::string> References =
        ExtractReferencedMarkdownPaths(InRepoRoot, AbsolutePath, Text);
    for (const std::string &Reference : References)
    {
        if (EndsWith(Reference, kExtPlan))
        {
            Result.insert(Reference);
        }
    }
    return Result;
}

std::map<std::string, fs::path>
BuildMarkdownPathMap(const std::vector<MarkdownDocument> &InDocuments)
{
    std::map<std::string, fs::path> Result;
    for (const MarkdownDocument &Document : InDocuments)
    {
        Result[Document.mRelativePath] = Document.mAbsolutePath;
    }
    return Result;
}

void BuildReferenceGraph(
    const fs::path &InRepoRoot,
    const std::map<std::string, fs::path> &InPathMap,
    std::vector<std::string> &InOutWarnings,
    std::map<std::string, std::set<std::string>> &OutOutgoing,
    std::map<std::string, std::set<std::string>> &OutIncoming)
{
    OutOutgoing.clear();
    OutIncoming.clear();

    for (const auto &PathEntry : InPathMap)
    {
        const std::string &SourceRelativePath = PathEntry.first;
        const fs::path &SourceAbsolutePath = PathEntry.second;
        std::string Text;
        std::string ReadError;
        if (!TryReadFileText(SourceAbsolutePath, Text, ReadError))
        {
            AddWarning(InOutWarnings,
                       "Unable to parse markdown references for '" +
                           SourceRelativePath + "': " + ReadError);
            continue;
        }

        const std::set<std::string> References = ExtractReferencedMarkdownPaths(
            InRepoRoot, SourceAbsolutePath, Text);
        for (const std::string &TargetRelativePath : References)
        {
            if (InPathMap.count(TargetRelativePath) == 0)
            {
                continue;
            }
            OutOutgoing[SourceRelativePath].insert(TargetRelativePath);
            OutIncoming[TargetRelativePath].insert(SourceRelativePath);
        }
    }
}

std::vector<std::string> CollectTopicSeedPaths(const Inventory &InInventory,
                                               const std::string &InTopicKey)
{
    std::set<std::string> SeedPaths;

    if (const DocumentRecord *Plan =
            FindSingleRecordByTopic(InInventory.mPlans, InTopicKey))
    {
        SeedPaths.insert(Plan->mPath);
    }
    if (const DocumentRecord *Implementation =
            FindSingleRecordByTopic(InInventory.mImplementations, InTopicKey))
    {
        SeedPaths.insert(Implementation->mPath);
    }

    const std::vector<DocumentRecord> Playbooks =
        CollectRecordsByTopic(InInventory.mPlaybooks, InTopicKey);
    for (const DocumentRecord &Playbook : Playbooks)
    {
        SeedPaths.insert(Playbook.mPath);
    }

    const std::vector<SidecarRecord> Sidecars =
        CollectSidecarsByTopic(InInventory.mSidecars, InTopicKey);
    for (const SidecarRecord &Sidecar : Sidecars)
    {
        SeedPaths.insert(Sidecar.mPath);
    }

    return std::vector<std::string>(SeedPaths.begin(), SeedPaths.end());
}

std::string
GetFirstFieldValue(const std::map<std::string, std::string> &InFields,
                   const std::initializer_list<const char *> &InKeys)
{
    for (const char *Key : InKeys)
    {
        const auto FieldIt = InFields.find(NormalizeHeaderKey(Key));
        if (FieldIt == InFields.end())
        {
            continue;
        }

        const std::string Value = Trim(FieldIt->second);
        if (!Value.empty())
        {
            return Value;
        }
    }
    return "";
}

std::vector<BlockerItem> CollectBlockerItemsFromDocument(
    const fs::path &InRepoRoot, const DocumentRecord &InDocument,
    const std::string &InDocClass, std::vector<std::string> &OutWarnings)
{
    FDocument Doc;
    std::string LoadError;
    if (!TryLoadDocument(InRepoRoot, InDocument.mPath, Doc, LoadError))
    {
        AddWarning(OutWarnings, "Unable to parse blocker rows from '" +
                                    InDocument.mPath + "': " + LoadError);
        return {};
    }

    std::vector<BlockerItem> Result;

    for (const auto &SecPair : Doc.mSections)
    {
        for (const FStructuredTable &Table : SecPair.second.mTables)
        {
            if (Table.mHeaders.empty())
            {
                continue;
            }

            const int StatusIndex =
                FindFirstHeaderIndex(Table.mHeaders, {"status"});
            const int ActionIndex = FindFirstHeaderIndex(
                Table.mHeaders,
                {"next action", "next actions", "action", "remaining scope"});
            if (ActionIndex < 0)
            {
                continue;
            }

            const int PhaseIndex = FindFirstHeaderIndex(
                Table.mHeaders,
                {"phase", "phase key", "lane", "lane key", "workstream"});
            const int PriorityIndex =
                FindFirstHeaderIndex(Table.mHeaders, {"priority"});
            const int OwnerIndex = FindFirstHeaderIndex(
                Table.mHeaders, {"owner", "owners", "lane owner"});
            const int NotesIndex = FindFirstHeaderIndex(
                Table.mHeaders, {"notes", "blocking details", "details",
                                 "evidence", "summary"});

            for (const std::vector<FTableCell> &Row : Table.mRows)
            {
                const auto ReadCell = [&Row](const int InIndex) -> std::string
                {
                    if (InIndex < 0)
                    {
                        return "";
                    }
                    const size_t Index = static_cast<size_t>(InIndex);
                    if (Index >= Row.size())
                    {
                        return "";
                    }
                    return Trim(Row[Index].mValue);
                };

                const std::string Action = ReadCell(ActionIndex);
                if (Action.empty())
                {
                    continue;
                }

                const std::string RawStatus = ReadCell(StatusIndex);
                const std::string NormalizedStatus =
                    NormalizeStatusValue(RawStatus);
                std::string ItemStatus = "open";
                if (NormalizedStatus == "blocked")
                {
                    ItemStatus = "blocked";
                }
                else if (NormalizedStatus == "completed" ||
                         NormalizedStatus == "closed" ||
                         NormalizedStatus == "canceled")
                {
                    ItemStatus = "closed";
                }

                BlockerItem Item;
                Item.mTopicKey = InDocument.mTopicKey;
                Item.mSourcePath = InDocument.mPath;
                Item.mKind = InDocClass + "_table_action";
                Item.mStatus = ItemStatus;
                Item.mPhaseKey = ReadCell(PhaseIndex);
                if (Item.mPhaseKey.empty())
                {
                    Item.mPhaseKey = InDocument.mPhaseKey;
                }
                if (Item.mPhaseKey.empty())
                {
                    Item.mPhaseKey = SecPair.first;
                }
                Item.mPriority = ReadCell(PriorityIndex);
                Item.mAction = Action;
                Item.mOwner = ReadCell(OwnerIndex);
                Item.mNotes = ReadCell(NotesIndex);
                Result.push_back(std::move(Item));
            }
        }
    }

    return Result;
}

bool MatchesPhaseStatusFilter(const std::string &InStatusFilter,
                              const std::string &InItemStatus)
{
    if (InStatusFilter == "all")
    {
        return true;
    }
    return MatchesStatusFilter(InStatusFilter, InItemStatus);
}

static std::string
DerivePhaseStatusFromPlaybook(const fs::path &InRepoRoot,
                              const std::string &InPlaybookPath)
{
    if (InPlaybookPath.empty())
    {
        return "not_started";
    }

    FPhaseRecord Phase;
    std::string LoadError;
    if (!TryLoadPhaseRecord(InRepoRoot, InPlaybookPath, Phase, LoadError))
    {
        return "unknown";
    }

    if (Phase.mLifecycle.mStatus != EExecutionStatus::NotStarted)
    {
        return ToString(Phase.mLifecycle.mStatus);
    }

    // Fallback: derive from lane statuses
    StatusCounters Counters;
    for (const FLaneRecord &Lane : Phase.mLanes)
    {
        AddStatusCandidate(Counters, ToString(Lane.mStatus));
    }
    return ResolveNormalizedStatus(Counters);
}

std::vector<PhaseItem> CollectPhaseItemsFromPlan(
    const fs::path &InRepoRoot, const DocumentRecord &InPlan,
    const std::vector<DocumentRecord> &InPlaybooks,
    const std::string &InStatusFilter, std::vector<std::string> &OutWarnings)
{
    // Load plan via document store (bundle-aware)
    FDocument PlanDoc;
    std::string LoadError;
    if (!TryLoadDocument(InRepoRoot, InPlan.mPath, PlanDoc, LoadError))
    {
        throw std::runtime_error("Failed to read plan '" + InPlan.mPath +
                                 "': " + LoadError);
    }

    std::map<std::string, std::string> PlaybookPathByPhase;
    for (const DocumentRecord &Playbook : InPlaybooks)
    {
        if (Playbook.mTopicKey != InPlan.mTopicKey)
        {
            continue;
        }
        if (PlaybookPathByPhase.count(Playbook.mPhaseKey) == 0)
        {
            PlaybookPathByPhase.emplace(Playbook.mPhaseKey, Playbook.mPath);
        }
    }

    // Find implementation_phases table in any section
    std::vector<PhaseItem> Result;
    bool FoundImplementationPhases = false;

    for (const auto &SecPair : PlanDoc.mSections)
    {
        for (const FStructuredTable &Table : SecPair.second.mTables)
        {
            const int PhaseIndex = FindHeaderIndex(Table.mHeaders, "phase");
            if (PhaseIndex < 0)
            {
                continue;
            }

            // Check if this looks like an implementation_phases table
            // (has Phase header + Scope/Output/Description headers)
            const int DescriptionIndex =
                FindFirstHeaderIndex(Table.mHeaders, {"description", "scope",
                                                      "goal", "focus", "name"});
            const int NextActionIndex = FindFirstHeaderIndex(
                Table.mHeaders, {"next action", "next_action", "next actions",
                                 "output", "deliverables", "main tasks",
                                 "exit criteria", "deliverable", "work"});

            if (DescriptionIndex < 0 && NextActionIndex < 0)
            {
                continue;
            }

            FoundImplementationPhases = true;

            for (size_t RowIndex = 0; RowIndex < Table.mRows.size(); ++RowIndex)
            {
                const std::vector<FTableCell> &Row = Table.mRows[RowIndex];
                const size_t PhaseCellIndex = static_cast<size_t>(PhaseIndex);
                if (PhaseCellIndex >= Row.size())
                {
                    continue;
                }

                const std::string PhaseKey =
                    ExtractPhaseKeyFromCell(Row[PhaseCellIndex].mValue);
                if (PhaseKey.empty())
                {
                    continue;
                }

                const auto PlaybookIt = PlaybookPathByPhase.find(PhaseKey);
                const std::string PlaybookPath =
                    (PlaybookIt != PlaybookPathByPhase.end())
                        ? PlaybookIt->second
                        : "";
                const std::string Status =
                    DerivePhaseStatusFromPlaybook(InRepoRoot, PlaybookPath);
                if (!MatchesPhaseStatusFilter(InStatusFilter, Status))
                {
                    continue;
                }

                PhaseItem Item;
                Item.mPhaseKey = PhaseKey;
                Item.mStatusRaw = Status;
                Item.mStatus = Status;
                Item.mTableId = Table.mTableID;
                Item.mRowIndex = static_cast<int>(RowIndex) + 1;
                Item.mPlaybookPath = PlaybookPath;

                if (DescriptionIndex >= 0)
                {
                    const size_t DI = static_cast<size_t>(DescriptionIndex);
                    if (DI < Row.size())
                    {
                        Item.mDescription = Trim(Row[DI].mValue);
                    }
                }

                if (NextActionIndex >= 0)
                {
                    const size_t NI = static_cast<size_t>(NextActionIndex);
                    if (NI < Row.size())
                    {
                        Item.mNextAction = Trim(Row[NI].mValue);
                    }
                }

                for (size_t HI = 0; HI < Table.mHeaders.size(); ++HI)
                {
                    const std::string Value =
                        (HI < Row.size()) ? Trim(Row[HI].mValue) : "";
                    Item.mFields.emplace_back(Table.mHeaders[HI], Value);
                }
                Result.push_back(std::move(Item));
            }
        }
    }

    if (!FoundImplementationPhases)
    {
        AddWarning(OutWarnings,
                   "No implementation_phases table found in plan '" +
                       InPlan.mPath + "'.");
    }

    return Result;
}

std::vector<PhaseListAllEntry>
BuildPhaseListAll(const fs::path &InRepoRoot, const Inventory &InInventory,
                  const std::string &InStatusFilter,
                  std::vector<std::string> &OutWarnings)
{
    std::vector<PhaseListAllEntry> Entries;
    Entries.reserve(InInventory.mPlans.size());

    for (const DocumentRecord &Plan : InInventory.mPlans)
    {
        PhaseListAllEntry Entry;
        Entry.mTopicKey = Plan.mTopicKey;
        Entry.mPlanPath = Plan.mPath;
        Entry.mPlanStatus = Plan.mStatus;
        Entry.mPhases =
            CollectPhaseItemsFromPlan(InRepoRoot, Plan, InInventory.mPlaybooks,
                                      InStatusFilter, OutWarnings);

        if (Entry.mPhases.empty())
        {
            continue;
        }
        Entries.push_back(std::move(Entry));
    }

    std::sort(Entries.begin(), Entries.end(),
              [](const PhaseListAllEntry &InA, const PhaseListAllEntry &InB)
              { return InA.mTopicKey < InB.mTopicKey; });

    return Entries;
}

bool MatchesBlockerStatusFilter(const std::string &InStatusFilter,
                                const std::string &InItemStatus)
{
    if (InStatusFilter == "all")
    {
        return true;
    }
    return MatchesStatusFilter(InStatusFilter, InItemStatus);
}

int BlockerStatusRank(const std::string &InStatus)
{
    if (InStatus == "blocked")
    {
        return 0;
    }
    if (InStatus == "open")
    {
        return 1;
    }
    return 2;
}

} // namespace UniPlan
