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

LintResult BuildLintResult(const std::string &InRepoRoot, const bool InQuiet) {
  const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);
  LintResult Result;
  Result.mGeneratedUtc = GetUtcNow();
  Result.mRepoRoot = ToGenericPath(RepoRoot);

  const std::vector<MarkdownDocument> Docs =
      EnumerateMarkdownDocuments(RepoRoot, Result.mWarnings);
  if (!InQuiet) {
    PrintScanInfo(Docs.size());
  }
  for (const MarkdownDocument &Doc : Docs) {
    const std::string Name = Doc.mAbsolutePath.filename().string();
    if (!IsAllowedLintFilename(Name)) {
      AddWarning(Result.mWarnings, "WARN name pattern: " + Doc.mRelativePath);
      Result.mNamePatternWarningCount += 1;
    }

    std::string H1Error;
    const bool HasH1 = HasFirstNonEmptyLineH1(Doc.mAbsolutePath, H1Error);
    if (!H1Error.empty()) {
      AddWarning(Result.mWarnings, "WARN read failure: " + Doc.mRelativePath +
                                       " (" + H1Error + ")");
      continue;
    }
    if (!HasH1) {
      AddWarning(Result.mWarnings, "WARN missing H1: " + Doc.mRelativePath);
      Result.mMissingH1WarningCount += 1;
    }
  }

  NormalizeWarnings(Result.mWarnings);
  Result.mWarningCount = static_cast<int>(Result.mWarnings.size());
  return Result;
}

InventoryResult BuildDocInventoryResult(const std::string &InRepoRoot) {
  const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);
  InventoryResult Result;
  Result.mGeneratedUtc = GetUtcNow();
  Result.mRepoRoot = ToGenericPath(RepoRoot);

  const std::vector<MarkdownDocument> Docs =
      EnumerateMarkdownDocuments(RepoRoot, Result.mWarnings);
  PrintScanInfo(Docs.size());
  for (const MarkdownDocument &Doc : Docs) {
    std::string CountError;
    const int LineCount = CountFileLines(Doc.mAbsolutePath, CountError);
    if (!CountError.empty()) {
      AddWarning(Result.mWarnings, "Line count failed for '" +
                                       Doc.mRelativePath + "': " + CountError);
      continue;
    }

    InventoryItem Item;
    Item.mPath = Doc.mRelativePath;
    Item.mLineCount = LineCount;
    Item.mLastCommit = GetLastCommitDate(RepoRoot, Doc.mRelativePath);
    Result.mItems.push_back(std::move(Item));
  }

  NormalizeWarnings(Result.mWarnings);
  return Result;
}

OrphanCheckResult BuildOrphanCheckResult(const std::string &InRepoRoot) {
  const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);
  OrphanCheckResult Result;
  Result.mGeneratedUtc = GetUtcNow();
  Result.mRepoRoot = ToGenericPath(RepoRoot);
  Result.mIgnoredRoots = {"README.md", "AGENTS.md"};

  const std::vector<MarkdownDocument> Docs =
      EnumerateMarkdownDocuments(RepoRoot, Result.mWarnings);
  PrintScanInfo(Docs.size());
  std::set<std::string> DocPathSet;
  std::map<std::string, int> Incoming;
  for (const MarkdownDocument &Doc : Docs) {
    DocPathSet.insert(Doc.mRelativePath);
    Incoming[Doc.mRelativePath] = 0;
  }

  std::set<std::string> Edges;
  for (const MarkdownDocument &Doc : Docs) {
    std::string Text;
    std::string ReadError;
    if (!TryReadFileText(Doc.mAbsolutePath, Text, ReadError)) {
      AddWarning(Result.mWarnings, "Reference scan failed for '" +
                                       Doc.mRelativePath + "': " + ReadError);
      continue;
    }

    const fs::path DocDirectory = Doc.mAbsolutePath.parent_path();
    for (std::sregex_iterator
             MatchIt(Text.begin(), Text.end(), kMarkdownPathRegex),
         EndIt;
         MatchIt != EndIt; ++MatchIt) {
      std::string Raw = MatchIt->str();
      std::replace(Raw.begin(), Raw.end(), '\\', '/');

      fs::path CandidateAbsolute;
      bool Found = false;

      const fs::path RootCandidate = RepoRoot / fs::path(Raw);
      std::error_code Error;
      if (fs::exists(RootCandidate, Error) &&
          fs::is_regular_file(RootCandidate, Error)) {
        CandidateAbsolute = fs::weakly_canonical(RootCandidate, Error);
        Found = !Error;
      }

      if (!Found) {
        const fs::path LocalCandidate = DocDirectory / fs::path(Raw);
        Error.clear();
        if (fs::exists(LocalCandidate, Error) &&
            fs::is_regular_file(LocalCandidate, Error)) {
          CandidateAbsolute = fs::weakly_canonical(LocalCandidate, Error);
          Found = !Error;
        }
      }

      if (!Found || !IsPathWithinRoot(CandidateAbsolute, RepoRoot)) {
        continue;
      }

      fs::path RelativePath;
      try {
        RelativePath = fs::relative(CandidateAbsolute, RepoRoot);
      } catch (const fs::filesystem_error &) {
        continue;
      }

      const std::string Relative = ToGenericPath(RelativePath);
      if (DocPathSet.count(Relative) == 0) {
        continue;
      }
      Edges.insert(Doc.mRelativePath + "|" + Relative);
    }
  }

  for (const std::string &Edge : Edges) {
    const size_t Separator = Edge.find('|');
    if (Separator == std::string::npos || Separator + 1 >= Edge.size()) {
      continue;
    }
    const std::string Target = Edge.substr(Separator + 1);
    auto IncomingIt = Incoming.find(Target);
    if (IncomingIt != Incoming.end()) {
      IncomingIt->second += 1;
    }
  }

  for (const MarkdownDocument &Doc : Docs) {
    if (Doc.mRelativePath == "README.md" || Doc.mRelativePath == "AGENTS.md") {
      continue;
    }
    const auto IncomingIt = Incoming.find(Doc.mRelativePath);
    if (IncomingIt != Incoming.end() && IncomingIt->second == 0) {
      Result.mOrphans.push_back(Doc.mRelativePath);
    }
  }

  std::sort(Result.mOrphans.begin(), Result.mOrphans.end());
  NormalizeWarnings(Result.mWarnings);
  return Result;
}

// Validation result structs (ActivePhaseRecord through HeadingAliasResult)
// moved to DocTypes.h

bool IsSnakeCaseHeadingLiteral(const std::string &InHeadingText) {
  static const std::regex SnakeCaseRegex(R"(^[a-z0-9_]+$)");
  return std::regex_match(Trim(InHeadingText), SnakeCaseRegex);
}

PlanSchemaValidationResult
EvaluatePlanSchemaConformance(const fs::path &InRepoRoot,
                              const std::vector<DocumentRecord> &InPlans,
                              std::vector<std::string> &OutWarnings) {
  PlanSchemaValidationResult Result;
  const std::vector<SectionSchemaEntry> SchemaEntries =
      BuildSectionSchemaEntries("plan", InRepoRoot);
  std::vector<std::string> RequiredSectionIds;
  std::vector<std::string> CanonicalSectionOrder;
  for (const SectionSchemaEntry &Entry : SchemaEntries) {
    CanonicalSectionOrder.push_back(Entry.mSectionId);
    if (Entry.mbRequired) {
      RequiredSectionIds.push_back(Entry.mSectionId);
    }
  }

  Result.mPlanCount = static_cast<int>(InPlans.size());
  for (const DocumentRecord &Plan : InPlans) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Plan.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      Result.mReadFailureCount += 1;
      AddWarning(OutWarnings, "Plan-schema validation parse skipped for '" +
                                  Plan.mPath + "': " + ReadError);
      continue;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    std::map<std::string, std::vector<const HeadingRecord *>> H2HeadingsById;
    for (const HeadingRecord &Heading : Headings) {
      if (Heading.mLevel != 2) {
        continue;
      }
      H2HeadingsById[Heading.mSectionId].push_back(&Heading);
    }

    std::vector<std::string> MissingRequired;
    for (const std::string &Required : RequiredSectionIds) {
      if (H2HeadingsById.count(Required) == 0) {
        MissingRequired.push_back(Required);
      }
    }
    if (!MissingRequired.empty()) {
      Result.mMissingRequiredPlanCount += 1;
      Result.mMissingRequiredDiagnostics.push_back(
          "plan=" + Plan.mPath +
          " missing_required_sections=" + JoinCommaSeparated(MissingRequired));
    }

    std::vector<std::string> LiteralMismatches;
    for (const std::string &Required : RequiredSectionIds) {
      const auto Found = H2HeadingsById.find(Required);
      if (Found == H2HeadingsById.end() || Found->second.empty()) {
        continue;
      }
      const HeadingRecord *Heading = Found->second.front();
      if (Heading->mText == Required) {
        continue;
      }
      LiteralMismatches.push_back(Required + "->" + Heading->mText + "@L" +
                                  std::to_string(Heading->mLine));
    }
    if (!LiteralMismatches.empty()) {
      Result.mLiteralMismatchPlanCount += 1;
      Result.mLiteralMismatchDiagnostics.push_back(
          "plan=" + Plan.mPath +
          " literal_heading_mismatch=" + JoinCommaSeparated(LiteralMismatches));
    }

    std::map<std::string, int> FirstCanonicalLineBySectionId;
    for (const auto &Entry : H2HeadingsById) {
      if (Entry.second.empty()) {
        continue;
      }
      FirstCanonicalLineBySectionId[Entry.first] = Entry.second.front()->mLine;
    }

    std::vector<std::string> OrderDrifts;
    std::string PreviousSectionId;
    int PreviousLine = 0;
    for (const std::string &CanonicalSectionId : CanonicalSectionOrder) {
      const auto FoundLine =
          FirstCanonicalLineBySectionId.find(CanonicalSectionId);
      if (FoundLine == FirstCanonicalLineBySectionId.end()) {
        continue;
      }
      if (!PreviousSectionId.empty() && FoundLine->second < PreviousLine) {
        OrderDrifts.push_back(
            PreviousSectionId + "@L" + std::to_string(PreviousLine) + ">" +
            CanonicalSectionId + "@L" + std::to_string(FoundLine->second));
      }
      PreviousSectionId = CanonicalSectionId;
      PreviousLine = FoundLine->second;
    }
    if (!OrderDrifts.empty()) {
      Result.mOrderDriftPlanCount += 1;
      Result.mOrderDriftDiagnostics.push_back(
          "plan=" + Plan.mPath +
          " canonical_order_drift=" + JoinCommaSeparated(OrderDrifts));
    }

    bool bPlanHasHeadingNamingDrift = false;
    bool bPlanHasIndexedHeadingPrefix = false;
    for (const HeadingRecord &Heading : Headings) {
      if (Heading.mLevel < 2 || Heading.mLevel > 6) {
        continue;
      }
      Result.mHeadingCheckedCount += 1;

      const std::string ExpectedId = NormalizeSectionId(Heading.mText);
      if (!IsSnakeCaseHeadingLiteral(Heading.mText)) {
        Result.mHeadingNonCompliantCount += 1;
        bPlanHasHeadingNamingDrift = true;
        Result.mHeadingNamingDiagnostics.push_back(
            "plan=" + Plan.mPath + " line=" + std::to_string(Heading.mLine) +
            " level=H" + std::to_string(Heading.mLevel) +
            " heading=" + Heading.mText + " expected=" + ExpectedId);
      }

      if (HasIndexedHeadingPrefix(Heading.mText)) {
        Result.mHeadingIndexedPrefixCount += 1;
        bPlanHasIndexedHeadingPrefix = true;
        Result.mHeadingIndexedPrefixDiagnostics.push_back(
            "plan=" + Plan.mPath + " line=" + std::to_string(Heading.mLine) +
            " level=H" + std::to_string(Heading.mLevel) +
            " heading=" + Heading.mText + " expected=" + ExpectedId);
      }
    }
    if (bPlanHasHeadingNamingDrift) {
      Result.mHeadingNamingDriftPlanCount += 1;
    }
    if (bPlanHasIndexedHeadingPrefix) {
      Result.mHeadingIndexedPrefixPlanCount += 1;
    }
  }

  return Result;
}

BlankSectionsResult
EvaluateBlankSections(const fs::path &InRepoRoot,
                      const std::vector<DocumentRecord> &InPlans,
                      std::vector<std::string> &OutWarnings) {
  BlankSectionsResult Result;
  static const std::vector<std::string> RequiredSectionIds = {
      "section_menu",        "summary",
      "execution_strategy",  "risks_and_mitigations",
      "acceptance_criteria", "next_actions"};

  Result.mPlanCount = static_cast<int>(InPlans.size());
  for (const DocumentRecord &Plan : InPlans) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Plan.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      Result.mReadFailureCount += 1;
      AddWarning(OutWarnings, "Blank-section check skipped for '" + Plan.mPath +
                                  "': " + ReadError);
      continue;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    std::vector<std::string> BlankSections;
    for (const std::string &RequiredId : RequiredSectionIds) {
      int SectionStart = -1;
      int SectionEnd = static_cast<int>(Lines.size());
      for (size_t HeadingIndex = 0; HeadingIndex < Headings.size();
           ++HeadingIndex) {
        if (Headings[HeadingIndex].mLevel == 2 &&
            Headings[HeadingIndex].mSectionId == RequiredId) {
          SectionStart = Headings[HeadingIndex].mLine;
          for (size_t NextIndex = HeadingIndex + 1; NextIndex < Headings.size();
               ++NextIndex) {
            if (Headings[NextIndex].mLevel <= 2) {
              SectionEnd = Headings[NextIndex].mLine;
              break;
            }
          }
          break;
        }
      }
      if (SectionStart < 0) {
        continue;
      }
      bool bHasContent = false;
      for (int LineIndex = SectionStart + 1;
           LineIndex < SectionEnd && LineIndex < static_cast<int>(Lines.size());
           ++LineIndex) {
        if (!Trim(Lines[static_cast<size_t>(LineIndex)]).empty()) {
          bHasContent = true;
          break;
        }
      }
      if (!bHasContent) {
        BlankSections.push_back(RequiredId);
      }
    }
    if (!BlankSections.empty()) {
      Result.mBlankSectionPlanCount += 1;
      Result.mDiagnostics.push_back("plan=" + Plan.mPath + " blank_sections=" +
                                    JoinCommaSeparated(BlankSections));
    }
  }

  return Result;
}

CrossStatusResult
EvaluateCrossStatus(const fs::path &InRepoRoot,
                    const std::vector<TopicPairRecord> &InPairs,
                    std::vector<std::string> &OutWarnings) {
  // Phase status is now single-sourced from playbook execution_lanes.
  // Plan and impl documents no longer carry Status columns.
  // This check is retained as a pass-through — no cross-status comparison
  // needed.
  CrossStatusResult Result;
  Result.mTopicCount = static_cast<int>(InPairs.size());
  return Result;
}

LinkIntegrityResult
EvaluateLinkIntegrity(const fs::path &InRepoRoot,
                      const std::vector<DocumentRecord> &InPlans,
                      const std::vector<DocumentRecord> &InPlaybooks,
                      const std::vector<DocumentRecord> &InImplementations,
                      std::vector<std::string> &OutWarnings) {
  LinkIntegrityResult Result;
  static const std::regex LinkPattern(R"(\[([^\]]*)\]\(([^)]+)\))");

  std::vector<const DocumentRecord *> AllDocs;
  for (const DocumentRecord &Doc : InPlans) {
    AllDocs.push_back(&Doc);
  }
  for (const DocumentRecord &Doc : InPlaybooks) {
    AllDocs.push_back(&Doc);
  }
  for (const DocumentRecord &Doc : InImplementations) {
    AllDocs.push_back(&Doc);
  }

  for (const DocumentRecord *rpDoc : AllDocs) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(rpDoc->mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      continue;
    }

    Result.mDocCount += 1;
    const fs::path ParentDir = AbsolutePath.parent_path();
    bool bInsideCodeFence = false;
    std::vector<std::string> BrokenLinks;

    for (const std::string &Line : Lines) {
      const std::string Trimmed = Trim(Line);
      if (Trimmed.size() >= 3 &&
          (Trimmed.substr(0, 3) == "```" || Trimmed.substr(0, 3) == "~~~")) {
        bInsideCodeFence = !bInsideCodeFence;
        continue;
      }
      if (bInsideCodeFence) {
        continue;
      }

      auto MatchBegin =
          std::sregex_iterator(Line.begin(), Line.end(), LinkPattern);
      auto MatchEnd = std::sregex_iterator();
      for (auto Iterator = MatchBegin; Iterator != MatchEnd; ++Iterator) {
        const std::string Target = (*Iterator)[2].str();
        if (Target.empty() || Target[0] == '#') {
          continue;
        }
        if (Target.find("http://") == 0 || Target.find("https://") == 0) {
          continue;
        }
        std::string FilePath = Target;
        const auto AnchorPos = FilePath.find('#');
        if (AnchorPos != std::string::npos) {
          FilePath = FilePath.substr(0, AnchorPos);
        }
        if (FilePath.empty()) {
          continue;
        }
        const fs::path ResolvedPath =
            fs::weakly_canonical(ParentDir / fs::path(FilePath));
        if (!fs::exists(ResolvedPath)) {
          BrokenLinks.push_back(FilePath);
        }
      }
    }

    if (!BrokenLinks.empty()) {
      Result.mBrokenLinkCount += static_cast<int>(BrokenLinks.size());
      for (const std::string &Link : BrokenLinks) {
        Result.mDiagnostics.push_back("source=" + rpDoc->mPath +
                                      " broken_link=" + Link);
      }
    }
  }

  return Result;
}

TaxonomyJobCompletenessResult
EvaluateTaxonomyJobCompleteness(const fs::path &InRepoRoot,
                                const std::vector<DocumentRecord> &InPlaybooks,
                                std::vector<std::string> &OutWarnings) {
  TaxonomyJobCompletenessResult Result;
  for (const DocumentRecord &Playbook : InPlaybooks) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Playbook.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      continue;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);

    bool bHasBoard = false;
    for (const MarkdownTableRecord &Table : Tables) {
      if (Table.mSectionId != "wave_lane_job_board") {
        continue;
      }
      bHasBoard = true;
      int WaveCol = -1;
      int LaneCol = -1;
      int JobCol = -1;
      for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size()); ++Col) {
        const std::string Lower =
            ToLower(Trim(Table.mHeaders[static_cast<size_t>(Col)]));
        if (Lower == "wave" || Lower == "wave_id") {
          WaveCol = Col;
        }
        if (Lower == "lane" || Lower == "lane_id") {
          LaneCol = Col;
        }
        if (Lower == "job" || Lower == "job_id") {
          JobCol = Col;
        }
      }
      if (WaveCol < 0 || LaneCol < 0 || JobCol < 0) {
        continue;
      }
      int IncompleteCount = 0;
      for (const std::vector<std::string> &Row : Table.mRows) {
        const std::string Wave = (static_cast<int>(Row.size()) > WaveCol)
                                     ? Trim(Row[static_cast<size_t>(WaveCol)])
                                     : "";
        const std::string Lane = (static_cast<int>(Row.size()) > LaneCol)
                                     ? Trim(Row[static_cast<size_t>(LaneCol)])
                                     : "";
        if (Wave.empty() || Lane.empty()) {
          IncompleteCount += 1;
        }
      }
      if (IncompleteCount > 0) {
        Result.mIncompleteJobCount += IncompleteCount;
        Result.mDiagnostics.push_back(
            "playbook=" + Playbook.mPath +
            " jobs_missing_wave_or_lane=" + std::to_string(IncompleteCount));
      }
    }
    if (bHasBoard) {
      Result.mPlaybookCount += 1;
    }
  }

  return Result;
}

TaxonomyTaskTraceabilityResult
EvaluateTaxonomyTaskTraceability(const fs::path &InRepoRoot,
                                 const std::vector<DocumentRecord> &InPlaybooks,
                                 std::vector<std::string> &OutWarnings) {
  TaxonomyTaskTraceabilityResult Result;
  for (const DocumentRecord &Playbook : InPlaybooks) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Playbook.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      continue;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);

    bool bHasChecklist = false;
    for (const MarkdownTableRecord &Table : Tables) {
      if (Table.mSectionId != "job_task_checklist") {
        continue;
      }
      bHasChecklist = true;
      int JobCol = -1;
      for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size()); ++Col) {
        const std::string Lower =
            ToLower(Trim(Table.mHeaders[static_cast<size_t>(Col)]));
        if (Lower == "job" || Lower == "job_id" || Lower == "parent_job") {
          JobCol = Col;
        }
      }
      if (JobCol < 0) {
        Result.mUntraceableTaskCount += static_cast<int>(Table.mRows.size());
        Result.mDiagnostics.push_back("playbook=" + Playbook.mPath +
                                      " missing_job_column=true tasks=" +
                                      std::to_string(Table.mRows.size()));
        continue;
      }
      int UntraceableCount = 0;
      for (const std::vector<std::string> &Row : Table.mRows) {
        const std::string JobRef = (static_cast<int>(Row.size()) > JobCol)
                                       ? Trim(Row[static_cast<size_t>(JobCol)])
                                       : "";
        if (JobRef.empty()) {
          UntraceableCount += 1;
        }
      }
      if (UntraceableCount > 0) {
        Result.mUntraceableTaskCount += UntraceableCount;
        Result.mDiagnostics.push_back(
            "playbook=" + Playbook.mPath +
            " untraceable_tasks=" + std::to_string(UntraceableCount));
      }
    }
    if (bHasChecklist) {
      Result.mPlaybookCount += 1;
    }
  }

  return Result;
}

ValidationHeadingOwnershipResult EvaluateValidationHeadingOwnership(
    const fs::path &InRepoRoot, const std::vector<DocumentRecord> &InPlans,
    const std::vector<DocumentRecord> &InImplementations,
    std::vector<std::string> &OutWarnings) {
  ValidationHeadingOwnershipResult Result;

  for (const DocumentRecord &Plan : InPlans) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Plan.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      continue;
    }
    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    for (const HeadingRecord &Heading : Headings) {
      if (Heading.mLevel == 2 && Heading.mSectionId == "verification") {
        Result.mPlanViolationCount += 1;
        Result.mDiagnostics.push_back(
            "plan=" + Plan.mPath +
            " has_verification_heading=true (should use validation_commands)");
        break;
      }
    }
  }

  for (const DocumentRecord &Impl : InImplementations) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Impl.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      continue;
    }
    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    for (const HeadingRecord &Heading : Headings) {
      if (Heading.mLevel == 2 && Heading.mSectionId == "validation_commands") {
        Result.mImplViolationCount += 1;
        Result.mDiagnostics.push_back(
            "implementation=" + Impl.mPath +
            " has_validation_commands_heading=true (should use verification)");
        break;
      }
    }
  }

  return Result;
}

TestingActorCoverageResult
EvaluateTestingActorCoverage(const fs::path &InRepoRoot,
                             const std::vector<DocumentRecord> &InPlaybooks,
                             std::vector<std::string> &OutWarnings) {
  TestingActorCoverageResult Result;
  for (const DocumentRecord &Playbook : InPlaybooks) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Playbook.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      continue;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);

    for (const MarkdownTableRecord &Table : Tables) {
      if (Table.mSectionId != "testing") {
        continue;
      }
      Result.mPlaybookCount += 1;
      int ActorCol = -1;
      for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size()); ++Col) {
        const std::string Lower =
            ToLower(Trim(Table.mHeaders[static_cast<size_t>(Col)]));
        if (Lower == "actor") {
          ActorCol = Col;
        }
      }
      if (ActorCol < 0) {
        Result.mMissingActorPlaybookCount += 1;
        Result.mDiagnostics.push_back("playbook=" + Playbook.mPath +
                                      " missing_actor_column=true");
        continue;
      }
      bool bHasHuman = false;
      bool bHasAiAgent = false;
      bool bHasBoth = false;
      for (const std::vector<std::string> &Row : Table.mRows) {
        const std::string Actor =
            (static_cast<int>(Row.size()) > ActorCol)
                ? ToLower(Trim(Row[static_cast<size_t>(ActorCol)]))
                : "";
        const std::string CleanActor =
            (Actor.size() >= 2 && Actor.front() == '`' && Actor.back() == '`')
                ? Actor.substr(1, Actor.size() - 2)
                : Actor;
        if (CleanActor == "human") {
          bHasHuman = true;
        } else if (CleanActor == "ai_agent" || CleanActor == "ai agent") {
          bHasAiAgent = true;
        } else if (CleanActor == "both") {
          bHasBoth = true;
        }
      }
      if (!bHasBoth && (!bHasHuman || !bHasAiAgent)) {
        Result.mMissingActorPlaybookCount += 1;
        std::string Missing;
        if (!bHasHuman && !bHasBoth) {
          Missing = "human";
        }
        if (!bHasAiAgent && !bHasBoth) {
          Missing += Missing.empty() ? "ai_agent" : ",ai_agent";
        }
        Result.mDiagnostics.push_back("playbook=" + Playbook.mPath +
                                      " missing_actors=" + Missing);
      }
      break;
    }
  }

  return Result;
}

HeadingAliasResult EvaluateHeadingAliasNormalization(
    const fs::path &InRepoRoot, const std::vector<DocumentRecord> &InPlans,
    const std::vector<DocumentRecord> &InPlaybooks,
    const std::vector<DocumentRecord> &InImplementations,
    std::vector<std::string> &OutWarnings) {
  HeadingAliasResult Result;

  static const std::vector<std::pair<std::string, std::string>> AliasMap = {
      {"code_entity_drafts", "code_entity_draft_contract"},
      {"code_entity_draft", "code_entity_draft_contract"},
      {"entity_draft_contracts", "code_entity_draft_contract"},
      {"best_practice_investigation", "internet_best_practice_investigation"},
      {"internet_backed_best_practice_investigation",
       "internet_best_practice_investigation"},
      {"exit_criteria", "handoff_points"},
      {"boundary_and_handoff_contract", "handoff_points"},
      {"phase_goal", "phase_binding"},
      {"current_state", "baseline_audit"},
      {"current_repository_baseline_and_gap", "baseline_audit"},
      {"phase_status", "phase_tracking"},
      {"step_tracking", "phase_tracking"},
      {"linked_playbooks", "linked_playbook"},
  };

  std::map<std::string, std::string> AliasLookup;
  for (const auto &Pair : AliasMap) {
    AliasLookup[Pair.first] = Pair.second;
  }

  const auto ScanDocs = [&](const std::vector<DocumentRecord> &InDocs) {
    for (const DocumentRecord &Doc : InDocs) {
      const fs::path AbsolutePath = InRepoRoot / fs::path(Doc.mPath);
      std::vector<std::string> Lines;
      std::string ReadError;
      if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
        AddWarning(OutWarnings, "Heading-alias check skipped for '" +
                                    Doc.mPath + "': " + ReadError);
        continue;
      }

      Result.mDocCount += 1;
      const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
      bool bDocHasAlias = false;
      for (const HeadingRecord &Heading : Headings) {
        if (Heading.mLevel == 2) {
          const auto It = AliasLookup.find(Heading.mSectionId);
          if (It != AliasLookup.end()) {
            Result.mAliasHeadingCount += 1;
            bDocHasAlias = true;
            Result.mDiagnostics.push_back("doc=" + Doc.mPath +
                                          " alias=" + Heading.mSectionId +
                                          " canonical=" + It->second);
          }
        }
      }
      if (bDocHasAlias) {
        Result.mAliasDocCount += 1;
      }
    }
  };

  ScanDocs(InPlans);
  ScanDocs(InPlaybooks);
  ScanDocs(InImplementations);

  return Result;
}

std::vector<ValidateCheck>
BuildValidateChecks(const Inventory &InInventory, const fs::path &InRepoRoot,
                    const bool InStrict, std::vector<std::string> &OutErrors,
                    std::vector<std::string> &OutWarnings, bool &OutOk) {
  int MissingImplementationCount = 0;
  int OrphanImplementationCount = 0;
  int MissingPhasePlaybookCount = 0;
  for (const TopicPairRecord &Pair : InInventory.mPairs) {
    if (Pair.mPairState == "missing_implementation") {
      MissingImplementationCount += 1;
    } else if (Pair.mPairState == "orphan_implementation") {
      OrphanImplementationCount += 1;
    } else if (Pair.mPairState == "missing_phase_playbook") {
      MissingPhasePlaybookCount += 1;
    }
  }

  int UnknownStatusCount = 0;
  const auto CountUnknownStatuses =
      [&UnknownStatusCount](const std::vector<DocumentRecord> &InRecords) {
        for (const DocumentRecord &Record : InRecords) {
          if (Record.mStatus.empty() || Record.mStatus == "unknown") {
            UnknownStatusCount += 1;
          }
        }
      };
  CountUnknownStatuses(InInventory.mPlans);
  CountUnknownStatuses(InInventory.mPlaybooks);
  CountUnknownStatuses(InInventory.mImplementations);

  int DuplicateWarningCount = 0;
  int OrphanSidecarWarningCount = 0;
  for (const std::string &Warning : InInventory.mWarnings) {
    if (Warning.find("Duplicate ") != std::string::npos) {
      DuplicateWarningCount += 1;
    }
    if (Warning.find("Orphan ") != std::string::npos &&
        Warning.find("sidecar") != std::string::npos) {
      OrphanSidecarWarningCount += 1;
    }
  }

  std::vector<std::string> GovernanceWarnings;
  const PhaseEntryGateResult PhaseEntryGate =
      EvaluatePhaseEntryGate(InRepoRoot, InInventory.mPlans,
                             InInventory.mPlaybooks, GovernanceWarnings);
  const ArtifactRoleBoundaryResult RoleBoundary =
      EvaluateArtifactRoleBoundaries(InRepoRoot, InInventory.mPlaybooks,
                                     InInventory.mImplementations,
                                     GovernanceWarnings);
  std::vector<std::string> PlanSchemaWarnings;
  const PlanSchemaValidationResult PlanSchema = EvaluatePlanSchemaConformance(
      InRepoRoot, InInventory.mPlans, PlanSchemaWarnings);
  std::vector<std::string> RuleWarnings;
  const std::vector<RuleEntry> Rules = BuildRules(InRepoRoot, RuleWarnings);
  int UnresolvedRuleCount = 0;
  for (const RuleEntry &Rule : Rules) {
    if (!Rule.mbSourceResolved) {
      UnresolvedRuleCount += 1;
    }
  }

  std::vector<std::string> BlankSectionWarnings;
  const BlankSectionsResult BlankSections = EvaluateBlankSections(
      InRepoRoot, InInventory.mPlans, BlankSectionWarnings);
  std::vector<std::string> CrossStatusWarnings;
  const CrossStatusResult CrossStatus =
      EvaluateCrossStatus(InRepoRoot, InInventory.mPairs, CrossStatusWarnings);
  std::vector<std::string> PlaybookSchemaWarnings;
  const PlaybookSchemaResult PlaybookSchema = EvaluatePlaybookSchema(
      InRepoRoot, InInventory.mPlaybooks, PlaybookSchemaWarnings);
  std::vector<std::string> LinkIntegrityWarnings;
  const LinkIntegrityResult LinkIntegrity = EvaluateLinkIntegrity(
      InRepoRoot, InInventory.mPlans, InInventory.mPlaybooks,
      InInventory.mImplementations, LinkIntegrityWarnings);
  std::vector<std::string> TaxonomyJobWarnings;
  const TaxonomyJobCompletenessResult TaxonomyJob =
      EvaluateTaxonomyJobCompleteness(InRepoRoot, InInventory.mPlaybooks,
                                      TaxonomyJobWarnings);
  std::vector<std::string> TaxonomyTaskWarnings;
  const TaxonomyTaskTraceabilityResult TaxonomyTask =
      EvaluateTaxonomyTaskTraceability(InRepoRoot, InInventory.mPlaybooks,
                                       TaxonomyTaskWarnings);
  std::vector<std::string> HeadingOwnershipWarnings;
  const ValidationHeadingOwnershipResult HeadingOwnership =
      EvaluateValidationHeadingOwnership(InRepoRoot, InInventory.mPlans,
                                         InInventory.mImplementations,
                                         HeadingOwnershipWarnings);
  std::vector<std::string> TestingActorWarnings;
  const TestingActorCoverageResult TestingActor = EvaluateTestingActorCoverage(
      InRepoRoot, InInventory.mPlaybooks, TestingActorWarnings);
  std::vector<std::string> HeadingAliasWarnings;
  const HeadingAliasResult HeadingAlias = EvaluateHeadingAliasNormalization(
      InRepoRoot, InInventory.mPlans, InInventory.mPlaybooks,
      InInventory.mImplementations, HeadingAliasWarnings);

  std::vector<ValidateCheck> Checks;
  Checks.push_back(
      {"core_discovery",
       !InInventory.mPlans.empty() && !InInventory.mImplementations.empty(),
       true,
       "plans=" + std::to_string(InInventory.mPlans.size()) +
           ", implementations=" +
           std::to_string(InInventory.mImplementations.size()),
       ""});
  Checks.push_back(
      {"plan_impl_pairing",
       MissingImplementationCount == 0 && OrphanImplementationCount == 0, true,
       "missing_implementation=" + std::to_string(MissingImplementationCount) +
           ", orphan_implementation=" +
           std::to_string(OrphanImplementationCount),
       "plan_impl_pairing"});
  Checks.push_back(
      {"phase_playbook_pairing", MissingPhasePlaybookCount == 0, InStrict,
       "missing_phase_playbook=" + std::to_string(MissingPhasePlaybookCount),
       "active_phase_playbook_required"});
  Checks.push_back(
      {"phase_entry_gate_readiness",
       PhaseEntryGate.mMissingPlaybookCount == 0 &&
           PhaseEntryGate.mUnpreparedPlaybookCount == 0,
       InStrict,
       "active_phases=" + std::to_string(PhaseEntryGate.mActivePhaseCount) +
           ", missing_playbook=" +
           std::to_string(PhaseEntryGate.mMissingPlaybookCount) +
           ", unprepared_playbook=" +
           std::to_string(PhaseEntryGate.mUnpreparedPlaybookCount),
       "phase_entry_gate"});
  Checks.push_back(
      {"artifact_role_boundaries",
       RoleBoundary.mPlaybookViolationCount == 0 &&
           RoleBoundary.mImplementationViolationCount == 0,
       InStrict,
       "playbook_violations=" +
           std::to_string(RoleBoundary.mPlaybookViolationCount) +
           ", implementation_violations=" +
           std::to_string(RoleBoundary.mImplementationViolationCount),
       "artifact_role_boundary"});
  Checks.push_back(
      {"plan_required_sections",
       PlanSchema.mMissingRequiredPlanCount == 0 &&
           PlanSchema.mReadFailureCount == 0,
       InStrict,
       "plans_checked=" + std::to_string(PlanSchema.mPlanCount) +
           ", plans_with_missing_required_sections=" +
           std::to_string(PlanSchema.mMissingRequiredPlanCount) +
           ", read_failures=" + std::to_string(PlanSchema.mReadFailureCount),
       "plan_schema_required_sections",
       PlanSchema.mMissingRequiredDiagnostics});
  Checks.push_back(
      {"plan_canonical_section_order",
       PlanSchema.mOrderDriftPlanCount == 0 &&
           PlanSchema.mReadFailureCount == 0,
       InStrict,
       "plans_checked=" + std::to_string(PlanSchema.mPlanCount) +
           ", plans_with_order_drift=" +
           std::to_string(PlanSchema.mOrderDriftPlanCount) +
           ", read_failures=" + std::to_string(PlanSchema.mReadFailureCount),
       "plan_schema_canonical_order", PlanSchema.mOrderDriftDiagnostics});
  Checks.push_back(
      {"plan_required_heading_literal_mismatch",
       PlanSchema.mLiteralMismatchPlanCount == 0 &&
           PlanSchema.mReadFailureCount == 0,
       InStrict,
       "plans_checked=" + std::to_string(PlanSchema.mPlanCount) +
           ", plans_with_literal_mismatch=" +
           std::to_string(PlanSchema.mLiteralMismatchPlanCount) +
           ", read_failures=" + std::to_string(PlanSchema.mReadFailureCount),
       "plan_schema_literal_heading_ids",
       PlanSchema.mLiteralMismatchDiagnostics});
  Checks.push_back(
      {"plan_any_level_heading_snake_case",
       PlanSchema.mHeadingNonCompliantCount == 0 &&
           PlanSchema.mReadFailureCount == 0,
       InStrict,
       "plans_checked=" + std::to_string(PlanSchema.mPlanCount) +
           ", plans_with_heading_naming_drift=" +
           std::to_string(PlanSchema.mHeadingNamingDriftPlanCount) +
           ", headings_checked_h2_to_h6=" +
           std::to_string(PlanSchema.mHeadingCheckedCount) +
           ", headings_non_compliant=" +
           std::to_string(PlanSchema.mHeadingNonCompliantCount) +
           ", read_failures=" + std::to_string(PlanSchema.mReadFailureCount),
       "plan_schema_any_level_heading_snake_case",
       PlanSchema.mHeadingNamingDiagnostics});
  Checks.push_back(
      {"plan_any_level_heading_no_index_prefix",
       PlanSchema.mHeadingIndexedPrefixCount == 0 &&
           PlanSchema.mReadFailureCount == 0,
       InStrict,
       "plans_checked=" + std::to_string(PlanSchema.mPlanCount) +
           ", plans_with_indexed_heading_prefix=" +
           std::to_string(PlanSchema.mHeadingIndexedPrefixPlanCount) +
           ", indexed_headings=" +
           std::to_string(PlanSchema.mHeadingIndexedPrefixCount) +
           ", read_failures=" + std::to_string(PlanSchema.mReadFailureCount),
       "plan_schema_any_level_heading_no_index_prefix",
       PlanSchema.mHeadingIndexedPrefixDiagnostics});
  Checks.push_back({"status_parseability", UnknownStatusCount == 0, InStrict,
                    "unknown_status_docs=" + std::to_string(UnknownStatusCount),
                    ""});
  Checks.push_back(
      {"sidecar_integrity",
       DuplicateWarningCount == 0 && OrphanSidecarWarningCount == 0, true,
       "duplicate_sidecar_warnings=" + std::to_string(DuplicateWarningCount) +
           ", orphan_sidecar_warnings=" +
           std::to_string(OrphanSidecarWarningCount),
       "detached_evidence_sidecars"});
  Checks.push_back(
      {"governance_rule_provenance", UnresolvedRuleCount == 0, InStrict,
       "resolved_rules=" +
           std::to_string(static_cast<int>(Rules.size()) -
                          UnresolvedRuleCount) +
           ", unresolved_rules=" + std::to_string(UnresolvedRuleCount),
       ""});
  Checks.push_back(
      {"blank_sections",
       BlankSections.mBlankSectionPlanCount == 0 &&
           BlankSections.mReadFailureCount == 0,
       InStrict,
       "plans_checked=" + std::to_string(BlankSections.mPlanCount) +
           ", plans_with_blank_sections=" +
           std::to_string(BlankSections.mBlankSectionPlanCount) +
           ", read_failures=" + std::to_string(BlankSections.mReadFailureCount),
       "schema_blank_sections", BlankSections.mDiagnostics});
  Checks.push_back(
      {"cross_status", CrossStatus.mMismatchCount == 0, InStrict,
       "topics_checked=" + std::to_string(CrossStatus.mTopicCount) +
           ", status_mismatches=" + std::to_string(CrossStatus.mMismatchCount),
       "cross_doc_status_sync", CrossStatus.mDiagnostics});
  Checks.push_back(
      {"playbook_schema",
       PlaybookSchema.mMissingSectionPlaybookCount == 0 &&
           PlaybookSchema.mReadFailureCount == 0,
       InStrict,
       "playbooks_checked=" + std::to_string(PlaybookSchema.mPlaybookCount) +
           ", playbooks_with_missing_sections=" +
           std::to_string(PlaybookSchema.mMissingSectionPlaybookCount) +
           ", read_failures=" +
           std::to_string(PlaybookSchema.mReadFailureCount),
       "playbook_schema_required_sections", PlaybookSchema.mDiagnostics});
  Checks.push_back(
      {"link_integrity", LinkIntegrity.mBrokenLinkCount == 0, false,
       "docs_checked=" + std::to_string(LinkIntegrity.mDocCount) +
           ", broken_links=" + std::to_string(LinkIntegrity.mBrokenLinkCount),
       "link_integrity", LinkIntegrity.mDiagnostics});
  Checks.push_back(
      {"taxonomy_job_completeness", TaxonomyJob.mIncompleteJobCount == 0,
       InStrict,
       "playbooks_with_board=" + std::to_string(TaxonomyJob.mPlaybookCount) +
           ", jobs_missing_wave_or_lane=" +
           std::to_string(TaxonomyJob.mIncompleteJobCount),
       "pet_2_job_requires_wave_lane", TaxonomyJob.mDiagnostics});
  Checks.push_back({"taxonomy_task_traceability",
                    TaxonomyTask.mUntraceableTaskCount == 0, InStrict,
                    "playbooks_with_checklist=" +
                        std::to_string(TaxonomyTask.mPlaybookCount) +
                        ", untraceable_tasks=" +
                        std::to_string(TaxonomyTask.mUntraceableTaskCount),
                    "pet_5_task_traceability", TaxonomyTask.mDiagnostics});
  Checks.push_back({"validation_heading_ownership",
                    HeadingOwnership.mPlanViolationCount == 0 &&
                        HeadingOwnership.mImplViolationCount == 0,
                    InStrict,
                    "plan_violations=" +
                        std::to_string(HeadingOwnership.mPlanViolationCount) +
                        ", impl_violations=" +
                        std::to_string(HeadingOwnership.mImplViolationCount),
                    "pet_7_heading_ownership", HeadingOwnership.mDiagnostics});
  Checks.push_back(
      {"testing_actor_coverage", TestingActor.mMissingActorPlaybookCount == 0,
       InStrict,
       "playbooks_with_testing=" + std::to_string(TestingActor.mPlaybookCount) +
           ", missing_actor_coverage=" +
           std::to_string(TestingActor.mMissingActorPlaybookCount),
       "pet_8_testing_actors", TestingActor.mDiagnostics});
  Checks.push_back({"heading_alias_normalization",
                    HeadingAlias.mAliasHeadingCount == 0, InStrict,
                    "docs_checked=" + std::to_string(HeadingAlias.mDocCount) +
                        ", docs_with_aliases=" +
                        std::to_string(HeadingAlias.mAliasDocCount) +
                        ", alias_headings=" +
                        std::to_string(HeadingAlias.mAliasHeadingCount),
                    "heading_alias_normalization", HeadingAlias.mDiagnostics});

  OutWarnings = InInventory.mWarnings;
  OutWarnings.insert(OutWarnings.end(), GovernanceWarnings.begin(),
                     GovernanceWarnings.end());
  OutWarnings.insert(OutWarnings.end(), PlanSchemaWarnings.begin(),
                     PlanSchemaWarnings.end());
  OutWarnings.insert(OutWarnings.end(), RuleWarnings.begin(),
                     RuleWarnings.end());
  OutWarnings.insert(OutWarnings.end(), BlankSectionWarnings.begin(),
                     BlankSectionWarnings.end());
  OutWarnings.insert(OutWarnings.end(), CrossStatusWarnings.begin(),
                     CrossStatusWarnings.end());
  OutWarnings.insert(OutWarnings.end(), PlaybookSchemaWarnings.begin(),
                     PlaybookSchemaWarnings.end());
  OutWarnings.insert(OutWarnings.end(), LinkIntegrityWarnings.begin(),
                     LinkIntegrityWarnings.end());
  OutWarnings.insert(OutWarnings.end(), TaxonomyJobWarnings.begin(),
                     TaxonomyJobWarnings.end());
  OutWarnings.insert(OutWarnings.end(), TaxonomyTaskWarnings.begin(),
                     TaxonomyTaskWarnings.end());
  OutWarnings.insert(OutWarnings.end(), HeadingOwnershipWarnings.begin(),
                     HeadingOwnershipWarnings.end());
  OutWarnings.insert(OutWarnings.end(), TestingActorWarnings.begin(),
                     TestingActorWarnings.end());
  OutWarnings.insert(OutWarnings.end(), HeadingAliasWarnings.begin(),
                     HeadingAliasWarnings.end());
  if (MissingPhasePlaybookCount > 0) {
    AddWarning(OutWarnings, "Topics with `missing_phase_playbook`: " +
                                std::to_string(MissingPhasePlaybookCount));
  }
  if (UnknownStatusCount > 0) {
    AddWarning(OutWarnings, "Documents with unresolved status (`unknown`): " +
                                std::to_string(UnknownStatusCount));
  }
  if (UnresolvedRuleCount > 0) {
    AddWarning(OutWarnings, "Rules with unresolved provenance: " +
                                std::to_string(UnresolvedRuleCount));
  }

  for (const ValidateCheck &Check : Checks) {
    if (Check.mbCritical && !Check.mbOk) {
      std::string Error = Check.mId + " failed (" + Check.mDetail + ")";
      if (!Check.mRuleId.empty()) {
        Error += " [rule=" + Check.mRuleId + "]";
      }
      if (!Check.mDiagnostics.empty()) {
        Error += " [example=" + Check.mDiagnostics.front() + "]";
      }
      OutErrors.push_back(std::move(Error));
    }
  }

  NormalizeWarnings(OutWarnings);
  OutOk = OutErrors.empty();
  return Checks;
}

} // namespace UniPlan
