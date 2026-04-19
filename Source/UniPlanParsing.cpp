#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace UniPlan
{

const std::regex kMarkdownPathRegex(R"([A-Za-z0-9_./\\-]+\.md)");

// Type definitions (EDocumentKind, DocumentRecord, ..., PhaseListAllEntry)
// moved to DocTypes.h

void SortRecords(std::vector<DocumentRecord> &InOutRecords)
{
    std::sort(InOutRecords.begin(), InOutRecords.end(),
              [](const DocumentRecord &InLeft, const DocumentRecord &InRight)
              {
                  if (InLeft.mTopicKey != InRight.mTopicKey)
                  {
                      return InLeft.mTopicKey < InRight.mTopicKey;
                  }
                  if (InLeft.mPhaseKey != InRight.mPhaseKey)
                  {
                      return InLeft.mPhaseKey < InRight.mPhaseKey;
                  }
                  return InLeft.mPath < InRight.mPath;
              });
}

void SortSidecars(std::vector<SidecarRecord> &InOutRecords)
{
    std::sort(InOutRecords.begin(), InOutRecords.end(),
              [](const SidecarRecord &InLeft, const SidecarRecord &InRight)
              {
                  if (InLeft.mTopicKey != InRight.mTopicKey)
                  {
                      return InLeft.mTopicKey < InRight.mTopicKey;
                  }
                  if (InLeft.mPhaseKey != InRight.mPhaseKey)
                  {
                      return InLeft.mPhaseKey < InRight.mPhaseKey;
                  }
                  if (InLeft.mOwnerKind != InRight.mOwnerKind)
                  {
                      return InLeft.mOwnerKind < InRight.mOwnerKind;
                  }
                  if (InLeft.mDocKind != InRight.mDocKind)
                  {
                      return InLeft.mDocKind < InRight.mDocKind;
                  }
                  return InLeft.mPath < InRight.mPath;
              });
}

std::string BuildPlaybookIdentity(const std::string &InTopicKey,
                                  const std::string &InPhaseKey)
{
    return InTopicKey + "::" + InPhaseKey;
}

std::string BuildSidecarIdentity(const SidecarRecord &InRecord)
{
    return InRecord.mOwnerKind + "|" + InRecord.mTopicKey + "|" +
           InRecord.mPhaseKey + "|" + InRecord.mDocKind;
}

std::vector<TopicPairRecord>
BuildTopicPairs(const std::vector<DocumentRecord> &InPlans,
                const std::vector<DocumentRecord> &InPlaybooks,
                const std::vector<DocumentRecord> &InImplementations,
                std::vector<std::string> &OutWarnings)
{
    struct TopicAggregate
    {
        std::string mPlanPath;
        std::string mPlanStatus;
        std::string mImplementationPath;
        std::string mImplementationStatus;
        std::vector<DocumentRecord> mPlaybooks;
        std::map<std::string, std::string> mPlaybookPhaseToPath;
    };

    std::map<std::string, TopicAggregate> Topics;

    for (const DocumentRecord &Record : InPlans)
    {
        TopicAggregate &Aggregate = Topics[Record.mTopicKey];
        if (!Aggregate.mPlanPath.empty() && Aggregate.mPlanPath != Record.mPath)
        {
            AddWarning(OutWarnings, "Duplicate plan for topic '" +
                                        Record.mTopicKey + "': '" +
                                        Aggregate.mPlanPath + "', '" +
                                        Record.mPath + "'.");
        }
        Aggregate.mPlanPath = Record.mPath;
        Aggregate.mPlanStatus = Record.mStatus;
    }

    for (const DocumentRecord &Record : InImplementations)
    {
        TopicAggregate &Aggregate = Topics[Record.mTopicKey];
        if (!Aggregate.mImplementationPath.empty() &&
            Aggregate.mImplementationPath != Record.mPath)
        {
            AddWarning(OutWarnings, "Duplicate implementation for topic '" +
                                        Record.mTopicKey + "': '" +
                                        Aggregate.mImplementationPath + "', '" +
                                        Record.mPath + "'.");
        }
        Aggregate.mImplementationPath = Record.mPath;
        Aggregate.mImplementationStatus = Record.mStatus;
    }

    for (const DocumentRecord &Record : InPlaybooks)
    {
        TopicAggregate &Aggregate = Topics[Record.mTopicKey];
        const auto ExistingPhase =
            Aggregate.mPlaybookPhaseToPath.find(Record.mPhaseKey);
        if (ExistingPhase != Aggregate.mPlaybookPhaseToPath.end() &&
            ExistingPhase->second != Record.mPath)
        {
            AddWarning(OutWarnings, "Duplicate playbook for topic '" +
                                        Record.mTopicKey + "' and phase '" +
                                        Record.mPhaseKey + "': '" +
                                        ExistingPhase->second + "', '" +
                                        Record.mPath + "'.");
        }
        Aggregate.mPlaybookPhaseToPath[Record.mPhaseKey] = Record.mPath;
        Aggregate.mPlaybooks.push_back(Record);
    }

    std::vector<TopicPairRecord> Pairs;
    for (auto &Entry : Topics)
    {
        TopicPairRecord PairRecord;
        PairRecord.mTopicKey = Entry.first;
        PairRecord.mPlanPath = Entry.second.mPlanPath;
        PairRecord.mPlanStatus = Entry.second.mPlanStatus;
        PairRecord.mImplementationPath = Entry.second.mImplementationPath;
        PairRecord.mImplementationStatus = Entry.second.mImplementationStatus;
        PairRecord.mPlaybooks = Entry.second.mPlaybooks;
        SortRecords(PairRecord.mPlaybooks);

        const bool HasPlan = !PairRecord.mPlanPath.empty();
        const bool HasImplementation = !PairRecord.mImplementationPath.empty();
        const bool HasPlaybook = !PairRecord.mPlaybooks.empty();

        StatusCounters PairStatusCounters;
        AddStatusCandidate(PairStatusCounters, PairRecord.mPlanStatus);
        AddStatusCandidate(PairStatusCounters,
                           PairRecord.mImplementationStatus);
        for (const DocumentRecord &Playbook : PairRecord.mPlaybooks)
        {
            AddStatusCandidate(PairStatusCounters, Playbook.mStatus);
        }
        PairRecord.mOverallStatus = ResolveNormalizedStatus(PairStatusCounters);

        if (HasPlan && HasImplementation && HasPlaybook)
        {
            PairRecord.mPairState = "paired";
        }
        else if (HasPlan && HasImplementation)
        {
            PairRecord.mPairState = "missing_phase_playbook";
        }
        else if (HasPlan)
        {
            PairRecord.mPairState = "missing_implementation";
        }
        else if (HasImplementation)
        {
            PairRecord.mPairState = "orphan_implementation";
        }
        else if (HasPlaybook)
        {
            PairRecord.mPairState = "orphan_playbook";
        }
        else
        {
            PairRecord.mPairState = "unknown";
        }

        Pairs.push_back(std::move(PairRecord));
    }

    std::sort(Pairs.begin(), Pairs.end(),
              [](const TopicPairRecord &InLeft, const TopicPairRecord &InRight)
              { return InLeft.mTopicKey < InRight.mTopicKey; });

    return Pairs;
}

void AppendSidecarIntegrityWarnings(
    const std::vector<DocumentRecord> &InPlans,
    const std::vector<DocumentRecord> &InPlaybooks,
    const std::vector<DocumentRecord> &InImplementations,
    const std::vector<SidecarRecord> &InSidecars,
    std::vector<std::string> &OutWarnings)
{
    std::set<std::string> PlanTopics;
    std::set<std::string> ImplementationTopics;
    std::set<std::string> PlaybookKeys;
    std::map<std::string, std::string> SidecarIdentityToPath;

    for (const DocumentRecord &Record : InPlans)
    {
        PlanTopics.insert(Record.mTopicKey);
    }

    for (const DocumentRecord &Record : InImplementations)
    {
        ImplementationTopics.insert(Record.mTopicKey);
    }

    for (const DocumentRecord &Record : InPlaybooks)
    {
        PlaybookKeys.insert(
            BuildPlaybookIdentity(Record.mTopicKey, Record.mPhaseKey));
    }

    for (const SidecarRecord &Sidecar : InSidecars)
    {
        const std::string SidecarIdentity = BuildSidecarIdentity(Sidecar);
        const auto ExistingSidecar =
            SidecarIdentityToPath.find(SidecarIdentity);
        if (ExistingSidecar != SidecarIdentityToPath.end() &&
            ExistingSidecar->second != Sidecar.mPath)
        {
            AddWarning(OutWarnings,
                       "Duplicate sidecar for '" + Sidecar.mOwnerKind +
                           "' topic '" + Sidecar.mTopicKey + "' phase '" +
                           Sidecar.mPhaseKey + "' doc-kind '" +
                           Sidecar.mDocKind + "': '" + ExistingSidecar->second +
                           "', '" + Sidecar.mPath + "'.");
        }
        SidecarIdentityToPath[SidecarIdentity] = Sidecar.mPath;

        if (Sidecar.mOwnerKind == "Plan")
        {
            if (PlanTopics.count(Sidecar.mTopicKey) == 0)
            {
                AddWarning(
                    OutWarnings,
                    "Orphan plan sidecar without plan owner for topic '" +
                        Sidecar.mTopicKey + "': '" + Sidecar.mPath + "'.");
            }
            continue;
        }

        if (Sidecar.mOwnerKind == "Impl")
        {
            if (ImplementationTopics.count(Sidecar.mTopicKey) == 0)
            {
                AddWarning(OutWarnings, "Orphan implementation sidecar without "
                                        "implementation owner for topic '" +
                                            Sidecar.mTopicKey + "': '" +
                                            Sidecar.mPath + "'.");
            }
            continue;
        }

        if (Sidecar.mOwnerKind == "Playbook")
        {
            const std::string PlaybookIdentity =
                BuildPlaybookIdentity(Sidecar.mTopicKey, Sidecar.mPhaseKey);
            if (PlaybookKeys.count(PlaybookIdentity) == 0)
            {
                AddWarning(OutWarnings, "Orphan playbook sidecar without "
                                        "playbook owner for topic '" +
                                            Sidecar.mTopicKey + "' phase '" +
                                            Sidecar.mPhaseKey + "': '" +
                                            Sidecar.mPath + "'.");
            }
            continue;
        }

        AddWarning(OutWarnings, "Unsupported sidecar owner kind '" +
                                    Sidecar.mOwnerKind + "' for path '" +
                                    Sidecar.mPath + "'.");
    }
}

bool ShouldSkipRecursionDirectory(const fs::path &InPath)
{
    const std::string DirectoryName = InPath.filename().string();
    if (DirectoryName.empty())
    {
        return false;
    }

    if (DirectoryName[0] == '.')
    {
        return true;
    }

    return DirectoryName == "Build" || DirectoryName == "ThirdParty" ||
           DirectoryName == "Intermediate" || DirectoryName == "node_modules";
}

fs::path NormalizeRepoRootPath(const std::string &InRepoRoot)
{
    if (InRepoRoot.empty())
    {
        throw std::runtime_error("Repository root cannot be empty.");
    }

    std::error_code Error;
    const fs::path AbsoluteRoot = fs::absolute(fs::path(InRepoRoot), Error);
    if (Error)
    {
        throw std::runtime_error(
            "Failed to resolve repository root to absolute path '" +
            InRepoRoot + "': " + Error.message());
    }

    const fs::path CanonicalRoot = fs::weakly_canonical(AbsoluteRoot, Error);
    if (Error)
    {
        throw std::runtime_error("Failed to canonicalize repository root '" +
                                 AbsoluteRoot.string() +
                                 "': " + Error.message());
    }

    static bool bPrinted = false;
    if (!bPrinted)
    {
        PrintRepoInfo(CanonicalRoot);
        bPrinted = true;
    }

    return CanonicalRoot;
}

bool IsExcludedDocsScriptPath(const std::string &InRelativePath)
{
    static const std::set<std::string> ExcludedSegments = {
        ".references", "ThirdParty", "x64", "build_deps"};

    std::string Segment;
    for (const char Character : (InRelativePath + "/"))
    {
        if (Character == '/')
        {
            if (!Segment.empty() && ExcludedSegments.count(Segment) > 0)
            {
                return true;
            }
            Segment.clear();
            continue;
        }
        Segment.push_back(Character);
    }

    return false;
}

std::vector<MarkdownDocument>
EnumerateMarkdownDocuments(const fs::path &InRepoRoot,
                           std::vector<std::string> &OutWarnings)
{
    std::vector<MarkdownDocument> Result;
    const fs::directory_options IteratorOptions =
        fs::directory_options::skip_permission_denied;

    std::error_code Error;
    fs::recursive_directory_iterator Iterator(InRepoRoot, IteratorOptions,
                                              Error);
    fs::recursive_directory_iterator EndIterator;
    if (Error)
    {
        throw std::runtime_error(
            "Failed to initialize markdown traversal at '" +
            InRepoRoot.string() + "': " + Error.message());
    }

    while (Iterator != EndIterator)
    {
        const fs::directory_entry Entry = *Iterator;
        const fs::path AbsolutePath = Entry.path();
        const auto AdvanceIterator =
            [&Iterator, &EndIterator, &OutWarnings, &AbsolutePath]()
        {
            std::error_code AdvanceError;
            Iterator.increment(AdvanceError);
            if (AdvanceError)
            {
                AddWarning(OutWarnings,
                           "Traversal advance failure after path '" +
                               AbsolutePath.string() +
                               "': " + AdvanceError.message());
                Iterator = EndIterator;
            }
        };

        fs::path RelativePath;
        try
        {
            RelativePath = fs::relative(AbsolutePath, InRepoRoot);
        }
        catch (const fs::filesystem_error &)
        {
            AdvanceIterator();
            continue;
        }

        const std::string Relative = ToGenericPath(RelativePath);

        std::error_code PathTypeError;
        const bool IsDirectory = Entry.is_directory(PathTypeError);
        if (PathTypeError)
        {
            AddWarning(OutWarnings,
                       "Skipping path due to directory-type read failure '" +
                           AbsolutePath.string() +
                           "': " + PathTypeError.message());
            AdvanceIterator();
            continue;
        }

        if (IsDirectory && IsExcludedDocsScriptPath(Relative))
        {
            Iterator.disable_recursion_pending();
            AdvanceIterator();
            continue;
        }

        const bool IsRegularFile = Entry.is_regular_file(PathTypeError);
        if (PathTypeError)
        {
            AddWarning(OutWarnings,
                       "Skipping path due to file-type read failure '" +
                           AbsolutePath.string() +
                           "': " + PathTypeError.message());
            AdvanceIterator();
            continue;
        }

        if (!IsRegularFile)
        {
            AdvanceIterator();
            continue;
        }

        if (AbsolutePath.extension() != ".md" ||
            IsExcludedDocsScriptPath(Relative))
        {
            AdvanceIterator();
            continue;
        }

        Result.push_back({AbsolutePath, Relative});
        AdvanceIterator();
    }

    std::sort(
        Result.begin(), Result.end(),
        [](const MarkdownDocument &InLeft, const MarkdownDocument &InRight)
        { return InLeft.mRelativePath < InRight.mRelativePath; });

    return Result;
}

bool IsAllowedLintFilename(const std::string &InName)
{
    static const std::set<std::string> AllowedSimpleNames = {
        "README.md",   "INDEX.md",  "AGENTS.md",      "TODO.md",
        "CODING.md",   "NAMING.md", "DIAGRAMMING.md", "COORDINATESYSTEM.md",
        "PATTERNS.md", "PARITY.md", "PARALLEL.md",    "PARALLEL.sample.md"};

    static const std::regex AllowedPrefixPattern(
        R"(^(PLAN|PLAYBOOK|IMPLEMENTATION|CHANGELOG|VERIFICATION|SPEC|REFERENCE|DIAGRAM|MIGRATION|ADR|ARCHITECTURE)(-[A-Z0-9-]+)*\.md$)");
    static const std::regex AllowedTopicArtifactPattern(
        R"(^[A-Z][A-Za-z0-9-]*(\.[A-Z][A-Za-z0-9-]*)*\.(Plan|Impl|Playbook)(\.(ChangeLog|Verification))?\.md$)");
    static const std::regex AllowedSchemaPattern(
        R"(^[A-Z][A-Za-z0-9]*\.Schema\.md$)");

    if (AllowedSimpleNames.count(InName) > 0)
    {
        return true;
    }

    return std::regex_match(InName, AllowedPrefixPattern) ||
           std::regex_match(InName, AllowedTopicArtifactPattern) ||
           std::regex_match(InName, AllowedSchemaPattern);
}

bool HasFirstNonEmptyLineH1(const fs::path &InPath, std::string &OutError)
{
    std::ifstream Input(InPath);
    if (!Input.is_open())
    {
        OutError = "Unable to open file.";
        return false;
    }

    std::string Line;
    while (std::getline(Input, Line))
    {
        const std::string Trimmed = Trim(Line);
        if (Trimmed.empty())
        {
            continue;
        }

        return Line.size() >= 2 && Line[0] == '#' &&
               std::isspace(static_cast<unsigned char>(Line[1])) != 0;
    }

    if (Input.bad())
    {
        OutError = "File read failure.";
        return false;
    }

    return false;
}

bool IsMarkdownTableLine(const std::string &InTrimmedLine)
{
    if (InTrimmedLine.size() < 2)
    {
        return false;
    }
    if (InTrimmedLine.front() != '|')
    {
        return false;
    }
    return InTrimmedLine.find('|', 1) != std::string::npos;
}

std::string NormalizeSectionID(const std::string &InHeadingText)
{
    const std::string HeadingText = Trim(InHeadingText);
    size_t Start = 0;
    while (Start < HeadingText.size())
    {
        const unsigned char Character =
            static_cast<unsigned char>(HeadingText[Start]);
        if (std::isdigit(Character) != 0 || std::isspace(Character) != 0 ||
            HeadingText[Start] == '.' || HeadingText[Start] == ')' ||
            HeadingText[Start] == '-' || HeadingText[Start] == '_')
        {
            ++Start;
            continue;
        }
        break;
    }

    std::string Result;
    bool LastUnderscore = false;
    for (size_t Index = Start; Index < HeadingText.size(); ++Index)
    {
        const unsigned char Character =
            static_cast<unsigned char>(HeadingText[Index]);
        if (std::isalnum(Character) != 0)
        {
            Result.push_back(static_cast<char>(std::tolower(Character)));
            LastUnderscore = false;
            continue;
        }

        if (!LastUnderscore && !Result.empty())
        {
            Result.push_back('_');
            LastUnderscore = true;
        }
    }

    while (!Result.empty() && Result.back() == '_')
    {
        Result.pop_back();
    }
    return Result;
}

bool ParseFenceDelimiterLine(const std::string &InLine, char &OutFenceChar,
                             size_t &OutFenceLength, std::string &OutRemainder)
{
    size_t Start = 0;
    while (Start < InLine.size() &&
           std::isspace(static_cast<unsigned char>(InLine[Start])) != 0)
    {
        ++Start;
    }

    if (Start >= InLine.size())
    {
        return false;
    }

    const char Marker = InLine[Start];
    if (Marker != '`' && Marker != '~')
    {
        return false;
    }

    size_t MarkerLength = 0;
    while ((Start + MarkerLength) < InLine.size() &&
           InLine[Start + MarkerLength] == Marker)
    {
        ++MarkerLength;
    }

    if (MarkerLength < 3)
    {
        return false;
    }

    OutFenceChar = Marker;
    OutFenceLength = MarkerLength;
    OutRemainder = Trim(InLine.substr(Start + MarkerLength));
    return true;
}

std::vector<HeadingRecord>
ParseHeadingRecords(const std::vector<std::string> &InLines)
{
    static const std::regex HeadingRegex(R"(^\s*(#{1,6})\s+(.+?)\s*$)");
    std::vector<HeadingRecord> Headings;
    bool InFenceBlock = false;
    char ActiveFenceChar = '\0';
    size_t ActiveFenceLength = 0;
    for (size_t Index = 0; Index < InLines.size(); ++Index)
    {
        char FenceChar = '\0';
        size_t FenceLength = 0;
        std::string FenceRemainder;
        if (ParseFenceDelimiterLine(InLines[Index], FenceChar, FenceLength,
                                    FenceRemainder))
        {
            if (!InFenceBlock)
            {
                InFenceBlock = true;
                ActiveFenceChar = FenceChar;
                ActiveFenceLength = FenceLength;
                continue;
            }

            if (FenceChar == ActiveFenceChar &&
                FenceLength >= ActiveFenceLength && FenceRemainder.empty())
            {
                InFenceBlock = false;
                ActiveFenceChar = '\0';
                ActiveFenceLength = 0;
            }
            continue;
        }

        if (InFenceBlock)
        {
            continue;
        }

        std::smatch Match;
        if (!std::regex_match(InLines[Index], Match, HeadingRegex))
        {
            continue;
        }

        std::string HeadingText = Trim(Match[2].str());
        while (!HeadingText.empty() && HeadingText.back() == '#')
        {
            HeadingText.pop_back();
        }
        HeadingText = Trim(HeadingText);
        if (HeadingText.empty())
        {
            continue;
        }

        HeadingRecord Heading;
        Heading.mLine = static_cast<int>(Index) + 1;
        Heading.mLevel = static_cast<int>(Match[1].str().size());
        Heading.mText = HeadingText;
        Heading.mSectionID = NormalizeSectionID(HeadingText);
        Headings.push_back(std::move(Heading));
    }
    return Headings;
}

std::vector<MarkdownTableRecord>
ParseMarkdownTables(const std::vector<std::string> &InLines,
                    const std::vector<HeadingRecord> &InHeadings)
{
    std::vector<MarkdownTableRecord> Tables;
    size_t HeadingIndex = 0;
    std::string CurrentSectionID;
    std::string CurrentHeading;
    int TableID = 1;

    for (size_t Index = 0; Index < InLines.size(); ++Index)
    {
        const int LineNumber = static_cast<int>(Index) + 1;
        while (HeadingIndex < InHeadings.size() &&
               InHeadings[HeadingIndex].mLine <= LineNumber)
        {
            CurrentSectionID = InHeadings[HeadingIndex].mSectionID;
            CurrentHeading = InHeadings[HeadingIndex].mText;
            ++HeadingIndex;
        }

        const std::string HeaderLine = Trim(InLines[Index]);
        if (!IsMarkdownTableLine(HeaderLine))
        {
            continue;
        }
        if (Index + 1 >= InLines.size())
        {
            continue;
        }

        const std::string DividerLine = Trim(InLines[Index + 1]);
        if (!IsMarkdownTableLine(DividerLine))
        {
            continue;
        }

        const std::vector<std::string> HeaderCells =
            SplitMarkdownTableRow(HeaderLine);
        const std::vector<std::string> DividerCells =
            SplitMarkdownTableRow(DividerLine);
        if (HeaderCells.empty() || !IsDividerRow(DividerCells))
        {
            continue;
        }

        MarkdownTableRecord Table;
        Table.mTableID = TableID;
        Table.mStartLine = LineNumber;
        Table.mSectionID = CurrentSectionID;
        Table.mSectionHeading = CurrentHeading;
        Table.mHeaders = HeaderCells;

        size_t RowIndex = Index + 2;
        while (RowIndex < InLines.size())
        {
            const std::string RowLine = Trim(InLines[RowIndex]);
            if (!IsMarkdownTableLine(RowLine))
            {
                break;
            }

            std::vector<std::string> RowCells = SplitMarkdownTableRow(RowLine);
            if (!IsDividerRow(RowCells))
            {
                // Pad short rows to match header count
                while (RowCells.size() < HeaderCells.size())
                {
                    RowCells.emplace_back("");
                }
                Table.mRows.push_back(std::move(RowCells));
            }
            ++RowIndex;
        }

        Table.mEndLine = static_cast<int>(RowIndex);
        Tables.push_back(std::move(Table));
        TableID += 1;
        Index = (RowIndex == 0) ? Index : RowIndex - 1;
    }

    return Tables;
}

std::string NormalizeHeaderKey(const std::string &InValue)
{
    std::string Key = ToLower(Trim(InValue));
    for (char &Character : Key)
    {
        if (Character == '_' || Character == '-')
        {
            Character = ' ';
        }
    }
    return Key;
}

// ResolvedDocument moved to DocTypes.h

} // namespace UniPlan
