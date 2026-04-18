#include "UniPlanRuntime.h"
#include "UniPlanForwardDecls.h"
#ifdef UPLAN_WATCH
#include "UniPlanWatchApp.h"
#endif
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace UniPlan
{

// kMarkdownPathRegex moved to DocParsing.cpp (declared in DocForwardDecls.h)

CacheConfigResult WriteCacheConfig(const std::string &InRepoRoot,
                                   const CacheConfigOptions &InOptions,
                                   const DocConfig &InCurrentConfig)
{
    CacheConfigResult Result;
    Result.mGeneratedUtc = GetUtcNow();

    const fs::path ExeDir = ResolveExecutableDirectory();
    const fs::path IniPath = ExeDir / "uni-plan.ini";
    Result.mIniPath = IniPath.string();

    // Start with current effective values
    std::string EffectiveDir = InCurrentConfig.mCacheDir;
    std::string EffectiveEnabled =
        InCurrentConfig.mbCacheEnabled ? "true" : "false";
    std::string EffectiveVerbose =
        InCurrentConfig.mbCacheVerbose ? "true" : "false";

    // Merge only explicitly-set fields (mbDirSet distinguishes "not passed"
    // from "set to empty")
    if (InOptions.mbDirSet)
    {
        EffectiveDir = InOptions.mDir;
    }
    if (!InOptions.mEnabled.empty())
    {
        EffectiveEnabled = InOptions.mEnabled;
    }
    if (!InOptions.mVerbose.empty())
    {
        EffectiveVerbose = InOptions.mVerbose;
    }

    std::string WriteError;
    if (!TryWriteDocIni(IniPath, EffectiveDir, EffectiveEnabled,
                        EffectiveVerbose, WriteError))
    {
        Result.mbSuccess = false;
        Result.mError = WriteError;
    }

    // Re-read to get effective config
    const DocConfig NewConfig = LoadConfig(ExeDir);
    Result.mDir = NewConfig.mCacheDir;
    Result.mbEnabled = NewConfig.mbCacheEnabled;
    Result.mbVerbose = NewConfig.mbCacheVerbose;

    return Result;
}


const std::vector<DocumentRecord> &
ResolveRecordsByKind(const Inventory &InInventory, const std::string &InKind)
{
    if (InKind == "plan")
    {
        return InInventory.mPlans;
    }
    if (InKind == "playbook")
    {
        return InInventory.mPlaybooks;
    }
    if (InKind == "implementation")
    {
        return InInventory.mImplementations;
    }
    throw UsageError("Invalid value for --type: " + InKind);
}

std::vector<const TopicPairRecord *>
FilterPairsByStatus(const std::vector<TopicPairRecord> &InPairs,
                    const std::string &InStatusFilter)
{
    const bool bFilterActive = (InStatusFilter != "all");
    std::vector<const TopicPairRecord *> Result;
    for (const TopicPairRecord &Pair : InPairs)
    {
        if (bFilterActive &&
            !MatchesStatusFilter(InStatusFilter,
                                 GetDisplayStatus(Pair.mOverallStatus)))
        {
            continue;
        }
        Result.push_back(&Pair);
    }
    return Result;
}

std::vector<const DocumentRecord *>
FilterRecordsByStatus(const std::vector<DocumentRecord> &InRecords,
                      const std::string &InStatusFilter)
{
    const bool bFilterActive = (InStatusFilter != "all");
    std::vector<const DocumentRecord *> Result;
    for (const DocumentRecord &Record : InRecords)
    {
        if (bFilterActive &&
            !MatchesStatusFilter(InStatusFilter,
                                 GetDisplayStatus(Record.mStatus)))
        {
            continue;
        }
        Result.push_back(&Record);
    }
    return Result;
}

std::vector<SchemaField>
ParseSchemaFields(const fs::path &InSchemaPath,
                  std::vector<std::string> &OutWarnings)
{
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(InSchemaPath, Lines, ReadError))
    {
        throw std::runtime_error("Failed to read schema file '" +
                                 InSchemaPath.string() + "': " + ReadError);
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);
    std::vector<SchemaField> Fields;

    for (const MarkdownTableRecord &Table : Tables)
    {
        if (Table.mHeaders.size() < 2)
        {
            continue;
        }

        const std::string Header0 = ToLower(Trim(Table.mHeaders[0]));
        const std::string Header1 = ToLower(Trim(Table.mHeaders[1]));
        if (Header0 != "property" || Header1 != "value")
        {
            continue;
        }

        for (const std::vector<std::string> &Row : Table.mRows)
        {
            if (Row.size() < 2)
            {
                continue;
            }
            SchemaField Field;
            Field.mSectionID = Table.mSectionID;
            Field.mProperty = Row[0];
            Field.mValue = Row[1];
            Fields.push_back(std::move(Field));
        }
    }

    if (Fields.empty())
    {
        AddWarning(OutWarnings, "No `Property|Value` schema tables found in '" +
                                    ToGenericPath(InSchemaPath) + "'.");
    }
    return Fields;
}





std::string JoinMarkdownRowCells(const std::vector<std::string> &InRow)
{
    std::ostringstream Stream;
    for (size_t Index = 0; Index < InRow.size(); ++Index)
    {
        if (Index > 0)
        {
            Stream << " | ";
        }
        Stream << Trim(InRow[Index]);
    }
    return Stream.str();
}

bool RowContainsAllTerms(const std::vector<std::string> &InRow,
                         const std::vector<std::string> &InTerms)
{
    if (InTerms.empty())
    {
        return true;
    }

    const std::string RowLower = ToLower(JoinMarkdownRowCells(InRow));
    for (const std::string &Term : InTerms)
    {
        if (RowLower.find(ToLower(Term)) == std::string::npos)
        {
            return false;
        }
    }
    return true;
}

bool TryResolveRuleProvenance(const fs::path &InRepoRoot,
                              const RuleProvenanceProbe &InProbe,
                              RuleEntry &InOutRule,
                              std::vector<std::string> &OutWarnings)
{
    const fs::path AbsolutePath = InRepoRoot / fs::path(InProbe.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError))
    {
        AddWarning(OutWarnings, "Rule provenance read failed for '" +
                                    InProbe.mPath + "' (`" + InOutRule.mID +
                                    "`): " + ReadError);
        return false;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);
    const std::string TargetSectionID = NormalizeSectionID(InProbe.mSectionID);

    for (const MarkdownTableRecord &Table : Tables)
    {
        if (!TargetSectionID.empty() && Table.mSectionID != TargetSectionID)
        {
            continue;
        }

        for (size_t RowIndex = 0; RowIndex < Table.mRows.size(); ++RowIndex)
        {
            const std::vector<std::string> &Row = Table.mRows[RowIndex];
            if (!RowContainsAllTerms(Row, InProbe.mRowTerms))
            {
                continue;
            }

            InOutRule.mbSourceResolved = true;
            InOutRule.mSourcePath = InProbe.mPath;
            InOutRule.mSourceSectionID = Table.mSectionID;
            InOutRule.mSourceTableID = Table.mTableID;
            InOutRule.mSourceRowIndex = static_cast<int>(RowIndex) + 1;
            InOutRule.mSourceEvidence = JoinMarkdownRowCells(Row);
            if (!InOutRule.mSourceSectionID.empty())
            {
                InOutRule.mSource =
                    InOutRule.mSourcePath + "#" + InOutRule.mSourceSectionID;
            }
            else
            {
                InOutRule.mSource = InOutRule.mSourcePath;
            }
            return true;
        }
    }

    AddWarning(OutWarnings, "Rule provenance unresolved for `" + InOutRule.mID +
                                "` in '" + InProbe.mPath + "' section '" +
                                InProbe.mSectionID + "'.");
    return false;
}

std::vector<RuleEntry> BuildRules(const fs::path &InRepoRoot,
                                  std::vector<std::string> &OutWarnings)
{
    std::vector<RuleEntry> Rules = {
        {"plan_impl_pairing",
         "Active `<TopicPascalCase>.Plan.md` docs must have paired "
         "`<TopicPascalCase>.Impl.md` trackers.",
         "CLAUDE.md#pairing_rules"},
        {"active_phase_playbook_required",
         "Every active plan phase must have a dedicated "
         "`<TopicPascalCase>.<PhaseKey>.Playbook.md`.",
         "CLAUDE.md#pairing_rules"},
        {"phase_entry_gate",
         "Before a phase is marked in progress, complete investigation and "
         "prepare the phase playbook execution lanes (and `testing` for "
         "testable "
         "phases).",
         "CLAUDE.md#pairing_rules"},
        {"artifact_role_boundary",
         "Keep artifact ownership explicit: playbook is procedure, "
         "implementation is outcomes, sidecars are evidence history.",
         "CLAUDE.md#pairing_rules"},
        {"canonical_naming",
         "Use canonical naming for plan, implementation, playbook, and sidecar "
         "docs.",
         "CLAUDE.md#document_type_naming"},
        {"canonical_placement",
         "Keep lifecycle artifacts in canonical scope folders (`Docs/Plans`, "
         "`Docs/Playbooks`, `Docs/Implementation`).",
         "PATTERNS.md#pattern-p-doc-plan-artifact-bundle"},
        {"detached_evidence_sidecars",
         "Keep long-running change/verification history in detached sidecars "
         "and "
         "keep core docs concise.",
         "PATTERNS.md#pattern-p-doc-plan-artifact-bundle"},
        {"doc_lint_required",
         "Run uni-plan lint after documentation tasks (`uni-plan lint`).",
         "CLAUDE.md#doc_lint_commands"}};

    const std::vector<RuleProvenanceProbe> Probes = {
        {"CLAUDE.md", "pairing_rules", {"paired artifacts"}},
        {"CLAUDE.md", "pairing_rules", {"playbook-first"}},
        {"CLAUDE.md", "pairing_rules", {"phase entry gates"}},
        {"CLAUDE.md", "pairing_rules", {"artifact boundary"}},
        {"CLAUDE.md", "document_type_naming", {"plan", ".plan.md"}},
        {"PATTERNS.md",
         "pattern_p_doc_plan_artifact_bundle",
         {"canonical scope folders"}},
        {"PATTERNS.md",
         "pattern_p_doc_plan_artifact_bundle",
         {"detached sidecars"}},
        {"CLAUDE.md", "doc_lint_commands", {"uni-plan lint", "yes"}}};

    if (Rules.size() != Probes.size())
    {
        AddWarning(OutWarnings, "Rule probe configuration mismatch; provenance "
                                "fallback is active.");
        return Rules;
    }

    for (size_t Index = 0; Index < Rules.size(); ++Index)
    {
        TryResolveRuleProvenance(InRepoRoot, Probes[Index], Rules[Index],
                                 OutWarnings);
    }
    return Rules;
}

bool HasIndexedHeadingPrefix(const std::string &InHeadingText)
{
    static const std::regex IndexedPrefixRegex(R"(^\s*\d+\s*[\.\)])");
    return std::regex_search(InHeadingText, IndexedPrefixRegex);
}

std::string BuildTopicPhaseIdentityNormalized(const std::string &InTopicKey,
                                              const std::string &InPhaseKey)
{
    return ToLower(Trim(InTopicKey)) + "::" + ToLower(Trim(InPhaseKey));
}

std::string ExtractPhaseKeyFromCell(const std::string &InCellValue)
{
    const std::string CellValue = Trim(InCellValue);
    const size_t TickStart = CellValue.find('`');
    if (TickStart != std::string::npos)
    {
        const size_t TickEnd = CellValue.find('`', TickStart + 1);
        if (TickEnd != std::string::npos && TickEnd > TickStart + 1)
        {
            return Trim(
                CellValue.substr(TickStart + 1, TickEnd - TickStart - 1));
        }
    }

    static const std::regex TokenPattern(R"(([A-Za-z][A-Za-z0-9_-]*))");
    std::smatch Match;
    if (std::regex_search(CellValue, Match, TokenPattern))
    {
        return Trim(Match[1].str());
    }
    return "";
}

std::vector<ActivePhaseRecord>
CollectActivePhaseRecords(const fs::path &InRepoRoot,
                          const std::vector<DocumentRecord> &InPlans,
                          std::vector<std::string> &OutWarnings)
{
    std::vector<ActivePhaseRecord> ActivePhases;
    for (const DocumentRecord &Plan : InPlans)
    {
        const fs::path AbsolutePath = InRepoRoot / fs::path(Plan.mPath);
        std::vector<std::string> Lines;
        std::string ReadError;
        if (!TryReadFileLines(AbsolutePath, Lines, ReadError))
        {
            AddWarning(OutWarnings, "Active-phase parse skipped for '" +
                                        Plan.mPath + "': " + ReadError);
            continue;
        }

        const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
        const std::vector<MarkdownTableRecord> Tables =
            ParseMarkdownTables(Lines, Headings);
        for (const MarkdownTableRecord &Table : Tables)
        {
            if (Table.mSectionID != "implementation_phases")
            {
                continue;
            }

            const int PhaseIndex = FindHeaderIndex(Table.mHeaders, "phase");
            const int StatusIndex = FindHeaderIndex(Table.mHeaders, "status");
            if (PhaseIndex < 0 || StatusIndex < 0)
            {
                continue;
            }

            for (const std::vector<std::string> &Row : Table.mRows)
            {
                const size_t PhaseCellIndex = static_cast<size_t>(PhaseIndex);
                const size_t StatusCellIndex = static_cast<size_t>(StatusIndex);
                if (PhaseCellIndex >= Row.size() ||
                    StatusCellIndex >= Row.size())
                {
                    continue;
                }

                const std::string PhaseKey =
                    ExtractPhaseKeyFromCell(Row[PhaseCellIndex]);
                if (PhaseKey.empty())
                {
                    continue;
                }

                const std::string StatusRaw = Trim(Row[StatusCellIndex]);
                const std::string Status = NormalizeStatusValue(StatusRaw);
                if (Status != "in_progress" && Status != "blocked")
                {
                    continue;
                }

                ActivePhaseRecord Record;
                Record.mTopicKey = Plan.mTopicKey;
                Record.mPlanPath = Plan.mPath;
                Record.mPhaseKey = PhaseKey;
                Record.mStatusRaw = StatusRaw;
                Record.mStatus = Status;
                ActivePhases.push_back(std::move(Record));
            }
        }
    }
    return ActivePhases;
}

std::set<std::string>
BuildHeadingIdSet(const std::vector<HeadingRecord> &InHeadings)
{
    std::set<std::string> HeadingIds;
    for (const HeadingRecord &Heading : InHeadings)
    {
        HeadingIds.insert(Heading.mSectionID);
    }
    return HeadingIds;
}

bool IsPlaybookPhaseEntryReady(const fs::path &InPlaybookAbsolutePath,
                               std::vector<std::string> &OutMissingSections,
                               std::string &OutReadError)
{
    std::vector<std::string> Lines;
    if (!TryReadFileLines(InPlaybookAbsolutePath, Lines, OutReadError))
    {
        return false;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::set<std::string> HeadingIds = BuildHeadingIdSet(Headings);
    const std::vector<std::string> RequiredSections = {
        "phase_binding", "investigation_baseline", "phase_entry_readiness_gate",
        "execution_lanes"};

    for (const std::string &RequiredSection : RequiredSections)
    {
        if (HeadingIds.count(RequiredSection) == 0)
        {
            OutMissingSections.push_back(RequiredSection);
        }
    }
    return true;
}





void AppendGraphEdgeUnique(std::vector<GraphEdge> &InOutEdges,
                           std::set<std::string> &InOutEdgeKeys,
                           const std::string &InFromNodeId,
                           const std::string &InToNodeId,
                           const std::string &InKind, const int InDepth)
{
    const std::string Key = InFromNodeId + "|" + InToNodeId + "|" + InKind +
                            "|" + std::to_string(InDepth);
    if (InOutEdgeKeys.count(Key) > 0)
    {
        return;
    }
    InOutEdgeKeys.insert(Key);

    GraphEdge Edge;
    Edge.mFromNodeID = InFromNodeId;
    Edge.mToNodeID = InToNodeId;
    Edge.mKind = InKind;
    Edge.mDepth = InDepth;
    InOutEdges.push_back(std::move(Edge));
}

void AddDriftItem(std::vector<DriftItem> &InOutDrifts, const std::string &InID,
                  const std::string &InSeverity, const std::string &InTopicKey,
                  const std::string &InPath, const std::string &InMessage)
{
    DriftItem Item;
    Item.mID = InID;
    Item.mSeverity = InSeverity;
    Item.mTopicKey = InTopicKey;
    Item.mPath = InPath;
    Item.mMessage = InMessage;
    InOutDrifts.push_back(std::move(Item));
}

int SeverityRank(const std::string &InSeverity)
{
    if (InSeverity == "error")
    {
        return 0;
    }
    if (InSeverity == "warning")
    {
        return 1;
    }
    return 2;
}


} // namespace UniPlan
