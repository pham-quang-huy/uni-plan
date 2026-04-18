#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// search — full-text substring search across plan documents
// ---------------------------------------------------------------------------

struct FSearchMatch
{
    std::string mSectionID;
    int mLine = 0;
    std::string mText;
};

struct FSearchResult
{
    std::string mTopicKey;
    std::string mPath;
    std::string mStatus;
    std::vector<FSearchMatch> mMatches;
};

static std::string ToLowerCopy(const std::string &InValue)
{
    std::string Result = InValue;
    std::transform(Result.begin(), Result.end(), Result.begin(),
                   [](unsigned char Character)
                   { return static_cast<char>(std::tolower(Character)); });
    return Result;
}

int RunSearchCommand(const std::vector<std::string> &InArgs)
{
    std::string Query;
    std::string TypeFilter;
    std::string StatusFilter;
    std::string SectionFilter;
    std::string RepoRoot;
    int Limit = 20;

    for (size_t Index = 0; Index < InArgs.size(); ++Index)
    {
        const std::string &Token = InArgs[Index];
        if (Token == "--query" && Index + 1 < InArgs.size())
            Query = InArgs[++Index];
        else if (Token == "--type" && Index + 1 < InArgs.size())
            TypeFilter = ToLower(InArgs[++Index]);
        else if (Token == "--status" && Index + 1 < InArgs.size())
            StatusFilter = ToLower(InArgs[++Index]);
        else if (Token == "--section" && Index + 1 < InArgs.size())
            SectionFilter = ToLower(InArgs[++Index]);
        else if (Token == "--limit" && Index + 1 < InArgs.size())
        {
            try
            {
                Limit = std::stoi(InArgs[++Index]);
                if (Limit < 1)
                    Limit = 1;
            }
            catch (...)
            {
                Limit = 20;
            }
        }
        else if (Token == "--repo-root" && Index + 1 < InArgs.size())
            RepoRoot = InArgs[++Index];
    }

    if (Query.empty())
    {
        std::cerr << "search requires --query <text>\n";
        return 2;
    }

    const fs::path Root = NormalizeRepoRootPath(RepoRoot);
    const Inventory Inv = BuildInventory(Root.string(), true, "", false);

    const std::string QueryLower = ToLowerCopy(Query);

    // Collect documents to search
    std::vector<DocumentRecord> DocsToSearch;
    if (TypeFilter.empty() || TypeFilter == "plan")
    {
        for (const DocumentRecord &Doc : Inv.mPlans)
            DocsToSearch.push_back(Doc);
    }
    if (TypeFilter.empty() || TypeFilter == "playbook")
    {
        for (const DocumentRecord &Doc : Inv.mPlaybooks)
            DocsToSearch.push_back(Doc);
    }
    if (TypeFilter.empty() || TypeFilter == "impl" ||
        TypeFilter == "implementation")
    {
        for (const DocumentRecord &Doc : Inv.mImplementations)
            DocsToSearch.push_back(Doc);
    }

    // Filter by status if specified
    if (!StatusFilter.empty() && StatusFilter != "all")
    {
        std::vector<DocumentRecord> Filtered;
        for (const DocumentRecord &Doc : DocsToSearch)
        {
            if (Doc.mStatus == StatusFilter)
            {
                Filtered.push_back(Doc);
            }
        }
        DocsToSearch = std::move(Filtered);
    }

    // Search each document
    std::vector<FSearchResult> Results;
    for (const DocumentRecord &Doc : DocsToSearch)
    {
        const fs::path AbsPath = Root / Doc.mPath;
        std::vector<std::string> Lines;
        std::string Error;
        if (!TryReadFileLines(AbsPath, Lines, Error))
        {
            continue;
        }

        const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);

        FSearchResult Result;
        Result.mTopicKey = Doc.mTopicKey;
        Result.mPath = Doc.mPath;
        Result.mStatus = Doc.mStatus;

        for (int LineIndex = 0; LineIndex < static_cast<int>(Lines.size());
             ++LineIndex)
        {
            const std::string LineLower =
                ToLowerCopy(Lines[static_cast<size_t>(LineIndex)]);
            if (LineLower.find(QueryLower) == std::string::npos)
            {
                continue;
            }

            // Determine section
            std::string CurrentSection;
            for (const HeadingRecord &Heading : Headings)
            {
                if (Heading.mLine <= LineIndex)
                {
                    CurrentSection = Heading.mSectionID;
                }
                else
                {
                    break;
                }
            }

            // Filter by section if specified
            if (!SectionFilter.empty() && CurrentSection != SectionFilter)
            {
                continue;
            }

            FSearchMatch Match;
            Match.mSectionID = CurrentSection;
            Match.mLine = LineIndex + 1;
            Match.mText = Trim(Lines[static_cast<size_t>(LineIndex)]);
            if (Match.mText.size() > 200)
            {
                Match.mText = Match.mText.substr(0, 200) + "...";
            }
            Result.mMatches.push_back(std::move(Match));
        }

        if (!Result.mMatches.empty())
        {
            Results.push_back(std::move(Result));
        }
    }

    // Sort by match count descending
    std::sort(Results.begin(), Results.end(),
              [](const FSearchResult &InLeft, const FSearchResult &InRight)
              { return InLeft.mMatches.size() > InRight.mMatches.size(); });

    // Truncate to limit
    if (static_cast<int>(Results.size()) > Limit)
    {
        Results.resize(static_cast<size_t>(Limit));
    }

    // Output JSON
    PrintJsonHeader("uni-plan-search-v1", GetUtcNow(), ToGenericPath(Root));
    std::cout << "\"query\":" << JsonQuote(Query)
              << ",\"count\":" << static_cast<int>(Results.size())
              << ",\"results\":[";

    for (size_t Index = 0; Index < Results.size(); ++Index)
    {
        if (Index > 0)
            std::cout << ",";
        const FSearchResult &Result = Results[Index];
        std::cout << "{\"topic_key\":" << JsonQuote(Result.mTopicKey)
                  << ",\"path\":" << JsonQuote(Result.mPath)
                  << ",\"status\":" << JsonQuote(Result.mStatus)
                  << ",\"match_count\":"
                  << static_cast<int>(Result.mMatches.size())
                  << ",\"matches\":[";

        for (size_t MatchIndex = 0;
             MatchIndex < Result.mMatches.size() && MatchIndex < 5;
             ++MatchIndex)
        {
            if (MatchIndex > 0)
                std::cout << ",";
            const FSearchMatch &Match = Result.mMatches[MatchIndex];
            std::cout << "{\"section\":" << JsonQuote(Match.mSectionID)
                      << ",\"line\":" << Match.mLine
                      << ",\"text\":" << JsonQuote(Match.mText) << "}";
        }

        std::cout << "]}";
    }

    std::cout << "]}\n";
    return 0;
}

} // namespace UniPlan
