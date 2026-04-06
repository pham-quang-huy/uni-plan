#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan {

int RunListText(const Inventory &InInventory, const std::string &InKind,
                const std::string &InStatusFilter) {
  const bool bFilterActive = (InStatusFilter != "all");

  if (InKind == "pair") {
    std::cout << "TopicKey | PairState | OverallStatus | Plan | PlanStatus | "
                 "Implementation | ImplementationStatus | Playbooks\n";
    int Count = 0;
    for (const TopicPairRecord &Pair : InInventory.mPairs) {
      if (bFilterActive) {
        const std::string Status = GetDisplayStatus(Pair.mOverallStatus);
        if (!MatchesStatusFilter(InStatusFilter, Status)) {
          continue;
        }
      }
      ++Count;
      std::ostringstream PlaybookSummary;
      for (size_t Index = 0; Index < Pair.mPlaybooks.size(); ++Index) {
        if (Index > 0) {
          PlaybookSummary << ", ";
        }
        PlaybookSummary << Pair.mPlaybooks[Index].mPhaseKey;
      }
      std::cout << Pair.mTopicKey << " | " << Pair.mPairState << " | "
                << (GetDisplayStatus(Pair.mOverallStatus)) << " | "
                << (Pair.mPlanPath.empty() ? "-" : Pair.mPlanPath) << " | "
                << (GetDisplayStatus(Pair.mPlanStatus)) << " | "
                << (Pair.mImplementationPath.empty() ? "-"
                                                     : Pair.mImplementationPath)
                << " | " << (GetDisplayStatus(Pair.mImplementationStatus))
                << " | "
                << (PlaybookSummary.str().empty() ? "-" : PlaybookSummary.str())
                << "\n";
    }
    if (bFilterActive && Count == 0) {
      std::cout << "No pairs found with status '" << InStatusFilter << "'.\n";
    }
    return 0;
  }

  const std::vector<DocumentRecord> &Records =
      ResolveRecordsByKind(InInventory, InKind);
  const std::map<std::string, std::string> TopicPairStates =
      BuildTopicPairStateMap(InInventory);
  const std::set<std::string> PlanTopics = BuildTopicSet(InInventory.mPlans);
  const std::set<std::string> ImplementationTopics =
      BuildTopicSet(InInventory.mImplementations);

  const std::vector<const DocumentRecord *> FilteredRecords =
      FilterRecordsByStatus(Records, InStatusFilter);

  std::cout << "Kind: " << InKind << "\n";
  if (bFilterActive) {
    std::cout << "Status filter: " << InStatusFilter << "\n";
  }
  std::cout << "Count: " << FilteredRecords.size() << "\n";
  if (bFilterActive && FilteredRecords.empty()) {
    std::cout << "No " << InKind << " found with status '" << InStatusFilter
              << "'.\n";
    return 0;
  }
  for (const DocumentRecord *RecordPtr : FilteredRecords) {
    const DocumentRecord &Record = *RecordPtr;
    std::string PairState = "unknown";
    if (InKind == "playbook") {
      PairState =
          DerivePlaybookPairState(Record, PlanTopics, ImplementationTopics);
    } else {
      const auto PairStateIt = TopicPairStates.find(Record.mTopicKey);
      if (PairStateIt != TopicPairStates.end()) {
        PairState = PairStateIt->second;
      }
    }

    std::cout << "- " << Record.mTopicKey;
    if (InKind == "playbook") {
      std::cout << " [" << Record.mPhaseKey << "]";
    }
    std::cout << " (status=" << (GetDisplayStatus(Record.mStatus))
              << ", pair_state=" << PairState << "): " << Record.mPath << "\n";
  }
  return 0;
}

int RunLintText(const LintResult &InResult) {
  for (const std::string &Warning : InResult.mWarnings) {
    std::cout << Warning << "\n";
  }
  std::cout << "Lint warnings: " << InResult.mWarningCount << "\n";
  return 0;
}

int RunInventoryText(const InventoryResult &InResult) {
  std::cout << std::left << std::setw(90) << "Path"
            << " " << std::setw(8) << "Lines"
            << " LastCommit\n";
  for (const InventoryItem &Item : InResult.mItems) {
    std::cout << std::left << std::setw(90) << Item.mPath << " " << std::setw(8)
              << Item.mLineCount << " " << Item.mLastCommit << "\n";
  }
  std::cout << "\nTotal docs: " << InResult.mItems.size() << "\n";
  PrintTextWarnings(InResult.mWarnings);
  return 0;
}

int RunOrphanCheckText(const OrphanCheckResult &InResult) {
  std::cout << "Orphan docs: " << InResult.mOrphans.size() << "\n";
  for (const std::string &Orphan : InResult.mOrphans) {
    std::cout << "ORPHAN: " << Orphan << "\n";
  }
  PrintTextWarnings(InResult.mWarnings);
  return 0;
}

int RunArtifactsText(const Inventory &InInventory,
                     const std::string &InTopicKey, const std::string &InKind) {
  const DocumentRecord *Plan =
      FindSingleRecordByTopic(InInventory.mPlans, InTopicKey);
  const DocumentRecord *Implementation =
      FindSingleRecordByTopic(InInventory.mImplementations, InTopicKey);
  const std::vector<DocumentRecord> Playbooks =
      CollectRecordsByTopic(InInventory.mPlaybooks, InTopicKey);
  const std::vector<SidecarRecord> Sidecars =
      CollectSidecarsByTopic(InInventory.mSidecars, InTopicKey);

  std::cout << "Topic: " << InTopicKey << "\n";
  std::cout << "Kind: " << InKind << "\n";
  std::cout << "PairState: "
            << ResolvePairStateForTopic(InInventory, InTopicKey) << "\n";
  if ((InKind == "all" || InKind == "plan") && Plan != nullptr) {
    std::cout << "Plan: " << Plan->mPath
              << " (status=" << (GetDisplayStatus(Plan->mStatus)) << ")\n";
  }
  if ((InKind == "all" || InKind == "implementation") &&
      Implementation != nullptr) {
    std::cout << "Implementation: " << Implementation->mPath
              << " (status=" << (GetDisplayStatus(Implementation->mStatus))
              << ")\n";
  }
  if (InKind == "all" || InKind == "playbook") {
    std::cout << "Playbooks: " << Playbooks.size() << "\n";
    for (const DocumentRecord &Playbook : Playbooks) {
      std::cout << "- [" << Playbook.mPhaseKey << "] " << Playbook.mPath
                << " (status=" << (GetDisplayStatus(Playbook.mStatus)) << ")\n";
    }
  }
  if (InKind == "all" || InKind == "sidecar") {
    std::cout << "Sidecars: " << Sidecars.size() << "\n";
    for (const SidecarRecord &Sidecar : Sidecars) {
      std::cout << "- " << Sidecar.mOwnerKind << " "
                << (Sidecar.mPhaseKey.empty() ? "-" : Sidecar.mPhaseKey) << " "
                << Sidecar.mDocKind << ": " << Sidecar.mPath << "\n";
    }
  }

  PrintTextWarnings(InInventory.mWarnings);
  return 0;
}

int RunEvidenceText(const std::string &InLabel, const std::string &InTopicKey,
                    const std::string &InDocClass,
                    const std::vector<EvidenceEntry> &InEntries,
                    const std::vector<std::string> &InWarnings) {
  std::cout << InLabel << " topic=" << InTopicKey << " doc_class=" << InDocClass
            << " entries=" << InEntries.size() << "\n";
  for (const EvidenceEntry &Entry : InEntries) {
    std::cout << "- " << Entry.mSourcePath;
    if (!Entry.mPhaseKey.empty()) {
      std::cout << " [" << Entry.mPhaseKey << "]";
    }
    std::cout << " table=" << Entry.mTableId << " row=" << Entry.mRowIndex;
    for (const auto &Field : Entry.mFields) {
      std::cout << " | " << Field.first << ": " << Field.second;
    }
    std::cout << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunSchemaText(const std::string &InType,
                  const std::vector<SchemaField> &InFields,
                  const std::vector<std::string> &InExamples,
                  const std::vector<std::string> &InWarnings) {
  std::cout << "Schema type: " << InType << "\n";
  std::cout << "Fields: " << InFields.size() << "\n";
  for (const SchemaField &Field : InFields) {
    std::cout << "- [" << (Field.mSectionId.empty() ? "-" : Field.mSectionId)
              << "] " << Field.mProperty << " = " << Field.mValue << "\n";
  }
  std::cout << "Examples:\n";
  for (const std::string &Example : InExamples) {
    std::cout << "- " << Example << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunRulesText(const std::vector<RuleEntry> &InRules,
                 const std::vector<std::string> &InWarnings) {
  int ResolvedCount = 0;
  for (const RuleEntry &Rule : InRules) {
    if (Rule.mbSourceResolved) {
      ResolvedCount += 1;
    }
  }

  std::cout << "Rules: " << InRules.size()
            << " (provenance_resolved=" << ResolvedCount << ", unresolved="
            << (static_cast<int>(InRules.size()) - ResolvedCount) << ")\n";
  for (const RuleEntry &Rule : InRules) {
    std::cout << "- [" << Rule.mId << "] " << Rule.mDescription
              << " (source=" << Rule.mSource;
    if (Rule.mbSourceResolved) {
      std::cout << ", table_id=" << Rule.mSourceTableId
                << ", row=" << Rule.mSourceRowIndex;
    }
    std::cout << ")\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunValidateText(const bool InStrict, const bool InOk,
                    const std::vector<ValidateCheck> &InChecks,
                    const std::vector<std::string> &InErrors,
                    const std::vector<std::string> &InWarnings) {
  std::cout << "Validate: " << (InOk ? "ok" : "failed")
            << " (strict=" << (InStrict ? "true" : "false") << ")\n";
  for (const ValidateCheck &Check : InChecks) {
    std::cout << "- " << Check.mId << " " << (Check.mbOk ? "ok" : "failed")
              << " (critical=" << (Check.mbCritical ? "true" : "false");
    if (!Check.mRuleId.empty()) {
      std::cout << ", rule=" << Check.mRuleId;
    }
    std::cout << "): " << Check.mDetail << "\n";
    for (const std::string &Diagnostic : Check.mDiagnostics) {
      std::cout << "  - " << Diagnostic << "\n";
    }
  }
  for (const std::string &Error : InErrors) {
    std::cout << "ERROR " << Error << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunSectionResolveText(const std::string &InDocPath,
                          const SectionResolution &InResolution,
                          const std::vector<std::string> &InWarnings) {
  if (!InResolution.mbFound) {
    std::cout << "Section not found in " << InDocPath << ": "
              << InResolution.mSectionQuery << "\n";
  } else {
    std::cout << "Section " << InResolution.mSectionId << " ("
              << InResolution.mSectionHeading << ") "
              << "lines " << InResolution.mStartLine << "-"
              << InResolution.mEndLine << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunSectionSchemaText(const std::string &InType,
                         const std::vector<SectionSchemaEntry> &InEntries) {
  std::cout << "Section schema: " << InType << "\n";
  std::cout << "Count: " << InEntries.size() << "\n";
  for (const SectionSchemaEntry &Entry : InEntries) {
    std::cout << (Entry.mbRequired ? "[required] " : "[optional] ")
              << Entry.mSectionId << "\n";
  }
  return 0;
}

int RunSectionListText(const std::vector<SectionCount> &InCounts,
                       const bool InShowCount) {
  std::cout << "Unique headings: " << InCounts.size() << "\n";
  for (const SectionCount &Item : InCounts) {
    std::cout << Item.mHeading << " [" << Item.mSectionId << "]";
    if (InShowCount) {
      std::cout << "  " << Item.mCount;
    }
    std::cout << "\n";
  }
  return 0;
}

int RunSectionListDocText(const std::string &InDocPath,
                          const std::vector<HeadingRecord> &InHeadings) {
  std::cout << "Doc: " << InDocPath << "\n";
  std::cout << "Sections: " << InHeadings.size() << "\n";
  for (const HeadingRecord &Heading : InHeadings) {
    std::cout << "H" << Heading.mLevel << " L" << Heading.mLine << ": "
              << Heading.mSectionId << " (" << Heading.mText << ")\n";
  }
  return 0;
}

int RunExcerptText(const std::string &InDocPath,
                   const SectionResolution &InResolution,
                   const int InExcerptStartLine,
                   const std::vector<std::string> &InExcerptLines,
                   const std::vector<std::string> &InWarnings) {
  std::cout << "Excerpt " << InDocPath << " [" << InResolution.mSectionId
            << "]\n";
  for (size_t LineIndex = 0; LineIndex < InExcerptLines.size(); ++LineIndex) {
    const int LineNumber = InExcerptStartLine + static_cast<int>(LineIndex);
    std::cout << std::setw(6) << LineNumber << " | "
              << InExcerptLines[LineIndex] << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunTableListText(const std::string &InDocPath,
                     const std::vector<MarkdownTableRecord> &InTables,
                     const std::vector<std::string> &InWarnings) {
  std::cout << "Table count in " << InDocPath << ": " << InTables.size()
            << "\n";
  for (const MarkdownTableRecord &Table : InTables) {
    std::cout << "- table_id=" << Table.mTableId << " section="
              << (Table.mSectionId.empty() ? "-" : Table.mSectionId)
              << " lines=" << Table.mStartLine << "-" << Table.mEndLine
              << " columns=" << Table.mHeaders.size()
              << " rows=" << Table.mRows.size() << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunTableGetText(const std::string &InDocPath,
                    const MarkdownTableRecord &InTable,
                    const std::vector<std::string> &InWarnings) {
  std::cout << "Table " << InTable.mTableId << " in " << InDocPath << "\n";
  for (size_t HeaderIndex = 0; HeaderIndex < InTable.mHeaders.size();
       ++HeaderIndex) {
    if (HeaderIndex > 0) {
      std::cout << " | ";
    }
    std::cout << InTable.mHeaders[HeaderIndex];
  }
  std::cout << "\n";
  for (const std::vector<std::string> &Row : InTable.mRows) {
    for (size_t CellIndex = 0; CellIndex < Row.size(); ++CellIndex) {
      if (CellIndex > 0) {
        std::cout << " | ";
      }
      std::cout << Row[CellIndex];
    }
    std::cout << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunGraphText(const std::string &InTopicKey, const int InDepth,
                 const std::vector<GraphNode> &InNodes,
                 const std::vector<GraphEdge> &InEdges,
                 const std::vector<std::string> &InWarnings) {
  std::cout << "Graph topic=" << InTopicKey << " depth=" << InDepth
            << " nodes=" << InNodes.size() << " edges=" << InEdges.size()
            << "\n";

  const size_t MaxNodeRows = 120;
  const size_t MaxEdgeRows = 180;
  for (size_t Index = 0; Index < InNodes.size() && Index < MaxNodeRows;
       ++Index) {
    const GraphNode &Node = InNodes[Index];
    std::cout << "NODE " << Node.mId << " (" << Node.mType
              << "): " << Node.mPath << "\n";
  }
  if (InNodes.size() > MaxNodeRows) {
    std::cout << "... " << (InNodes.size() - MaxNodeRows)
              << " more nodes omitted\n";
  }

  for (size_t Index = 0; Index < InEdges.size() && Index < MaxEdgeRows;
       ++Index) {
    const GraphEdge &Edge = InEdges[Index];
    std::cout << "EDGE " << Edge.mFromNodeId << " -> " << Edge.mToNodeId << " ("
              << Edge.mKind << ", depth=" << Edge.mDepth << ")\n";
  }
  if (InEdges.size() > MaxEdgeRows) {
    std::cout << "... " << (InEdges.size() - MaxEdgeRows)
              << " more edges omitted\n";
  }

  PrintTextWarnings(InWarnings);
  return 0;
}

int RunDiagnoseDriftText(const bool InOk,
                         const std::vector<DriftItem> &InDrifts,
                         const std::vector<std::string> &InWarnings) {
  std::cout << "Diagnose drift: " << (InOk ? "ok" : "drift_detected") << " ("
            << InDrifts.size() << " items)\n";
  for (const DriftItem &Drift : InDrifts) {
    std::cout << "- [" << Drift.mSeverity << "] " << Drift.mId;
    if (!Drift.mTopicKey.empty()) {
      std::cout << " topic=" << Drift.mTopicKey;
    }
    if (!Drift.mPath.empty()) {
      std::cout << " path=" << Drift.mPath;
    }
    std::cout << ": " << Drift.mMessage << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunTimelineText(const std::string &InTopicKey, const std::string &InSince,
                    const std::vector<TimelineItem> &InItems,
                    const std::vector<std::string> &InWarnings) {
  std::cout << "Timeline topic=" << InTopicKey
            << " since=" << (InSince.empty() ? "-" : InSince)
            << " entries=" << InItems.size() << "\n";
  for (const TimelineItem &Item : InItems) {
    std::cout << "- " << (Item.mDate.empty() ? "unknown-date" : Item.mDate)
              << " [" << Item.mDocClass;
    if (!Item.mPhaseKey.empty()) {
      std::cout << ":" << Item.mPhaseKey;
    }
    std::cout << "] " << Item.mUpdate;
    if (!Item.mEvidence.empty()) {
      std::cout << " | evidence: " << Item.mEvidence;
    }
    std::cout << " | source: " << Item.mSourcePath << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunPhaseText(const std::string &InTopicKey, const std::string &InPlanPath,
                 const std::string &InStatusFilter,
                 const std::vector<PhaseItem> &InItems,
                 const std::vector<std::string> &InWarnings) {
  std::cout << "Phase list topic=" << InTopicKey << " status=" << InStatusFilter
            << " count=" << InItems.size() << " plan=" << InPlanPath << "\n";
  for (const PhaseItem &Item : InItems) {
    std::cout << "- " << Item.mPhaseKey << " status=" << Item.mStatus;
    if (!Item.mStatusRaw.empty()) {
      std::cout << " raw=" << Item.mStatusRaw;
    }
    if (!Item.mDescription.empty()) {
      std::cout << " desc=\"" << Item.mDescription << "\"";
    }
    if (!Item.mNextAction.empty()) {
      std::cout << " next_action=\"" << Item.mNextAction << "\"";
    }
    if (!Item.mPlaybookPath.empty()) {
      std::cout << " playbook=" << Item.mPlaybookPath;
    }
    std::cout << " table=" << Item.mTableId << " row=" << Item.mRowIndex
              << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunBlockersText(const std::string &InStatusFilter,
                    const std::vector<BlockerItem> &InItems,
                    const std::vector<std::string> &InWarnings) {
  std::cout << "Blockers status=" << InStatusFilter
            << " count=" << InItems.size() << "\n";
  for (const BlockerItem &Item : InItems) {
    std::cout << "- " << Item.mTopicKey << " [" << Item.mKind << "/"
              << Item.mStatus << "]";
    if (!Item.mPhaseKey.empty()) {
      std::cout << " phase=" << Item.mPhaseKey;
    }
    if (!Item.mPriority.empty()) {
      std::cout << " priority=" << Item.mPriority;
    }
    std::cout << " action=" << Item.mAction;
    if (!Item.mNotes.empty()) {
      std::cout << " | notes: " << Item.mNotes;
    }
    std::cout << " | source: " << Item.mSourcePath << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunCacheInfoText(const CacheInfoResult &InResult) {
  std::cout << "Cache directory: " << InResult.mCacheDir << "\n";
  std::cout << "Exists: " << (InResult.mbCacheExists ? "yes" : "no") << "\n";
  if (InResult.mbCacheExists) {
    std::cout << "Size: " << FormatBytesHuman(InResult.mCacheSizeBytes) << " ("
              << InResult.mCacheSizeBytes << " bytes)\n";
    std::cout << "Entries: " << InResult.mCacheEntryCount << "\n";
  }
  std::cout << "Current repo cache: " << InResult.mCurrentRepoCachePath << "\n";
  std::cout << "Current repo cached: "
            << (InResult.mbCurrentRepoCacheExists ? "yes" : "no") << "\n";
  std::cout << "Config dir override: "
            << (InResult.mConfigCacheDir.empty() ? "(default)"
                                                 : InResult.mConfigCacheDir)
            << "\n";
  std::cout << "Cache enabled: " << (InResult.mbCacheEnabled ? "true" : "false")
            << "\n";
  std::cout << "Cache verbose: " << (InResult.mbCacheVerbose ? "true" : "false")
            << "\n";
  std::cout << "INI path: " << InResult.mIniPath << "\n";
  PrintTextWarnings(InResult.mWarnings);
  return 0;
}

int RunCacheClearText(const CacheClearResult &InResult) {
  if (!InResult.mbSuccess) {
    std::cerr << "Error: " << InResult.mError << "\n";
    return 1;
  }
  if (InResult.mEntriesRemoved == 0 && InResult.mBytesFreed == 0) {
    std::cout << "Cache is already empty.\n";
  } else {
    std::cout << "Cleared " << InResult.mEntriesRemoved << " entries, freed "
              << FormatBytesHuman(InResult.mBytesFreed) << ".\n";
  }
  PrintTextWarnings(InResult.mWarnings);
  return InResult.mbSuccess ? 0 : 1;
}

int RunCacheConfigText(const CacheConfigResult &InResult) {
  if (!InResult.mbSuccess) {
    std::cerr << "Error: " << InResult.mError << "\n";
    return 1;
  }
  std::cout << "Configuration written to: " << InResult.mIniPath << "\n";
  std::cout << "Effective config:\n";
  std::cout << "  dir     = "
            << (InResult.mDir.empty() ? "(default)" : InResult.mDir) << "\n";
  std::cout << "  enabled = " << (InResult.mbEnabled ? "true" : "false")
            << "\n";
  std::cout << "  verbose = " << (InResult.mbVerbose ? "true" : "false")
            << "\n";
  PrintTextWarnings(InResult.mWarnings);
  return InResult.mbSuccess ? 0 : 1;
}

int RunPhaseListAllText(const std::string &InStatusFilter,
                        const std::vector<PhaseListAllEntry> &InEntries,
                        const std::vector<std::string> &InWarnings) {
  size_t TotalPhases = 0;
  for (const PhaseListAllEntry &Entry : InEntries) {
    TotalPhases += Entry.mPhases.size();
  }
  std::cout << "Phase list (all) status=" << InStatusFilter
            << " plans=" << InEntries.size() << " phases=" << TotalPhases
            << "\n";

  for (size_t PlanIndex = 0; PlanIndex < InEntries.size(); ++PlanIndex) {
    const PhaseListAllEntry &Entry = InEntries[PlanIndex];
    if (PlanIndex > 0) {
      std::cout << "\n";
    }
    std::cout << Entry.mTopicKey << " status=" << Entry.mPlanStatus
              << " plan=" << Entry.mPlanPath
              << " phases=" << Entry.mPhases.size() << "\n";
    for (const PhaseItem &Item : Entry.mPhases) {
      std::cout << "- " << Item.mPhaseKey << " status=" << Item.mStatus;
      if (!Item.mDescription.empty()) {
        std::cout << " desc=\"" << Item.mDescription << "\"";
      }
      std::cout << "\n";
    }
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

int RunSectionContentText(const std::string &InDocPath,
                          const SectionResolution &InResolution,
                          const int InLineCharLimit,
                          const int InContentStartLine,
                          const std::vector<std::string> &InContentLines,
                          const std::vector<std::string> &InWarnings) {
  std::cout << "Section content " << InDocPath << " ["
            << InResolution.mSectionId << "]\n";
  for (size_t LineIndex = 0; LineIndex < InContentLines.size(); ++LineIndex) {
    const int LineNumber = InContentStartLine + static_cast<int>(LineIndex);
    std::string Text = InContentLines[LineIndex];
    if (InLineCharLimit > 0) {
      Text = TruncateForDisplay(Text, static_cast<size_t>(InLineCharLimit));
    }
    std::cout << std::setw(6) << LineNumber << " | " << Text << "\n";
  }
  PrintTextWarnings(InWarnings);
  return 0;
}

} // namespace UniPlan
