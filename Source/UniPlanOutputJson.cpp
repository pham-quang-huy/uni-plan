#include "UniPlanTypes.h"
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


int RunListJson(const Inventory& InInventory, const std::string& InKind, const std::string& InStatusFilter)
{
    const bool bFilterActive = (InStatusFilter != "all");

    if (InKind == "pair")
    {
        const std::vector<const TopicPairRecord*> FilteredPairs = FilterPairsByStatus(InInventory.mPairs, InStatusFilter);

        PrintJsonHeader(kPairListSchema, InInventory.mGeneratedUtc, InInventory.mRepoRoot);
        if (bFilterActive)
        {
            EmitJsonField("status_filter", InStatusFilter);
        }
        EmitJsonFieldSizeT("count", FilteredPairs.size());
        std::cout << "\"pairs\":[";
        for (size_t Index = 0; Index < FilteredPairs.size(); ++Index)
        {
            const TopicPairRecord& Pair = *FilteredPairs[Index];
            PrintJsonSep(Index);
            std::cout << "{";
            EmitJsonField("topic_key", Pair.mTopicKey);
            EmitJsonFieldNullable("plan", Pair.mPlanPath);
            EmitJsonField("plan_status", GetDisplayStatus(Pair.mPlanStatus));
            EmitJsonFieldNullable("implementation", Pair.mImplementationPath);
            std::cout << "\"implementation_status\":" << JsonQuote(GetDisplayStatus(Pair.mImplementationStatus)) << ",";
            std::cout << "\"playbooks\":[";
            for (size_t PlaybookIndex = 0; PlaybookIndex < Pair.mPlaybooks.size(); ++PlaybookIndex)
            {
                const DocumentRecord& Playbook = Pair.mPlaybooks[PlaybookIndex];
                PrintJsonSep(PlaybookIndex);
                std::cout << "{";
                EmitJsonField("phase_key", Playbook.mPhaseKey);
                EmitJsonField("status", GetDisplayStatus(Playbook.mStatus));
                EmitJsonFieldNullable("status_raw", Playbook.mStatusRaw);
                EmitJsonField("path", Playbook.mPath, false);
                std::cout << "}";
            }
            std::cout << "],";
            EmitJsonField("pair_state", Pair.mPairState);
            EmitJsonField("overall_status", GetDisplayStatus(Pair.mOverallStatus), false);
            std::cout << "}";
        }
        std::cout << "],";
        PrintJsonClose(InInventory.mWarnings);
        return 0;
    }

    const std::vector<DocumentRecord>& Records = ResolveRecordsByKind(InInventory, InKind);
    const std::map<std::string, std::string> TopicPairStates = BuildTopicPairStateMap(InInventory);
    const std::set<std::string> PlanTopics = BuildTopicSet(InInventory.mPlans);
    const std::set<std::string> ImplementationTopics = BuildTopicSet(InInventory.mImplementations);

    const std::vector<const DocumentRecord*> FilteredRecords = FilterRecordsByStatus(Records, InStatusFilter);

    PrintJsonHeader(kListSchema, InInventory.mGeneratedUtc, InInventory.mRepoRoot);
    EmitJsonField("kind", InKind);
    if (bFilterActive)
    {
        EmitJsonField("status_filter", InStatusFilter);
    }
    EmitJsonFieldSizeT("count", FilteredRecords.size());
    std::cout << "\"items\":[";
    for (size_t Index = 0; Index < FilteredRecords.size(); ++Index)
    {
        const DocumentRecord& Record = *FilteredRecords[Index];
        PrintJsonSep(Index);
        std::string PairState = "unknown";
        if (InKind == "playbook")
        {
            PairState = DerivePlaybookPairState(Record, PlanTopics, ImplementationTopics);
        }
        else
        {
            const auto PairStateIt = TopicPairStates.find(Record.mTopicKey);
            if (PairStateIt != TopicPairStates.end())
            {
                PairState = PairStateIt->second;
            }
        }
        std::cout << "{";
        EmitJsonField("topic_key", Record.mTopicKey);
        if (InKind == "playbook")
        {
            EmitJsonField("phase_key", Record.mPhaseKey);
        }
        EmitJsonField("status", GetDisplayStatus(Record.mStatus));
        EmitJsonFieldNullable("status_raw", Record.mStatusRaw);
        EmitJsonField("pair_state", PairState);
        EmitJsonField("path", Record.mPath, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(InInventory.mWarnings);
    return 0;
}


int RunLintJson(const LintResult& InResult)
{
    PrintJsonHeader(kLintSchema, InResult.mGeneratedUtc, InResult.mRepoRoot);
    EmitJsonFieldBool("ok", InResult.mWarningCount == 0);
    EmitJsonFieldInt("warning_count", InResult.mWarningCount);
    PrintJsonWarnings(InResult.mWarnings);
    std::cout << ",\"checks\":[";
    std::cout << "{\"id\":\"name_pattern\",\"warning_count\":" << InResult.mNamePatternWarningCount << "},";
    std::cout << "{\"id\":\"missing_h1\",\"warning_count\":" << InResult.mMissingH1WarningCount << "}";
    std::cout << "]}\n";
    return 0;
}


int RunInventoryJson(const InventoryResult& InResult)
{
    PrintJsonHeader(kInventorySchema, InResult.mGeneratedUtc, InResult.mRepoRoot);
    EmitJsonFieldSizeT("count", InResult.mItems.size());
    std::cout << "\"items\":[";
    for (size_t ItemIndex = 0; ItemIndex < InResult.mItems.size(); ++ItemIndex)
    {
        PrintJsonSep(ItemIndex);
        const InventoryItem& Item = InResult.mItems[ItemIndex];
        std::cout << "{";
        EmitJsonField("path", Item.mPath);
        EmitJsonFieldInt("line_count", Item.mLineCount);
        EmitJsonFieldNullable("last_commit", Item.mLastCommit, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(InResult.mWarnings);
    return 0;
}


int RunOrphanCheckJson(const OrphanCheckResult& InResult)
{
    PrintJsonHeader(kOrphanCheckSchema, InResult.mGeneratedUtc, InResult.mRepoRoot);
    EmitJsonFieldSizeT("orphan_count", InResult.mOrphans.size());
    PrintJsonStringArray("orphans", InResult.mOrphans);
    std::cout << ",";
    PrintJsonStringArray("ignored_roots", InResult.mIgnoredRoots);
    std::cout << ",";
    PrintJsonClose(InResult.mWarnings);
    return 0;
}


int RunArtifactsJson(const Inventory& InInventory, const std::string& InTopicKey, const std::string& InKind)
{
    const bool IncludePlan = (InKind == "all" || InKind == "plan");
    const bool IncludeImplementation = (InKind == "all" || InKind == "implementation");
    const bool IncludePlaybook = (InKind == "all" || InKind == "playbook");
    const bool IncludeSidecar = (InKind == "all" || InKind == "sidecar");

    const DocumentRecord* Plan = FindSingleRecordByTopic(InInventory.mPlans, InTopicKey);
    const DocumentRecord* Implementation = FindSingleRecordByTopic(InInventory.mImplementations, InTopicKey);
    const std::vector<DocumentRecord> Playbooks = CollectRecordsByTopic(InInventory.mPlaybooks, InTopicKey);
    const std::vector<SidecarRecord> Sidecars = CollectSidecarsByTopic(InInventory.mSidecars, InTopicKey);

    const auto FindSidecarPath = [&Sidecars](const std::string& InOwnerKind, const std::string& InDocKind, const std::string& InPhaseKey) -> std::string {
        for (const SidecarRecord& Sidecar : Sidecars)
        {
            if (Sidecar.mOwnerKind == InOwnerKind && Sidecar.mDocKind == InDocKind && (InPhaseKey.empty() || Sidecar.mPhaseKey == InPhaseKey))
            {
                return Sidecar.mPath;
            }
        }
        return "";
    };

    PrintJsonHeader(kArtifactsSchema, InInventory.mGeneratedUtc, InInventory.mRepoRoot);
    EmitJsonField("topic_key", InTopicKey);
    std::cout << "\"bundle\":{";
    EmitJsonField("pair_state", ResolvePairStateForTopic(InInventory, InTopicKey));
    std::cout << "\"plan\":";
    if (IncludePlan && Plan != nullptr)
    {
        std::cout << "{";
        EmitJsonField("path", Plan->mPath);
        EmitJsonField("status", GetDisplayStatus(Plan->mStatus));
        EmitJsonFieldNullable("status_raw", Plan->mStatusRaw, false);
        std::cout << "}";
    }
    else
    {
        std::cout << "null";
    }

    std::cout << ",";
    std::cout << "\"implementation\":";
    if (IncludeImplementation && Implementation != nullptr)
    {
        std::cout << "{";
        EmitJsonField("path", Implementation->mPath);
        EmitJsonField("status", GetDisplayStatus(Implementation->mStatus));
        EmitJsonFieldNullable("status_raw", Implementation->mStatusRaw, false);
        std::cout << "}";
    }
    else
    {
        std::cout << "null";
    }

    std::cout << ",";
    std::cout << "\"playbooks\":[";
    if (IncludePlaybook)
    {
        for (size_t Index = 0; Index < Playbooks.size(); ++Index)
        {
            PrintJsonSep(Index);
            const DocumentRecord& Playbook = Playbooks[Index];
            std::cout << "{";
            EmitJsonField("phase_key", Playbook.mPhaseKey);
            EmitJsonField("path", Playbook.mPath);
            EmitJsonField("status", GetDisplayStatus(Playbook.mStatus));
            EmitJsonFieldNullable("status_raw", Playbook.mStatusRaw, false);
            std::cout << "}";
        }
    }
    std::cout << "],";

    std::cout << "\"sidecars\":";
    if (!IncludeSidecar)
    {
        std::cout << "null";
    }
    else
    {
        std::cout << "{";
        std::cout << "\"plan\":{";
        EmitJsonFieldNullable("changelog", FindSidecarPath("Plan", kSidecarChangeLog, ""));
        EmitJsonFieldNullable("verification", FindSidecarPath("Plan", kSidecarVerification, ""), false);
        std::cout << "},";
        std::cout << "\"implementation\":{";
        EmitJsonFieldNullable("changelog", FindSidecarPath("Impl", kSidecarChangeLog, ""));
        EmitJsonFieldNullable("verification", FindSidecarPath("Impl", kSidecarVerification, ""), false);
        std::cout << "},";
        std::cout << "\"playbooks\":[";
        bool FirstSidecarPlaybook = true;
        for (const DocumentRecord& Playbook : Playbooks)
        {
            const std::string PlaybookChangeLog = FindSidecarPath("Playbook", kSidecarChangeLog, Playbook.mPhaseKey);
            const std::string PlaybookVerification = FindSidecarPath("Playbook", kSidecarVerification, Playbook.mPhaseKey);
            if (PlaybookChangeLog.empty() && PlaybookVerification.empty())
            {
                continue;
            }
            if (!FirstSidecarPlaybook)
            {
                std::cout << ",";
            }
            FirstSidecarPlaybook = false;
            std::cout << "{";
            EmitJsonField("phase_key", Playbook.mPhaseKey);
            EmitJsonFieldNullable("changelog", PlaybookChangeLog);
            EmitJsonFieldNullable("verification", PlaybookVerification, false);
            std::cout << "}";
        }
        std::cout << "]";
        std::cout << "}";
    }

    std::cout << "},";
    PrintJsonClose(InInventory.mWarnings);
    return 0;
}


int RunEvidenceJson(
    const char* InSchemaId,
    const std::string& InGeneratedUtc,
    const std::string& InRepoRoot,
    const std::string& InTopicKey,
    const std::string& InDocClass,
    const std::vector<EvidenceEntry>& InEntries,
    const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(InSchemaId, InGeneratedUtc, InRepoRoot);
    EmitJsonField("topic_key", InTopicKey);
    EmitJsonField("doc_class", InDocClass);
    std::cout << "\"entries\":[";
    for (size_t EntryIndex = 0; EntryIndex < InEntries.size(); ++EntryIndex)
    {
        PrintJsonSep(EntryIndex);
        const EvidenceEntry& Entry = InEntries[EntryIndex];
        std::cout << "{";
        EmitJsonField("source_path", Entry.mSourcePath);
        EmitJsonFieldNullable("phase_key", Entry.mPhaseKey);
        EmitJsonFieldInt("table_id", Entry.mTableId);
        EmitJsonFieldInt("row_index", Entry.mRowIndex);
        std::cout << "\"fields\":{";
        for (size_t FieldIndex = 0; FieldIndex < Entry.mFields.size(); ++FieldIndex)
        {
            PrintJsonSep(FieldIndex);
            std::cout << JsonQuote(Entry.mFields[FieldIndex].first) << ":" << JsonQuote(Entry.mFields[FieldIndex].second);
        }
        std::cout << "}}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunSchemeJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::string& InType, const std::vector<SchemaField>& InFields, const std::vector<std::string>& InExamples, const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kSchemeSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("type", InType);
    std::cout << "\"fields\":[";
    for (size_t FieldIndex = 0; FieldIndex < InFields.size(); ++FieldIndex)
    {
        PrintJsonSep(FieldIndex);
        const SchemaField& Field = InFields[FieldIndex];
        std::cout << "{";
        EmitJsonFieldNullable("section_id", Field.mSectionId);
        EmitJsonField("name", Field.mProperty);
        EmitJsonField("value", Field.mValue, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonStringArray("examples", InExamples);
    std::cout << ",";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunRulesJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::vector<RuleEntry>& InRules, const std::vector<std::string>& InWarnings)
{
    std::set<std::string> Sources;
    int ResolvedCount = 0;
    for (const RuleEntry& Rule : InRules)
    {
        Sources.insert(Rule.mSource);
        if (Rule.mbSourceResolved)
        {
            ResolvedCount += 1;
        }
    }

    PrintJsonHeader(kRulesSchema, InGeneratedUtc, InRepoRoot);
    std::cout << "\"rules\":[";
    for (size_t RuleIndex = 0; RuleIndex < InRules.size(); ++RuleIndex)
    {
        PrintJsonSep(RuleIndex);
        const RuleEntry& Rule = InRules[RuleIndex];
        std::cout << "{";
        EmitJsonField("id", Rule.mId);
        EmitJsonField("description", Rule.mDescription);
        EmitJsonField("source", Rule.mSource);
        EmitJsonFieldNullable("source_path", Rule.mSourcePath);
        EmitJsonFieldNullable("source_section_id", Rule.mSourceSectionId);
        std::cout << "\"source_table_id\":";
        if (Rule.mSourceTableId > 0)
        {
            std::cout << Rule.mSourceTableId;
        }
        else
        {
            std::cout << "null";
        }
        std::cout << ",";
        std::cout << "\"source_row_index\":";
        if (Rule.mSourceRowIndex > 0)
        {
            std::cout << Rule.mSourceRowIndex;
        }
        else
        {
            std::cout << "null";
        }
        std::cout << ",";
        EmitJsonFieldNullable("source_evidence", Rule.mSourceEvidence);
        EmitJsonFieldBool("provenance_resolved", Rule.mbSourceResolved, false);
        std::cout << "}";
    }
    std::cout << "],";
    std::cout << "\"sources\":[";
    size_t SourceIndex = 0;
    for (const std::string& Source : Sources)
    {
        PrintJsonSep(SourceIndex);
        std::cout << JsonQuote(Source);
        SourceIndex += 1;
    }
    std::cout << "],";
    EmitJsonFieldInt("provenance_resolved_count", ResolvedCount);
    std::cout << "\"provenance_unresolved_count\":" << (static_cast<int>(InRules.size()) - ResolvedCount) << ",";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunValidateJson(
    const std::string& InGeneratedUtc,
    const std::string& InRepoRoot,
    const bool InStrict,
    const bool InOk,
    const std::vector<ValidateCheck>& InChecks,
    const std::vector<std::string>& InErrors,
    const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kValidateSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonFieldBool("strict", InStrict);
    EmitJsonFieldBool("ok", InOk);
    std::cout << "\"checks\":[";
    for (size_t CheckIndex = 0; CheckIndex < InChecks.size(); ++CheckIndex)
    {
        PrintJsonSep(CheckIndex);
        const ValidateCheck& Check = InChecks[CheckIndex];
        std::cout << "{";
        EmitJsonField("id", Check.mId);
        EmitJsonFieldBool("ok", Check.mbOk);
        EmitJsonFieldBool("critical", Check.mbCritical);
        EmitJsonFieldNullable("rule_id", Check.mRuleId);
        EmitJsonField("detail", Check.mDetail);
        PrintJsonStringArray("diagnostics", Check.mDiagnostics);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonStringArray("errors", InErrors);
    std::cout << ",";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunSectionResolveJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::string& InDocPath, const SectionResolution& InResolution, const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kSectionResolveSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("doc", InDocPath);
    EmitJsonField("section_query", InResolution.mSectionQuery);
    EmitJsonFieldBool("matched", InResolution.mbFound);
    std::cout << "\"section\":";
    if (InResolution.mbFound)
    {
        std::cout << "{";
        EmitJsonField("section_id", InResolution.mSectionId);
        EmitJsonField("heading", InResolution.mSectionHeading);
        EmitJsonFieldInt("level", InResolution.mLevel);
        EmitJsonFieldInt("start_line", InResolution.mStartLine);
        EmitJsonFieldInt("end_line", InResolution.mEndLine, false);
        std::cout << "}";
    }
    else
    {
        std::cout << "null";
    }
    std::cout << ",";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunSectionSchemaJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::string& InType, const std::vector<SectionSchemaEntry>& InEntries)
{
    PrintJsonHeader(kSectionSchemaSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("type", InType);
    EmitJsonFieldSizeT("count", InEntries.size());
    std::cout << "\"sections\":[";
    for (size_t Index = 0; Index < InEntries.size(); ++Index)
    {
        PrintJsonSep(Index);
        const SectionSchemaEntry& Entry = InEntries[Index];
        std::cout << "{\"section_id\":" << JsonQuote(Entry.mSectionId) << ",\"required\":" << (Entry.mbRequired ? "true" : "false") << ",\"order\":" << Entry.mOrder << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}


// ---------------------------------------------------------------------------
// Section List renderers (inventory mode — literal headings + normalized IDs)
// ---------------------------------------------------------------------------

// SectionCount moved to DocTypes.h

int RunSectionListJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::vector<SectionCount>& InCounts, const bool InShowCount)
{
    std::set<std::string> UniqueSectionIds;
    for (const SectionCount& Item : InCounts)
    {
        UniqueSectionIds.insert(Item.mSectionId);
    }

    PrintJsonHeader(kSectionListSchema, InGeneratedUtc, InRepoRoot);
    std::cout << "\"mode\":\"inventory\",";
    EmitJsonFieldSizeT("unique_headings", InCounts.size());
    EmitJsonFieldSizeT("unique_sections", UniqueSectionIds.size());
    std::cout << "\"sections\":[";
    for (size_t Index = 0; Index < InCounts.size(); ++Index)
    {
        PrintJsonSep(Index);
        const SectionCount& Item = InCounts[Index];
        std::cout << "{\"heading\":" << JsonQuote(Item.mHeading) << ",\"section_id\":" << JsonQuote(Item.mSectionId);
        if (InShowCount)
        {
            std::cout << ",\"count\":" << Item.mCount;
        }
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}


// ---------------------------------------------------------------------------
// Section List renderers (single-doc mode — heading tree)
// ---------------------------------------------------------------------------

int RunSectionListDocJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::string& InDocPath, const std::vector<HeadingRecord>& InHeadings)
{
    PrintJsonHeader(kSectionListSchema, InGeneratedUtc, InRepoRoot);
    std::cout << "\"mode\":\"document\",";
    EmitJsonField("doc", InDocPath);
    EmitJsonFieldSizeT("count", InHeadings.size());
    std::cout << "\"sections\":[";
    for (size_t Index = 0; Index < InHeadings.size(); ++Index)
    {
        PrintJsonSep(Index);
        const HeadingRecord& Heading = InHeadings[Index];
        std::cout << "{\"section_id\":" << JsonQuote(Heading.mSectionId) << ",\"heading\":" << JsonQuote(Heading.mText) << ",\"level\":" << Heading.mLevel << ",\"line\":" << Heading.mLine << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}


int RunExcerptJson(
    const std::string& InGeneratedUtc,
    const std::string& InRepoRoot,
    const std::string& InDocPath,
    const SectionResolution& InResolution,
    const int InContextLines,
    const int InExcerptStartLine,
    const std::vector<std::string>& InExcerptLines,
    const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kExcerptSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("doc", InDocPath);
    EmitJsonFieldInt("context_lines", InContextLines);
    std::cout << "\"section\":{";
    EmitJsonField("section_id", InResolution.mSectionId);
    EmitJsonField("heading", InResolution.mSectionHeading);
    EmitJsonFieldInt("start_line", InResolution.mStartLine);
    EmitJsonFieldInt("end_line", InResolution.mEndLine, false);
    std::cout << "},";
    std::cout << "\"excerpt\":[";
    for (size_t LineIndex = 0; LineIndex < InExcerptLines.size(); ++LineIndex)
    {
        PrintJsonSep(LineIndex);
        const int LineNumber = InExcerptStartLine + static_cast<int>(LineIndex);
        std::cout << "{";
        EmitJsonFieldInt("line", LineNumber);
        EmitJsonField("text", InExcerptLines[LineIndex], false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunTableListJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::string& InDocPath, const std::vector<MarkdownTableRecord>& InTables, const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kTableListSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("doc", InDocPath);
    EmitJsonFieldSizeT("count", InTables.size());
    std::cout << "\"tables\":[";
    for (size_t TableIndex = 0; TableIndex < InTables.size(); ++TableIndex)
    {
        PrintJsonSep(TableIndex);
        const MarkdownTableRecord& Table = InTables[TableIndex];
        std::cout << "{";
        EmitJsonFieldInt("table_id", Table.mTableId);
        EmitJsonFieldNullable("section_id", Table.mSectionId);
        EmitJsonFieldNullable("section_heading", Table.mSectionHeading);
        EmitJsonFieldInt("start_line", Table.mStartLine);
        EmitJsonFieldInt("end_line", Table.mEndLine);
        EmitJsonFieldSizeT("column_count", Table.mHeaders.size());
        EmitJsonFieldSizeT("row_count", Table.mRows.size());
        PrintJsonStringArray("headers", Table.mHeaders);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunTableGetJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::string& InDocPath, const MarkdownTableRecord& InTable, const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kTableGetSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("doc", InDocPath);
    std::cout << "\"table\":{";
    EmitJsonFieldInt("table_id", InTable.mTableId);
    EmitJsonFieldNullable("section_id", InTable.mSectionId);
    EmitJsonFieldNullable("section_heading", InTable.mSectionHeading);
    EmitJsonFieldInt("start_line", InTable.mStartLine);
    EmitJsonFieldInt("end_line", InTable.mEndLine);
    PrintJsonStringArray("headers", InTable.mHeaders);
    std::cout << ",\"rows\":[";
    for (size_t RowIndex = 0; RowIndex < InTable.mRows.size(); ++RowIndex)
    {
        PrintJsonSep(RowIndex);
        std::cout << "[";
        for (size_t CellIndex = 0; CellIndex < InTable.mRows[RowIndex].size(); ++CellIndex)
        {
            PrintJsonSep(CellIndex);
            std::cout << JsonQuote(InTable.mRows[RowIndex][CellIndex]);
        }
        std::cout << "]";
    }
    std::cout << "],\"row_objects\":[";
    for (size_t RowIndex = 0; RowIndex < InTable.mRows.size(); ++RowIndex)
    {
        PrintJsonSep(RowIndex);
        std::cout << "{";
        for (size_t HeaderIndex = 0; HeaderIndex < InTable.mHeaders.size(); ++HeaderIndex)
        {
            PrintJsonSep(HeaderIndex);
            const std::string Value = (HeaderIndex < InTable.mRows[RowIndex].size()) ? InTable.mRows[RowIndex][HeaderIndex] : "";
            std::cout << JsonQuote(InTable.mHeaders[HeaderIndex]) << ":" << JsonQuote(Value);
        }
        std::cout << "}";
    }
    std::cout << "]},";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunGraphJson(
    const std::string& InGeneratedUtc,
    const std::string& InRepoRoot,
    const std::string& InTopicKey,
    const int InDepth,
    const std::vector<GraphNode>& InNodes,
    const std::vector<GraphEdge>& InEdges,
    const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kGraphSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("topic_key", InTopicKey);
    EmitJsonFieldInt("depth", InDepth);
    EmitJsonFieldSizeT("node_count", InNodes.size());
    EmitJsonFieldSizeT("edge_count", InEdges.size());
    std::cout << "\"nodes\":[";
    for (size_t Index = 0; Index < InNodes.size(); ++Index)
    {
        PrintJsonSep(Index);
        const GraphNode& Node = InNodes[Index];
        std::cout << "{";
        EmitJsonField("id", Node.mId);
        EmitJsonField("type", Node.mType);
        EmitJsonField("path", Node.mPath);
        EmitJsonFieldNullable("topic_key", Node.mTopicKey);
        EmitJsonFieldNullable("phase_key", Node.mPhaseKey);
        EmitJsonFieldNullable("owner_kind", Node.mOwnerKind);
        EmitJsonFieldNullable("doc_kind", Node.mDocKind, false);
        std::cout << "}";
    }
    std::cout << "],\"edges\":[";
    for (size_t Index = 0; Index < InEdges.size(); ++Index)
    {
        PrintJsonSep(Index);
        const GraphEdge& Edge = InEdges[Index];
        std::cout << "{";
        EmitJsonField("from", Edge.mFromNodeId);
        EmitJsonField("to", Edge.mToNodeId);
        EmitJsonField("kind", Edge.mKind);
        EmitJsonFieldInt("depth", Edge.mDepth, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunDiagnoseDriftJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const bool InOk, const std::vector<DriftItem>& InDrifts, const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kDriftDiagnoseSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonFieldBool("ok", InOk);
    EmitJsonFieldSizeT("drift_count", InDrifts.size());
    std::cout << "\"drifts\":[";
    for (size_t Index = 0; Index < InDrifts.size(); ++Index)
    {
        PrintJsonSep(Index);
        const DriftItem& Drift = InDrifts[Index];
        std::cout << "{";
        EmitJsonField("id", Drift.mId);
        EmitJsonField("severity", Drift.mSeverity);
        EmitJsonFieldNullable("topic_key", Drift.mTopicKey);
        EmitJsonFieldNullable("path", Drift.mPath);
        EmitJsonField("message", Drift.mMessage, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunTimelineJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::string& InTopicKey, const std::string& InSince, const std::vector<TimelineItem>& InItems, const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kTimelineSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("topic_key", InTopicKey);
    EmitJsonFieldNullable("since", InSince);
    EmitJsonFieldSizeT("count", InItems.size());
    std::cout << "\"entries\":[";
    for (size_t Index = 0; Index < InItems.size(); ++Index)
    {
        PrintJsonSep(Index);
        const TimelineItem& Item = InItems[Index];
        std::cout << "{";
        EmitJsonFieldNullable("date", Item.mDate);
        EmitJsonField("doc_class", Item.mDocClass);
        EmitJsonFieldNullable("phase_key", Item.mPhaseKey);
        EmitJsonField("source_path", Item.mSourcePath);
        EmitJsonField("update", Item.mUpdate);
        EmitJsonField("evidence", Item.mEvidence, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunPhaseJson(
    const std::string& InGeneratedUtc,
    const std::string& InRepoRoot,
    const std::string& InTopicKey,
    const std::string& InPlanPath,
    const std::string& InStatusFilter,
    const std::vector<PhaseItem>& InItems,
    const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kPhaseListSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("topic_key", InTopicKey);
    EmitJsonField("plan_path", InPlanPath);
    EmitJsonField("status", InStatusFilter);
    EmitJsonFieldSizeT("count", InItems.size());
    std::cout << "\"phases\":[";
    for (size_t Index = 0; Index < InItems.size(); ++Index)
    {
        PrintJsonSep(Index);
        const PhaseItem& Item = InItems[Index];
        std::cout << "{";
        EmitJsonField("phase_key", Item.mPhaseKey);
        EmitJsonField("status", Item.mStatus);
        EmitJsonFieldNullable("status_raw", Item.mStatusRaw);
        EmitJsonFieldNullable("playbook", Item.mPlaybookPath);
        EmitJsonFieldNullable("description", Item.mDescription);
        EmitJsonFieldNullable("next_action", Item.mNextAction);
        EmitJsonFieldInt("table_id", Item.mTableId);
        EmitJsonFieldInt("row_index", Item.mRowIndex);
        std::cout << "\"fields\":{";
        for (size_t FieldIndex = 0; FieldIndex < Item.mFields.size(); ++FieldIndex)
        {
            PrintJsonSep(FieldIndex);
            std::cout << JsonQuote(Item.mFields[FieldIndex].first) << ":" << JsonQuote(Item.mFields[FieldIndex].second);
        }
        std::cout << "}}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}


int RunBlockersJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::string& InStatusFilter, const std::vector<BlockerItem>& InItems, const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kBlockersSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("status", InStatusFilter);
    EmitJsonFieldSizeT("count", InItems.size());
    std::cout << "\"items\":[";
    for (size_t Index = 0; Index < InItems.size(); ++Index)
    {
        PrintJsonSep(Index);
        const BlockerItem& Item = InItems[Index];
        std::cout << "{";
        EmitJsonField("topic_key", Item.mTopicKey);
        EmitJsonField("source_path", Item.mSourcePath);
        EmitJsonField("kind", Item.mKind);
        EmitJsonField("status", Item.mStatus);
        EmitJsonFieldNullable("phase_key", Item.mPhaseKey);
        EmitJsonFieldNullable("priority", Item.mPriority);
        EmitJsonField("action", Item.mAction);
        EmitJsonFieldNullable("owner", Item.mOwner);
        EmitJsonFieldNullable("notes", Item.mNotes, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}


// ---------------------------------------------------------------------------
// Cache info output (JSON / text)
// ---------------------------------------------------------------------------

int RunCacheInfoJson(const std::string& InRepoRoot, const CacheInfoResult& InResult)
{
    PrintJsonHeader(kCacheInfoSchema, InResult.mGeneratedUtc, InRepoRoot);
    EmitJsonField("cache_dir", InResult.mCacheDir);
    EmitJsonFieldNullable("config_cache_dir", InResult.mConfigCacheDir);
    EmitJsonField("ini_path", InResult.mIniPath);
    EmitJsonFieldBool("cache_enabled", InResult.mbCacheEnabled);
    EmitJsonFieldBool("cache_verbose", InResult.mbCacheVerbose);
    EmitJsonFieldBool("cache_exists", InResult.mbCacheExists);
    std::cout << "\"cache_size_bytes\":" << InResult.mCacheSizeBytes << ",";
    EmitJsonFieldInt("cache_entry_count", InResult.mCacheEntryCount);
    EmitJsonField("current_repo_cache_path", InResult.mCurrentRepoCachePath);
    EmitJsonFieldBool("current_repo_cache_exists", InResult.mbCurrentRepoCacheExists);
    PrintJsonClose(InResult.mWarnings);
    return 0;
}


// ---------------------------------------------------------------------------
// Cache clear output (JSON / text)
// ---------------------------------------------------------------------------

int RunCacheClearJson(const std::string& InRepoRoot, const CacheClearResult& InResult)
{
    PrintJsonHeader(kCacheClearSchema, InResult.mGeneratedUtc, InRepoRoot);
    EmitJsonField("cache_dir", InResult.mCacheDir);
    EmitJsonFieldInt("entries_removed", InResult.mEntriesRemoved);
    std::cout << "\"bytes_freed\":" << InResult.mBytesFreed << ",";
    EmitJsonFieldBool("success", InResult.mbSuccess);
    EmitJsonFieldNullable("error", InResult.mError);
    PrintJsonClose(InResult.mWarnings);
    return 0;
}


// ---------------------------------------------------------------------------
// Cache config output (JSON / text)
// ---------------------------------------------------------------------------

int RunCacheConfigJson(const std::string& InRepoRoot, const CacheConfigResult& InResult)
{
    PrintJsonHeader(kCacheConfigSchema, InResult.mGeneratedUtc, InRepoRoot);
    EmitJsonField("ini_path", InResult.mIniPath);
    EmitJsonFieldBool("success", InResult.mbSuccess);
    EmitJsonFieldNullable("error", InResult.mError);
    std::cout << "\"effective_config\":{";
    EmitJsonFieldNullable("dir", InResult.mDir);
    EmitJsonFieldBool("enabled", InResult.mbEnabled);
    EmitJsonFieldBool("verbose", InResult.mbVerbose, false);
    std::cout << "},";
    PrintJsonClose(InResult.mWarnings);
    return 0;
}


// ---------------------------------------------------------------------------
// Phase list all output (JSON / text)
// ---------------------------------------------------------------------------

int RunPhaseListAllJson(const std::string& InGeneratedUtc, const std::string& InRepoRoot, const std::string& InStatusFilter, const std::vector<PhaseListAllEntry>& InEntries, const std::vector<std::string>& InWarnings)
{
    size_t TotalPhases = 0;
    for (const PhaseListAllEntry& Entry : InEntries)
    {
        TotalPhases += Entry.mPhases.size();
    }

    PrintJsonHeader(kPhaseListSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("mode", "all");
    EmitJsonField("status_filter", InStatusFilter);
    EmitJsonFieldSizeT("plan_count", InEntries.size());
    EmitJsonFieldSizeT("phase_count", TotalPhases);
    std::cout << "\"plans\":[";
    for (size_t PlanIndex = 0; PlanIndex < InEntries.size(); ++PlanIndex)
    {
        PrintJsonSep(PlanIndex);
        const PhaseListAllEntry& Entry = InEntries[PlanIndex];
        std::cout << "{";
        EmitJsonField("topic_key", Entry.mTopicKey);
        EmitJsonField("plan_path", Entry.mPlanPath);
        EmitJsonField("plan_status", Entry.mPlanStatus);
        EmitJsonFieldSizeT("phase_count", Entry.mPhases.size());
        std::cout << "\"phases\":[";
        for (size_t PhaseIndex = 0; PhaseIndex < Entry.mPhases.size(); ++PhaseIndex)
        {
            PrintJsonSep(PhaseIndex);
            const PhaseItem& Item = Entry.mPhases[PhaseIndex];
            std::cout << "{";
            EmitJsonField("phase_key", Item.mPhaseKey);
            EmitJsonField("status", Item.mStatus);
            EmitJsonFieldNullable("status_raw", Item.mStatusRaw);
            EmitJsonFieldNullable("playbook", Item.mPlaybookPath);
            EmitJsonFieldNullable("description", Item.mDescription);
            EmitJsonFieldNullable("next_action", Item.mNextAction, false);
            std::cout << "}";
        }
        std::cout << "]}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}


// ---------------------------------------------------------------------------
// Section Content output functions
// ---------------------------------------------------------------------------

int RunSectionContentJson(
    const std::string& InGeneratedUtc,
    const std::string& InRepoRoot,
    const std::string& InDocPath,
    const SectionResolution& InResolution,
    const int InLineCharLimit,
    const int InContentStartLine,
    const std::vector<std::string>& InContentLines,
    const std::vector<MarkdownTableRecord>& InTables,
    const std::vector<std::string>& InWarnings)
{
    PrintJsonHeader(kSectionContentSchema, InGeneratedUtc, InRepoRoot);
    EmitJsonField("doc", InDocPath);
    std::cout << "\"section\":{";
    EmitJsonField("section_id", InResolution.mSectionId);
    EmitJsonField("heading", InResolution.mSectionHeading);
    EmitJsonFieldInt("start_line", InResolution.mStartLine);
    EmitJsonFieldInt("end_line", InResolution.mEndLine, false);
    std::cout << "},";
    EmitJsonFieldInt("line_char_limit", InLineCharLimit);
    std::cout << "\"lines\":[";
    for (size_t LineIndex = 0; LineIndex < InContentLines.size(); ++LineIndex)
    {
        PrintJsonSep(LineIndex);
        const int LineNumber = InContentStartLine + static_cast<int>(LineIndex);
        std::string Text = InContentLines[LineIndex];
        if (InLineCharLimit > 0)
        {
            Text = TruncateForDisplay(Text, static_cast<size_t>(InLineCharLimit));
        }
        std::cout << "{";
        EmitJsonFieldInt("line", LineNumber);
        EmitJsonField("text", Text, false);
        std::cout << "}";
    }
    std::cout << "],\"tables\":[";
    for (size_t TableIndex = 0; TableIndex < InTables.size(); ++TableIndex)
    {
        PrintJsonSep(TableIndex);
        const MarkdownTableRecord& Table = InTables[TableIndex];
        std::cout << "{";
        EmitJsonFieldInt("table_id", Table.mTableId);
        EmitJsonFieldNullable("section_id", Table.mSectionId);
        EmitJsonFieldInt("start_line", Table.mStartLine);
        EmitJsonFieldInt("end_line", Table.mEndLine);
        PrintJsonStringArray("headers", Table.mHeaders);
        std::cout << ",\"rows\":[";
        for (size_t RowIndex = 0; RowIndex < Table.mRows.size(); ++RowIndex)
        {
            PrintJsonSep(RowIndex);
            std::cout << "[";
            for (size_t CellIndex = 0; CellIndex < Table.mRows[RowIndex].size(); ++CellIndex)
            {
                PrintJsonSep(CellIndex);
                std::string Cell = Table.mRows[RowIndex][CellIndex];
                if (InLineCharLimit > 0)
                {
                    Cell = TruncateForDisplay(Cell, static_cast<size_t>(InLineCharLimit));
                }
                std::cout << JsonQuote(Cell);
            }
            std::cout << "]";
        }
        std::cout << "]}";
    }
    std::cout << "],";
    PrintJsonClose(InWarnings);
    return 0;
}

} // namespace UniPlan
