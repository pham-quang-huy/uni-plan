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









// ---------------------------------------------------------------------------
// Human-mode helpers (ANSI color + formatted table rendering)
// ---------------------------------------------------------------------------






struct HumanTable
{
    std::vector<std::string> mHeaders;
    std::vector<std::vector<std::string>> mRows;

    void AddRow(const std::vector<std::string>& InRow)
    {
        mRows.push_back(InRow);
    }

    static size_t VisibleWidth(const std::string& InText)
    {
        size_t Width = 0;
        bool InEscape = false;
        for (const char Char : InText)
        {
            if (Char == '\033')
            {
                InEscape = true;
                continue;
            }
            if (InEscape)
            {
                if (Char == 'm')
                {
                    InEscape = false;
                }
                continue;
            }
            Width += 1;
        }
        return Width;
    }

    void Print() const
    {
        if (mHeaders.empty())
        {
            return;
        }

        const size_t ColumnCount = mHeaders.size();
        std::vector<size_t> Widths(ColumnCount, 0);
        for (size_t Col = 0; Col < ColumnCount; ++Col)
        {
            Widths[Col] = VisibleWidth(mHeaders[Col]);
        }
        for (const std::vector<std::string>& Row : mRows)
        {
            for (size_t Col = 0; Col < Row.size() && Col < ColumnCount; ++Col)
            {
                Widths[Col] = (std::max)(Widths[Col], VisibleWidth(Row[Col]));
            }
        }

        PrintRow(mHeaders, Widths, true);
        PrintSeparator(Widths);
        for (const std::vector<std::string>& Row : mRows)
        {
            PrintRow(Row, Widths, false);
        }
    }

private:
    void PrintRow(const std::vector<std::string>& InCells, const std::vector<size_t>& InWidths, const bool InBold) const
    {
        for (size_t Col = 0; Col < InWidths.size(); ++Col)
        {
            if (Col > 0)
            {
                std::cout << "  ";
            }
            const std::string Cell = (Col < InCells.size()) ? InCells[Col] : "";
            const size_t Visible = VisibleWidth(Cell);
            if (InBold)
            {
                std::cout << kColorBold << Cell << kColorReset;
            }
            else
            {
                std::cout << Cell;
            }
            if (Visible < InWidths[Col])
            {
                std::cout << std::string(InWidths[Col] - Visible, ' ');
            }
        }
        std::cout << "\n";
    }

    void PrintSeparator(const std::vector<size_t>& InWidths) const
    {
        for (size_t Col = 0; Col < InWidths.size(); ++Col)
        {
            if (Col > 0)
            {
                std::cout << "  ";
            }
            std::cout << std::string(InWidths[Col], '-');
        }
        std::cout << "\n";
    }
};


// ---------------------------------------------------------------------------
// Human-mode output functions (--human flag)
// ---------------------------------------------------------------------------

int RunListHuman(const Inventory& InInventory, const std::string& InKind, const std::string& InStatusFilter)
{
    const bool bFilterActive = (InStatusFilter != "all");

    if (InKind == "pair")
    {
        const std::vector<const TopicPairRecord*> FilteredPairs = FilterPairsByStatus(InInventory.mPairs, InStatusFilter);

        std::cout << ColorBold("List: pairs");
        if (bFilterActive)
        {
            std::cout << " [" << Colorize(kColorYellow, "status=" + InStatusFilter) << "]";
        }
        std::cout << " (" << Colorize(kColorOrange, std::to_string(FilteredPairs.size())) << " topics)\n\n";
        HumanTable Table;
        Table.mHeaders = {"TopicKey", "PairState", "OverallStatus", "PlanStatus", "ImplStatus", "Playbooks"};
        for (const TopicPairRecord* PairPtr : FilteredPairs)
        {
            const TopicPairRecord& Pair = *PairPtr;
            std::ostringstream PlaybookSummary;
            for (size_t Index = 0; Index < Pair.mPlaybooks.size(); ++Index)
            {
                if (Index > 0)
                {
                    PlaybookSummary << ",";
                }
                PlaybookSummary << Pair.mPlaybooks[Index].mPhaseKey;
            }
            Table.AddRow(
                {Colorize(kColorOrange, Pair.mTopicKey),
                 ColorizeStatus(Pair.mPairState),
                 ColorizeStatus(GetDisplayStatus(Pair.mOverallStatus)),
                 ColorizeStatus(GetDisplayStatus(Pair.mPlanStatus)),
                 ColorizeStatus(GetDisplayStatus(Pair.mImplementationStatus)),
                 PlaybookSummary.str().empty() ? "-" : PlaybookSummary.str()});
        }
        if (bFilterActive && FilteredPairs.empty())
        {
            std::cout << Colorize(kColorYellow, "No pairs found with status '" + InStatusFilter + "'.") << "\n";
        }
        else
        {
            Table.Print();
        }
        return 0;
    }

    const std::vector<DocumentRecord>& Records = ResolveRecordsByKind(InInventory, InKind);
    const std::map<std::string, std::string> TopicPairStates = BuildTopicPairStateMap(InInventory);
    const std::set<std::string> PlanTopics = BuildTopicSet(InInventory.mPlans);
    const std::set<std::string> ImplementationTopics = BuildTopicSet(InInventory.mImplementations);

    const std::vector<const DocumentRecord*> FilteredRecords = FilterRecordsByStatus(Records, InStatusFilter);

    std::cout << ColorBold("List: " + InKind);
    if (bFilterActive)
    {
        std::cout << " [" << Colorize(kColorYellow, "status=" + InStatusFilter) << "]";
    }
    std::cout << " (" << Colorize(kColorOrange, std::to_string(FilteredRecords.size())) << " docs)\n\n";
    if (bFilterActive && FilteredRecords.empty())
    {
        std::cout << Colorize(kColorYellow, "No " + InKind + " found with status '" + InStatusFilter + "'.") << "\n";
        return 0;
    }
    HumanTable Table;
    if (InKind == "playbook")
    {
        Table.mHeaders = {"TopicKey", "Phase", "Status", "PairState", "Path"};
    }
    else
    {
        Table.mHeaders = {"TopicKey", "Status", "PairState", "Path"};
    }
    for (const DocumentRecord* RecordPtr : FilteredRecords)
    {
        const DocumentRecord& Record = *RecordPtr;
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
        const std::string StatusValue = GetDisplayStatus(Record.mStatus);
        if (InKind == "playbook")
        {
            Table.AddRow({Colorize(kColorOrange, Record.mTopicKey), Record.mPhaseKey, ColorizeStatus(StatusValue), ColorizeStatus(PairState), Colorize(kColorDim, Record.mPath)});
        }
        else
        {
            Table.AddRow({Colorize(kColorOrange, Record.mTopicKey), ColorizeStatus(StatusValue), ColorizeStatus(PairState), Colorize(kColorDim, Record.mPath)});
        }
    }
    Table.Print();
    return 0;
}


int RunLintHuman(const LintResult& InResult)
{
    for (const std::string& Warning : InResult.mWarnings)
    {
        std::cout << Colorize(kColorYellow, "WARN") << " " << Warning << "\n";
    }
    std::cout << "\n" << ColorBold("Lint warnings: ") << Colorize(kColorOrange, std::to_string(InResult.mWarningCount)) << "\n";
    return 0;
}


int RunInventoryHuman(const InventoryResult& InResult)
{
    std::cout << ColorBold("Inventory") << " (" << Colorize(kColorOrange, std::to_string(InResult.mItems.size())) << " docs)\n\n";
    HumanTable Table;
    Table.mHeaders = {"Path", "Lines", "LastCommit"};
    for (const InventoryItem& Item : InResult.mItems)
    {
        Table.AddRow({Colorize(kColorDim, Item.mPath), Colorize(kColorOrange, std::to_string(Item.mLineCount)), Item.mLastCommit});
    }
    Table.Print();
    PrintHumanWarnings(InResult.mWarnings);
    return 0;
}


int RunOrphanCheckHuman(const OrphanCheckResult& InResult)
{
    std::cout << ColorBold("Orphan docs: ") << Colorize(kColorOrange, std::to_string(InResult.mOrphans.size())) << "\n";
    for (const std::string& Orphan : InResult.mOrphans)
    {
        std::cout << Colorize(kColorRed, "ORPHAN") << " " << Colorize(kColorDim, Orphan) << "\n";
    }
    PrintHumanWarnings(InResult.mWarnings);
    return 0;
}


int RunArtifactsHuman(const Inventory& InInventory, const std::string& InTopicKey, const std::string& InKind)
{
    const DocumentRecord* Plan = FindSingleRecordByTopic(InInventory.mPlans, InTopicKey);
    const DocumentRecord* Implementation = FindSingleRecordByTopic(InInventory.mImplementations, InTopicKey);
    const std::vector<DocumentRecord> Playbooks = CollectRecordsByTopic(InInventory.mPlaybooks, InTopicKey);
    const std::vector<SidecarRecord> Sidecars = CollectSidecarsByTopic(InInventory.mSidecars, InTopicKey);

    std::cout << ColorBold("Topic: ") << Colorize(kColorOrange, InTopicKey) << "\n";
    std::cout << ColorBold("Kind: ") << InKind << "\n";
    std::cout << ColorBold("PairState: ") << ColorizeStatus(ResolvePairStateForTopic(InInventory, InTopicKey)) << "\n";
    if ((InKind == "all" || InKind == "plan") && Plan != nullptr)
    {
        std::cout << ColorBold("Plan: ") << Colorize(kColorDim, Plan->mPath) << " (" << ColorizeStatus(GetDisplayStatus(Plan->mStatus)) << ")\n";
    }
    if ((InKind == "all" || InKind == "implementation") && Implementation != nullptr)
    {
        std::cout << ColorBold("Implementation: ") << Colorize(kColorDim, Implementation->mPath) << " (" << ColorizeStatus(GetDisplayStatus(Implementation->mStatus)) << ")\n";
    }
    if (InKind == "all" || InKind == "playbook")
    {
        std::cout << ColorBold("Playbooks: ") << Colorize(kColorOrange, std::to_string(Playbooks.size())) << "\n";
        for (const DocumentRecord& Playbook : Playbooks)
        {
            std::cout << "  - [" << Colorize(kColorOrange, Playbook.mPhaseKey) << "] " << Colorize(kColorDim, Playbook.mPath) << " (" << ColorizeStatus(GetDisplayStatus(Playbook.mStatus)) << ")\n";
        }
    }
    if (InKind == "all" || InKind == "sidecar")
    {
        std::cout << ColorBold("Sidecars: ") << Colorize(kColorOrange, std::to_string(Sidecars.size())) << "\n";
        for (const SidecarRecord& Sidecar : Sidecars)
        {
            std::cout << "  - " << Sidecar.mOwnerKind << " " << (Sidecar.mPhaseKey.empty() ? "-" : Sidecar.mPhaseKey) << " " << Sidecar.mDocKind << ": " << Colorize(kColorDim, Sidecar.mPath) << "\n";
        }
    }
    PrintHumanWarnings(InInventory.mWarnings);
    return 0;
}


int RunEvidenceHuman(const std::string& InLabel, const std::string& InTopicKey, const std::string& InDocClass, const std::vector<EvidenceEntry>& InEntries, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold(InLabel) << " topic=" << Colorize(kColorOrange, InTopicKey) << " doc_class=" << InDocClass << " entries=" << Colorize(kColorOrange, std::to_string(InEntries.size())) << "\n\n";
    HumanTable Table;
    Table.mHeaders = {"Source", "Phase", "TableId", "Row", "Fields"};
    for (const EvidenceEntry& Entry : InEntries)
    {
        std::ostringstream Fields;
        for (size_t FieldIndex = 0; FieldIndex < Entry.mFields.size(); ++FieldIndex)
        {
            if (FieldIndex > 0)
            {
                Fields << ", ";
            }
            Fields << Entry.mFields[FieldIndex].first << "=" << Entry.mFields[FieldIndex].second;
        }
        Table.AddRow({Colorize(kColorDim, Entry.mSourcePath), Entry.mPhaseKey.empty() ? "-" : Entry.mPhaseKey, std::to_string(Entry.mTableId), std::to_string(Entry.mRowIndex), Fields.str()});
    }
    Table.Print();
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunSchemeHuman(const std::string& InType, const std::vector<SchemaField>& InFields, const std::vector<std::string>& InExamples, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Schema type: ") << Colorize(kColorOrange, InType) << "\n";
    std::cout << ColorBold("Fields: ") << Colorize(kColorOrange, std::to_string(InFields.size())) << "\n\n";
    HumanTable Table;
    Table.mHeaders = {"Section", "Property", "Value"};
    for (const SchemaField& Field : InFields)
    {
        Table.AddRow({Colorize(kColorDim, Field.mSectionId), Colorize(kColorOrange, Field.mProperty), Field.mValue});
    }
    Table.Print();
    if (!InExamples.empty())
    {
        std::cout << "\n" << ColorBold("Examples:") << "\n";
        for (const std::string& Example : InExamples)
        {
            std::cout << "  - " << Example << "\n";
        }
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunRulesHuman(const std::vector<RuleEntry>& InRules, const std::vector<std::string>& InWarnings)
{
    int ResolvedCount = 0;
    for (const RuleEntry& Rule : InRules)
    {
        if (Rule.mbSourceResolved)
        {
            ResolvedCount += 1;
        }
    }

    std::cout << ColorBold("Rules: ") << Colorize(kColorOrange, std::to_string(InRules.size())) << " (resolved=" << Colorize(kColorGreen, std::to_string(ResolvedCount))
              << ", unresolved=" << Colorize(kColorYellow, std::to_string(static_cast<int>(InRules.size()) - ResolvedCount)) << ")\n\n";
    HumanTable Table;
    Table.mHeaders = {"Id", "Description", "Source", "Resolved"};
    for (const RuleEntry& Rule : InRules)
    {
        Table.AddRow({Colorize(kColorOrange, Rule.mId), Rule.mDescription, Colorize(kColorDim, Rule.mSource), Rule.mbSourceResolved ? Colorize(kColorGreen, "yes") : Colorize(kColorYellow, "no")});
    }
    Table.Print();
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunValidateHuman(const bool InStrict, const bool InOk, const std::vector<ValidateCheck>& InChecks, const std::vector<std::string>& InErrors, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Validate: ") << (InOk ? Colorize(kColorGreen, "ok") : Colorize(kColorRed, "failed")) << " (strict=" << (InStrict ? "true" : "false") << ")\n\n";
    HumanTable Table;
    Table.mHeaders = {"Check", "Result", "Critical", "Detail"};
    for (const ValidateCheck& Check : InChecks)
    {
        std::string Detail = Check.mDetail;
        if (!Check.mRuleId.empty())
        {
            Detail = "[" + Check.mRuleId + "] " + Detail;
        }
        Table.AddRow({Colorize(kColorOrange, Check.mId), Check.mbOk ? Colorize(kColorGreen, "ok") : Colorize(kColorRed, "failed"), Check.mbCritical ? Colorize(kColorYellow, "yes") : "no", Detail});
    }
    Table.Print();
    for (const ValidateCheck& Check : InChecks)
    {
        if (Check.mDiagnostics.empty())
        {
            continue;
        }
        std::cout << "\n" << Colorize(kColorOrange, Check.mId) << " diagnostics:\n";
        for (const std::string& Diagnostic : Check.mDiagnostics)
        {
            std::cout << "  - " << Diagnostic << "\n";
        }
    }
    for (const std::string& Error : InErrors)
    {
        std::cout << Colorize(kColorRed, "ERROR") << " " << Error << "\n";
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunSectionResolveHuman(const std::string& InDocPath, const SectionResolution& InResolution, const std::vector<std::string>& InWarnings)
{
    if (!InResolution.mbFound)
    {
        std::cout << "Section " << Colorize(kColorRed, "not found") << " in " << Colorize(kColorDim, InDocPath) << ": " << Colorize(kColorOrange, InResolution.mSectionQuery) << "\n";
    }
    else
    {
        std::cout << "Section " << Colorize(kColorOrange, InResolution.mSectionId) << " (" << InResolution.mSectionHeading << ") " << Colorize(kColorGreen, "lines " + std::to_string(InResolution.mStartLine) + "-" + std::to_string(InResolution.mEndLine))
                  << "\n";
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunSectionSchemaHuman(const std::string& InType, const std::vector<SectionSchemaEntry>& InEntries)
{
    std::cout << ColorBold("Section schema: " + InType) << " (" << Colorize(kColorOrange, std::to_string(InEntries.size())) << " sections)\n\n";
    HumanTable Table;
    Table.mHeaders = {"Order", "Section ID", "Required"};
    for (const SectionSchemaEntry& Entry : InEntries)
    {
        const std::string RequiredLabel = Entry.mbRequired ? Colorize(kColorGreen, "required") : Colorize(kColorDim, "optional");
        Table.AddRow({std::to_string(Entry.mOrder), Colorize(kColorOrange, Entry.mSectionId), RequiredLabel});
    }
    Table.Print();
    return 0;
}


int RunSectionListHuman(const std::vector<SectionCount>& InCounts, const bool InShowCount)
{
    std::cout << ColorBold("Section list") << " (" << Colorize(kColorOrange, std::to_string(InCounts.size())) << " unique headings)\n\n";
    if (InShowCount)
    {
        HumanTable Table;
        Table.mHeaders = {"Heading", "Section ID", "Count"};
        for (const SectionCount& Item : InCounts)
        {
            Table.AddRow({Colorize(kColorGreen, Item.mHeading), Colorize(kColorOrange, Item.mSectionId), Colorize(kColorGreen, std::to_string(Item.mCount))});
        }
        Table.Print();
    }
    else
    {
        for (const SectionCount& Item : InCounts)
        {
            std::cout << Colorize(kColorGreen, Item.mHeading) << " " << Colorize(kColorDim, "[" + Item.mSectionId + "]") << "\n";
        }
    }
    return 0;
}


int RunSectionListDocHuman(const std::string& InDocPath, const std::vector<HeadingRecord>& InHeadings)
{
    std::cout << ColorBold("Sections") << " " << Colorize(kColorDim, InDocPath) << " (" << Colorize(kColorOrange, std::to_string(InHeadings.size())) << " headings)\n\n";

    if (InHeadings.empty())
    {
        std::cout << Colorize(kColorYellow, "No headings found.") << "\n";
        return 0;
    }

    // Build tree with Unicode box-drawing connectors.
    // For each heading, determine its children by scanning forward until a heading
    // at the same or higher level is found. Use a stack to track connector state.
    for (size_t Index = 0; Index < InHeadings.size(); ++Index)
    {
        const HeadingRecord& Heading = InHeadings[Index];
        const int Level = Heading.mLevel;

        // Determine if this heading is the last child among its siblings.
        // A sibling is the next heading at the same level with no intervening heading
        // at a lower (parent) level.
        bool bIsLastChild = true;
        for (size_t NextIndex = Index + 1; NextIndex < InHeadings.size(); ++NextIndex)
        {
            if (InHeadings[NextIndex].mLevel < Level)
            {
                break;
            }
            if (InHeadings[NextIndex].mLevel == Level)
            {
                bIsLastChild = false;
                break;
            }
        }

        // Build prefix by checking ancestors at each level.
        // For levels 2..Level-1, determine if the ancestor at that level has more siblings.
        std::string Prefix;
        for (int PrefixLevel = 2; PrefixLevel < Level; ++PrefixLevel)
        {
            // Check if there's a future heading at PrefixLevel that is an ancestor connector.
            bool bAncestorHasMore = false;
            for (size_t FutureIndex = Index + 1; FutureIndex < InHeadings.size(); ++FutureIndex)
            {
                if (InHeadings[FutureIndex].mLevel < PrefixLevel)
                {
                    break;
                }
                if (InHeadings[FutureIndex].mLevel == PrefixLevel)
                {
                    bAncestorHasMore = true;
                    break;
                }
            }
            Prefix += bAncestorHasMore ? "\xe2\x94\x82   " : "    ";
        }

        // Add connector for this heading (skip for H1 root).
        if (Level > 1)
        {
            Prefix += bIsLastChild ? "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 " : "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ";
        }

        const std::string Label = Colorize(kColorGreen, Heading.mText) + Colorize(kColorDim, " [" + Heading.mSectionId + "]") + Colorize(kColorDim, " (H" + std::to_string(Level) + ", L" + std::to_string(Heading.mLine) + ")");
        std::cout << Prefix << Label << "\n";
    }

    return 0;
}


int RunExcerptHuman(const std::string& InDocPath, const SectionResolution& InResolution, const int InExcerptStartLine, const std::vector<std::string>& InExcerptLines, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Excerpt ") << Colorize(kColorDim, InDocPath) << " [" << Colorize(kColorOrange, InResolution.mSectionId) << "]\n";
    for (size_t LineIndex = 0; LineIndex < InExcerptLines.size(); ++LineIndex)
    {
        const int LineNumber = InExcerptStartLine + static_cast<int>(LineIndex);
        std::cout << Colorize(kColorDim, std::to_string(LineNumber));
        const size_t NumWidth = std::to_string(LineNumber).size();
        if (NumWidth < 6)
        {
            std::cout << std::string(6 - NumWidth, ' ');
        }
        std::cout << " | " << InExcerptLines[LineIndex] << "\n";
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunSectionContentHuman(
    const std::string& InDocPath,
    const SectionResolution& InResolution,
    const int InLineCharLimit,
    const int InContentStartLine,
    const std::vector<std::string>& InContentLines,
    const std::vector<MarkdownTableRecord>& InTables,
    const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Section content ") << "[" << Colorize(kColorOrange, InResolution.mSectionId) << "] " << Colorize(kColorDim, InDocPath) << "\n\n";

    // Build a set of line ranges that are tables so we can render them via HumanTable.
    // Each entry maps a 1-based line number to a table index.
    std::map<int, size_t> TableStartLines;
    std::set<int> TableCoveredLines;
    for (size_t TableIndex = 0; TableIndex < InTables.size(); ++TableIndex)
    {
        const MarkdownTableRecord& Table = InTables[TableIndex];
        TableStartLines[Table.mStartLine] = TableIndex;
        for (int Line = Table.mStartLine; Line <= Table.mEndLine; ++Line)
        {
            TableCoveredLines.insert(Line);
        }
    }

    for (size_t LineIndex = 0; LineIndex < InContentLines.size(); ++LineIndex)
    {
        const int LineNumber = InContentStartLine + static_cast<int>(LineIndex);

        // If this line is inside a table region but not the start, skip it.
        if (TableCoveredLines.count(LineNumber) && !TableStartLines.count(LineNumber))
        {
            continue;
        }

        // If this is the start of a table, render it via HumanTable.
        auto TableIt = TableStartLines.find(LineNumber);
        if (TableIt != TableStartLines.end())
        {
            const MarkdownTableRecord& Table = InTables[TableIt->second];
            HumanTable HTable;
            if (InLineCharLimit > 0)
            {
                std::vector<std::string> TruncatedHeaders;
                TruncatedHeaders.reserve(Table.mHeaders.size());
                for (const std::string& Header : Table.mHeaders)
                {
                    TruncatedHeaders.push_back(TruncateForDisplay(Header, static_cast<size_t>(InLineCharLimit)));
                }
                HTable.mHeaders = TruncatedHeaders;
                for (const std::vector<std::string>& Row : Table.mRows)
                {
                    std::vector<std::string> TruncatedRow;
                    TruncatedRow.reserve(Row.size());
                    for (const std::string& Cell : Row)
                    {
                        TruncatedRow.push_back(TruncateForDisplay(Cell, static_cast<size_t>(InLineCharLimit)));
                    }
                    HTable.AddRow(TruncatedRow);
                }
            }
            else
            {
                HTable.mHeaders = Table.mHeaders;
                for (const std::vector<std::string>& Row : Table.mRows)
                {
                    HTable.AddRow(Row);
                }
            }
            std::cout << "\n";
            HTable.Print();
            std::cout << "\n";
            continue;
        }

        // Regular line: print with dim line number.
        std::string Text = InContentLines[LineIndex];
        if (InLineCharLimit > 0)
        {
            Text = TruncateForDisplay(Text, static_cast<size_t>(InLineCharLimit));
        }
        std::cout << Colorize(kColorDim, std::to_string(LineNumber));
        const size_t NumWidth = std::to_string(LineNumber).size();
        if (NumWidth < 6)
        {
            std::cout << std::string(6 - NumWidth, ' ');
        }
        std::cout << " | " << Text << "\n";
    }

    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunTableListHuman(const std::string& InDocPath, const std::vector<MarkdownTableRecord>& InTables, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Tables in ") << Colorize(kColorDim, InDocPath) << " (" << Colorize(kColorOrange, std::to_string(InTables.size())) << ")\n\n";
    HumanTable Table;
    Table.mHeaders = {"Id", "Section", "Lines", "Cols", "Rows"};
    for (const MarkdownTableRecord& Record : InTables)
    {
        Table.AddRow(
            {Colorize(kColorOrange, std::to_string(Record.mTableId)),
             Record.mSectionId.empty() ? "-" : Record.mSectionId,
             std::to_string(Record.mStartLine) + "-" + std::to_string(Record.mEndLine),
             std::to_string(Record.mHeaders.size()),
             std::to_string(Record.mRows.size())});
    }
    Table.Print();
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunTableGetHuman(const std::string& InDocPath, const MarkdownTableRecord& InTable, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Table " + std::to_string(InTable.mTableId)) << " in " << Colorize(kColorDim, InDocPath) << "\n\n";
    HumanTable Table;
    Table.mHeaders = InTable.mHeaders;
    for (const std::vector<std::string>& Row : InTable.mRows)
    {
        Table.AddRow(Row);
    }
    Table.Print();
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunGraphHuman(const std::string& InTopicKey, const int InDepth, const std::vector<GraphNode>& InNodes, const std::vector<GraphEdge>& InEdges, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Graph") << " topic=" << Colorize(kColorOrange, InTopicKey) << " depth=" << InDepth << " nodes=" << Colorize(kColorOrange, std::to_string(InNodes.size())) << " edges=" << Colorize(kColorOrange, std::to_string(InEdges.size()))
              << "\n\n";

    const size_t MaxNodeRows = 120;
    const size_t MaxEdgeRows = 180;
    {
        std::cout << ColorBold("Nodes:") << "\n";
        HumanTable NodeTable;
        NodeTable.mHeaders = {"Id", "Type", "Path"};
        for (size_t Index = 0; Index < InNodes.size() && Index < MaxNodeRows; ++Index)
        {
            const GraphNode& Node = InNodes[Index];
            NodeTable.AddRow({Colorize(kColorOrange, Node.mId), Colorize(kColorGreen, Node.mType), Colorize(kColorDim, Node.mPath)});
        }
        NodeTable.Print();
        if (InNodes.size() > MaxNodeRows)
        {
            std::cout << Colorize(kColorDim, "... " + std::to_string(InNodes.size() - MaxNodeRows) + " more nodes omitted") << "\n";
        }
    }

    std::cout << "\n";
    {
        std::cout << ColorBold("Edges:") << "\n";
        HumanTable EdgeTable;
        EdgeTable.mHeaders = {"From", "To", "Kind", "Depth"};
        for (size_t Index = 0; Index < InEdges.size() && Index < MaxEdgeRows; ++Index)
        {
            const GraphEdge& Edge = InEdges[Index];
            EdgeTable.AddRow({Colorize(kColorOrange, Edge.mFromNodeId), Edge.mToNodeId, Colorize(kColorGreen, Edge.mKind), std::to_string(Edge.mDepth)});
        }
        EdgeTable.Print();
        if (InEdges.size() > MaxEdgeRows)
        {
            std::cout << Colorize(kColorDim, "... " + std::to_string(InEdges.size() - MaxEdgeRows) + " more edges omitted") << "\n";
        }
    }

    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunDiagnoseDriftHuman(const bool InOk, const std::vector<DriftItem>& InDrifts, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Diagnose drift: ") << (InOk ? Colorize(kColorGreen, "ok") : Colorize(kColorRed, "drift_detected")) << " (" << Colorize(kColorOrange, std::to_string(InDrifts.size())) << " items)\n\n";
    if (!InDrifts.empty())
    {
        HumanTable Table;
        Table.mHeaders = {"Severity", "Id", "Topic", "Message"};
        for (const DriftItem& Drift : InDrifts)
        {
            const char* SeverityColor = (Drift.mSeverity == "error") ? kColorRed : kColorYellow;
            Table.AddRow({Colorize(SeverityColor, Drift.mSeverity), Drift.mId, Drift.mTopicKey.empty() ? "-" : Colorize(kColorOrange, Drift.mTopicKey), Drift.mMessage});
        }
        Table.Print();
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunTimelineHuman(const std::string& InTopicKey, const std::string& InSince, const std::vector<TimelineItem>& InItems, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Timeline") << " topic=" << Colorize(kColorOrange, InTopicKey) << " since=" << (InSince.empty() ? "-" : InSince) << " entries=" << Colorize(kColorOrange, std::to_string(InItems.size())) << "\n\n";
    if (!InItems.empty())
    {
        HumanTable Table;
        Table.mHeaders = {"Date", "Class", "Phase", "Update", "Source"};
        for (const TimelineItem& Item : InItems)
        {
            Table.AddRow({Colorize(kColorOrange, GetDisplayStatus(Item.mDate)), Colorize(kColorGreen, Item.mDocClass), Item.mPhaseKey.empty() ? "-" : Item.mPhaseKey, Item.mUpdate, Colorize(kColorDim, Item.mSourcePath)});
        }
        Table.Print();
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunPhaseHuman(const std::string& InTopicKey, const std::string& InPlanPath, const std::string& InStatusFilter, const std::vector<PhaseItem>& InItems, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Phase list") << " topic=" << Colorize(kColorOrange, InTopicKey) << " status=" << InStatusFilter << " count=" << Colorize(kColorOrange, std::to_string(InItems.size())) << " plan=" << Colorize(kColorDim, InPlanPath) << "\n\n";
    if (!InItems.empty())
    {
        HumanTable Table;
        Table.mHeaders = {"Phase", "Status", "Description", "Next Action", "Playbook"};
        for (const PhaseItem& Item : InItems)
        {
            const std::string DescDisplay = Item.mDescription.empty() ? std::string("-") : TruncateForDisplay(Item.mDescription, 40);
            const std::string NextDisplay = Item.mNextAction.empty() ? std::string("-") : TruncateForDisplay(Item.mNextAction, 40);
            Table.AddRow({Colorize(kColorOrange, Item.mPhaseKey), ColorizeStatus(Item.mStatus), Colorize(kColorDim, DescDisplay), Colorize(kColorDim, NextDisplay), Item.mPlaybookPath.empty() ? "-" : Colorize(kColorDim, Item.mPlaybookPath)});
        }
        Table.Print();
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunPhaseListAllHuman(const std::string& InStatusFilter, const std::vector<PhaseListAllEntry>& InEntries, const std::vector<std::string>& InWarnings)
{
    size_t TotalPhases = 0;
    for (const PhaseListAllEntry& Entry : InEntries)
    {
        TotalPhases += Entry.mPhases.size();
    }

    std::cout << ColorBold("Phase List (all)") << " status=" << InStatusFilter << " plans=" << Colorize(kColorOrange, std::to_string(InEntries.size())) << " phases=" << Colorize(kColorOrange, std::to_string(TotalPhases)) << "\n\n";

    for (size_t PlanIndex = 0; PlanIndex < InEntries.size(); ++PlanIndex)
    {
        const PhaseListAllEntry& Entry = InEntries[PlanIndex];
        if (PlanIndex > 0)
        {
            std::cout << "\n";
        }
        std::cout << Colorize(kColorOrange, Entry.mTopicKey) << "  " << ColorizeStatus(Entry.mPlanStatus) << "  " << Colorize(kColorDim, Entry.mPlanPath) << "\n";
        for (size_t PhaseIndex = 0; PhaseIndex < Entry.mPhases.size(); ++PhaseIndex)
        {
            const PhaseItem& Item = Entry.mPhases[PhaseIndex];
            const bool bIsLast = (PhaseIndex + 1 == Entry.mPhases.size());
            const char* Connector = bIsLast ? "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 " : "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ";
            const std::string DescDisplay = Item.mDescription.empty() ? std::string() : "  " + TruncateForDisplay(Item.mDescription, 50);
            std::cout << "  " << Connector << Colorize(kColorOrange, Item.mPhaseKey) << "  " << ColorizeStatus(Item.mStatus) << Colorize(kColorDim, DescDisplay) << "\n";
        }
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunBlockersHuman(const std::string& InStatusFilter, const std::vector<BlockerItem>& InItems, const std::vector<std::string>& InWarnings)
{
    std::cout << ColorBold("Blockers") << " status=" << InStatusFilter << " count=" << Colorize(kColorOrange, std::to_string(InItems.size())) << "\n\n";
    if (!InItems.empty())
    {
        HumanTable Table;
        Table.mHeaders = {"Topic", "Kind", "Status", "Phase", "Priority", "Action"};
        for (const BlockerItem& Item : InItems)
        {
            Table.AddRow({Colorize(kColorOrange, Item.mTopicKey), Item.mKind, ColorizeStatus(Item.mStatus), Item.mPhaseKey.empty() ? "-" : Item.mPhaseKey, Item.mPriority.empty() ? "-" : Colorize(kColorYellow, Item.mPriority), Item.mAction});
        }
        Table.Print();
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}


int RunCacheInfoHuman(const CacheInfoResult& InResult)
{
    std::cout << ColorBold("Cache Info") << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Property", "Value"};
    Table.AddRow({"Cache directory", Colorize(kColorOrange, InResult.mCacheDir)});
    Table.AddRow({"Exists", InResult.mbCacheExists ? Colorize(kColorGreen, "yes") : Colorize(kColorDim, "no")});
    if (InResult.mbCacheExists)
    {
        Table.AddRow({"Size", Colorize(kColorOrange, FormatBytesHuman(InResult.mCacheSizeBytes))});
        Table.AddRow({"Entries", Colorize(kColorOrange, std::to_string(InResult.mCacheEntryCount))});
    }
    Table.AddRow({"Current repo cache", Colorize(kColorDim, InResult.mCurrentRepoCachePath)});
    Table.AddRow({"Current repo cached", InResult.mbCurrentRepoCacheExists ? Colorize(kColorGreen, "yes") : Colorize(kColorDim, "no")});
    Table.AddRow({"Config dir override", InResult.mConfigCacheDir.empty() ? Colorize(kColorDim, "(default)") : Colorize(kColorOrange, InResult.mConfigCacheDir)});
    Table.AddRow({"Cache enabled", InResult.mbCacheEnabled ? Colorize(kColorGreen, "true") : Colorize(kColorYellow, "false")});
    Table.AddRow({"Cache verbose", InResult.mbCacheVerbose ? Colorize(kColorGreen, "true") : Colorize(kColorDim, "false")});
    Table.AddRow({"INI path", Colorize(kColorDim, InResult.mIniPath)});
    Table.Print();

    PrintHumanWarnings(InResult.mWarnings);
    return 0;
}


int RunCacheClearHuman(const CacheClearResult& InResult)
{
    std::cout << ColorBold("Cache Clear") << "\n\n";

    if (!InResult.mbSuccess)
    {
        std::cout << Colorize(kColorRed, "Error: " + InResult.mError) << "\n";
        return 1;
    }

    if (InResult.mEntriesRemoved == 0 && InResult.mBytesFreed == 0)
    {
        std::cout << Colorize(kColorDim, "Cache is already empty.") << "\n";
    }
    else
    {
        std::cout << "Cleared " << Colorize(kColorOrange, std::to_string(InResult.mEntriesRemoved)) << " entries, freed " << Colorize(kColorOrange, FormatBytesHuman(InResult.mBytesFreed)) << ".\n";
    }

    PrintHumanWarnings(InResult.mWarnings);
    return InResult.mbSuccess ? 0 : 1;
}


int RunCacheConfigHuman(const CacheConfigResult& InResult)
{
    std::cout << ColorBold("Cache Config") << "\n\n";

    if (!InResult.mbSuccess)
    {
        std::cout << Colorize(kColorRed, "Error: " + InResult.mError) << "\n";
        return 1;
    }

    std::cout << "Configuration written to: " << Colorize(kColorDim, InResult.mIniPath) << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Setting", "Value"};
    Table.AddRow({"dir", InResult.mDir.empty() ? Colorize(kColorDim, "(default)") : Colorize(kColorOrange, InResult.mDir)});
    Table.AddRow({"enabled", InResult.mbEnabled ? Colorize(kColorGreen, "true") : Colorize(kColorYellow, "false")});
    Table.AddRow({"verbose", InResult.mbVerbose ? Colorize(kColorGreen, "true") : Colorize(kColorDim, "false")});
    Table.Print();

    PrintHumanWarnings(InResult.mWarnings);
    return InResult.mbSuccess ? 0 : 1;
}

} // namespace UniPlan
