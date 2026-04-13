#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJsonIO.h"
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

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace UniPlan
{

const std::regex kMarkdownPathRegex(R"([A-Za-z0-9_./\\-]+\.md)");

// Type definitions (EDocumentKind, DocumentRecord, ..., PhaseListAllEntry)
// moved to DocTypes.h

bool PathContainsSegment(const std::string &InRelativePath,
                         const std::string &InSegment)
{
    const std::string PaddedPath = "/" + InRelativePath;
    return PaddedPath.find(InSegment) != std::string::npos;
}

bool IsPlanPath(const std::string &InRelativePath)
{
    return PathContainsSegment(InRelativePath, "/Docs/Plans/");
}

bool IsPlaybookPath(const std::string &InRelativePath)
{
    return PathContainsSegment(InRelativePath, "/Docs/Playbooks/");
}

bool IsImplementationPath(const std::string &InRelativePath)
{
    return PathContainsSegment(InRelativePath, "/Docs/Implementation/");
}

bool TryClassifyCoreDocument(const std::string &InRelativePath,
                             const std::string &InFilename,
                             DocumentRecord &OutRecord)
{
    static const std::regex PlanNameRegex(
        R"(^([A-Za-z0-9]+)\.Plan\.(json|md)$)");
    static const std::regex PlaybookNameRegex(
        R"(^([A-Za-z0-9]+)\.([^.]+)\.Playbook\.(json|md)$)");
    static const std::regex ImplementationNameRegex(
        R"(^([A-Za-z0-9]+)\.Impl\.(json|md)$)");

    std::smatch Match;
    if (IsPlanPath(InRelativePath) &&
        std::regex_match(InFilename, Match, PlanNameRegex))
    {
        OutRecord.mKind = EDocumentKind::Plan;
        OutRecord.mTopicKey = Match[1].str();
        OutRecord.mPath = InRelativePath;
        return true;
    }

    if (IsPlaybookPath(InRelativePath) &&
        std::regex_match(InFilename, Match, PlaybookNameRegex))
    {
        OutRecord.mKind = EDocumentKind::Playbook;
        OutRecord.mTopicKey = Match[1].str();
        OutRecord.mPhaseKey = Match[2].str();
        OutRecord.mPath = InRelativePath;
        return true;
    }

    if (IsImplementationPath(InRelativePath) &&
        std::regex_match(InFilename, Match, ImplementationNameRegex))
    {
        OutRecord.mKind = EDocumentKind::Implementation;
        OutRecord.mTopicKey = Match[1].str();
        OutRecord.mPath = InRelativePath;
        return true;
    }

    return false;
}

bool TryClassifySidecarDocument(const std::string &InRelativePath,
                                const std::string &InFilename,
                                SidecarRecord &OutRecord)
{
    static const std::regex PlanSidecarRegex(
        R"(^([A-Za-z0-9]+)\.Plan\.(ChangeLog|Verification)\.(json|md)$)");
    static const std::regex ImplementationSidecarRegex(
        R"(^([A-Za-z0-9]+)\.Impl\.(ChangeLog|Verification)\.(json|md)$)");
    static const std::regex PlaybookSidecarRegex(
        R"(^([A-Za-z0-9]+)\.([^.]+)\.Playbook\.(ChangeLog|Verification)\.(json|md)$)");

    std::smatch Match;
    if (IsPlanPath(InRelativePath) &&
        std::regex_match(InFilename, Match, PlanSidecarRegex))
    {
        OutRecord.mTopicKey = Match[1].str();
        OutRecord.mOwnerKind = "Plan";
        OutRecord.mDocKind = Match[2].str();
        OutRecord.mPath = InRelativePath;
        return true;
    }

    if (IsImplementationPath(InRelativePath) &&
        std::regex_match(InFilename, Match, ImplementationSidecarRegex))
    {
        OutRecord.mTopicKey = Match[1].str();
        OutRecord.mOwnerKind = "Impl";
        OutRecord.mDocKind = Match[2].str();
        OutRecord.mPath = InRelativePath;
        return true;
    }

    if (IsPlaybookPath(InRelativePath) &&
        std::regex_match(InFilename, Match, PlaybookSidecarRegex))
    {
        OutRecord.mTopicKey = Match[1].str();
        OutRecord.mPhaseKey = Match[2].str();
        OutRecord.mOwnerKind = "Playbook";
        OutRecord.mDocKind = Match[3].str();
        OutRecord.mPath = InRelativePath;
        return true;
    }

    return false;
}

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

Inventory BuildInventoryFresh(const std::string &InRepoRoot)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);

    std::error_code Error;
    if (!fs::exists(RepoRoot, Error))
    {
        if (Error)
        {
            throw std::runtime_error(
                "Failed to check repository root existence '" +
                RepoRoot.string() + "': " + Error.message());
        }
        throw std::runtime_error("Repository root does not exist: " +
                                 RepoRoot.string());
    }
    if (!fs::is_directory(RepoRoot, Error))
    {
        if (Error)
        {
            throw std::runtime_error(
                "Failed to check repository root directory type '" +
                RepoRoot.string() + "': " + Error.message());
        }
        throw std::runtime_error("Repository root is not a directory: " +
                                 RepoRoot.string());
    }

    Inventory Result;
    Result.mGeneratedUtc = GetUtcNow();
    Result.mRepoRoot = ToGenericPath(RepoRoot);

    const fs::directory_options IteratorOptions =
        fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator Iterator(RepoRoot, IteratorOptions, Error);
    fs::recursive_directory_iterator EndIterator;
    if (Error)
    {
        throw std::runtime_error(
            "Failed to initialize repository traversal at '" +
            RepoRoot.string() + "': " + Error.message());
    }

    while (Iterator != EndIterator)
    {
        const fs::directory_entry Entry = *Iterator;
        const fs::path AbsolutePath = Entry.path();
        const auto AdvanceIterator =
            [&Iterator, &EndIterator, &Result, &AbsolutePath]()
        {
            std::error_code AdvanceError;
            Iterator.increment(AdvanceError);
            if (AdvanceError)
            {
                AddWarning(Result.mWarnings,
                           "Traversal advance failure after path '" +
                               AbsolutePath.string() +
                               "': " + AdvanceError.message());
                Iterator = EndIterator;
            }
        };

        std::error_code PathTypeError;
        const bool IsDirectory = Entry.is_directory(PathTypeError);
        if (PathTypeError)
        {
            AddWarning(Result.mWarnings,
                       "Skipping path due to directory-type read failure '" +
                           AbsolutePath.string() +
                           "': " + PathTypeError.message());
            AdvanceIterator();
            continue;
        }

        if (IsDirectory && ShouldSkipRecursionDirectory(AbsolutePath))
        {
            Iterator.disable_recursion_pending();
            AdvanceIterator();
            continue;
        }

        const bool IsRegularFile = Entry.is_regular_file(PathTypeError);
        if (PathTypeError)
        {
            AddWarning(Result.mWarnings,
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

        const std::string Ext = AbsolutePath.extension().string();
        if (Ext != ".json" && Ext != ".md")
        {
            AdvanceIterator();
            continue;
        }

        fs::path RelativePath;
        try
        {
            RelativePath = fs::relative(AbsolutePath, RepoRoot);
        }
        catch (const fs::filesystem_error &)
        {
            AdvanceIterator();
            continue;
        }

        const std::string Relative = ToGenericPath(RelativePath);
        const std::string Filename = AbsolutePath.filename().string();

        // Try as plan-bundle first (*.Plan.json with
        // plan-bundle schema)
        static const std::regex BundleRegex(R"(^([A-Za-z0-9]+)\.Plan\.json$)");
        std::smatch BundleMatch;
        bool HandledAsBundle = false;

        if (std::regex_match(Filename, BundleMatch, BundleRegex))
        {
            FTopicBundle Bundle;
            std::string BundleError;
            if (TryReadTopicBundle(AbsolutePath, Bundle, BundleError))
            {
                HandledAsBundle = true;

                // Create DocumentRecords from bundle
                const std::string TopicKey = Bundle.mTopicKey;

                // Derive plan status — try bundle level, then
                // scan plan summary table for Status row
                std::string DerivedStatus = Bundle.mStatus;
                if (DerivedStatus.empty() || DerivedStatus == "unknown")
                {
                    DerivedStatus = ToString(Bundle.mPlan.mStatus);
                }
                if (DerivedStatus.empty() || DerivedStatus == "unknown")
                {
                    // Scan plan tables for Status field
                    for (const auto &SP : Bundle.mPlan.mSections)
                    {
                        for (const FStructuredTable &T : SP.second.mTables)
                        {
                            if (T.mHeaders.size() < 2)
                                continue;
                            for (const auto &Row : T.mRows)
                            {
                                if (Row.size() >= 2 &&
                                    ToLower(Trim(Row[0].mValue)) == "status")
                                {
                                    DerivedStatus =
                                        NormalizeStatusValue(Row[1].mValue);
                                    break;
                                }
                            }
                            if (DerivedStatus != "unknown")
                                break;
                        }
                        if (DerivedStatus != "unknown")
                            break;
                    }
                }

                // Plan record
                {
                    DocumentRecord PlanRecord;
                    PlanRecord.mKind = EDocumentKind::Plan;
                    PlanRecord.mTopicKey = TopicKey;
                    PlanRecord.mPath = Relative + "#plan";
                    PlanRecord.mStatus = DerivedStatus;
                    PlanRecord.mStatusRaw = DerivedStatus;
                    Result.mPlans.push_back(std::move(PlanRecord));
                }

                // Implementation record
                {
                    DocumentRecord ImplRecord;
                    ImplRecord.mKind = EDocumentKind::Implementation;
                    ImplRecord.mTopicKey = TopicKey;
                    ImplRecord.mPath = Relative + "#implementation";
                    ImplRecord.mStatus =
                        ToString(Bundle.mImplementation.mStatus);
                    ImplRecord.mStatusRaw = Bundle.mImplementation.mStatusRaw;
                    Result.mImplementations.push_back(std::move(ImplRecord));
                }

                // Playbook records
                for (const auto &PBPair : Bundle.mPlaybooks)
                {
                    DocumentRecord PBRecord;
                    PBRecord.mKind = EDocumentKind::Playbook;
                    PBRecord.mTopicKey = TopicKey;
                    PBRecord.mPhaseKey = PBPair.first;
                    PBRecord.mPath = Relative + "#playbook:" + PBPair.first;
                    PBRecord.mStatus = ToString(PBPair.second.mStatus);
                    PBRecord.mStatusRaw = PBPair.second.mStatusRaw;
                    Result.mPlaybooks.push_back(std::move(PBRecord));
                }

                // Sidecar records for changelogs
                for (const auto &CLPair : Bundle.mChangeLogs)
                {
                    UniPlan::SidecarRecord SCRecord;
                    SCRecord.mTopicKey = TopicKey;
                    SCRecord.mDocKind = "ChangeLog";
                    if (CLPair.first == "plan")
                    {
                        SCRecord.mOwnerKind = "Plan";
                    }
                    else if (CLPair.first == "implementation")
                    {
                        SCRecord.mOwnerKind = "Impl";
                    }
                    else
                    {
                        SCRecord.mOwnerKind = "Playbook";
                        SCRecord.mPhaseKey = CLPair.first;
                    }
                    SCRecord.mPath = Relative + "#changelog:" + CLPair.first;
                    Result.mSidecars.push_back(std::move(SCRecord));
                }

                // Sidecar records for verifications
                for (const auto &VPair : Bundle.mVerifications)
                {
                    UniPlan::SidecarRecord SCRecord;
                    SCRecord.mTopicKey = TopicKey;
                    SCRecord.mDocKind = "Verification";
                    if (VPair.first == "plan")
                    {
                        SCRecord.mOwnerKind = "Plan";
                    }
                    else if (VPair.first == "implementation")
                    {
                        SCRecord.mOwnerKind = "Impl";
                    }
                    else
                    {
                        SCRecord.mOwnerKind = "Playbook";
                        SCRecord.mPhaseKey = VPair.first;
                    }
                    SCRecord.mPath = Relative + "#verification:" + VPair.first;
                    Result.mSidecars.push_back(std::move(SCRecord));
                }

                AdvanceIterator();
                continue;
            }
        }

        // Legacy fallback: old multi-file format
        // (*.Impl.json, *.Playbook.json, *.ChangeLog.json, etc.)
        if (!HandledAsBundle)
        {
            DocumentRecord CoreRecord;
            if (TryClassifyCoreDocument(Relative, Filename, CoreRecord))
            {
                FDocument JsonDoc;
                std::string JsonError;
                if (TryReadDocumentJson(AbsolutePath, JsonDoc, JsonError))
                {
                    CoreRecord.mStatusRaw = JsonDoc.mStatusRaw;
                    CoreRecord.mStatus = ToString(JsonDoc.mStatus);
                }
                else
                {
                    CoreRecord.mStatus = "unknown";
                }

                switch (CoreRecord.mKind)
                {
                case EDocumentKind::Plan:
                    Result.mPlans.push_back(CoreRecord);
                    break;
                case EDocumentKind::Playbook:
                    Result.mPlaybooks.push_back(CoreRecord);
                    break;
                case EDocumentKind::Implementation:
                    Result.mImplementations.push_back(CoreRecord);
                    break;
                }
                AdvanceIterator();
                continue;
            }

            UniPlan::SidecarRecord SCRec;
            if (TryClassifySidecarDocument(Relative, Filename, SCRec))
            {
                Result.mSidecars.push_back(SCRec);
            }
        }

        AdvanceIterator();
    }

    SortRecords(Result.mPlans);
    SortRecords(Result.mPlaybooks);
    SortRecords(Result.mImplementations);
    SortSidecars(Result.mSidecars);
    AppendSidecarIntegrityWarnings(Result.mPlans, Result.mPlaybooks,
                                   Result.mImplementations, Result.mSidecars,
                                   Result.mWarnings);
    Result.mPairs = BuildTopicPairs(Result.mPlans, Result.mPlaybooks,
                                    Result.mImplementations, Result.mWarnings);
    NormalizeWarnings(Result.mWarnings);

    return Result;
}

Inventory BuildInventory(const std::string &InRepoRoot, const bool InUseCache,
                         const std::string &InConfigCacheDir,
                         const bool InVerbose, const bool InQuiet)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);
    const std::string RepoRootPath = ToGenericPath(RepoRoot);
    const fs::path CachePath =
        BuildInventoryCachePath(RepoRoot, InConfigCacheDir);

    uint64_t Signature = 0;
    std::string SignatureError;
    std::string CacheReadError;
    const bool SignatureOk =
        TryComputeMarkdownCorpusSignature(RepoRoot, Signature, SignatureError);
    if (InUseCache && SignatureOk)
    {
        Inventory CachedInventory;
        if (TryLoadInventoryCache(CachePath, RepoRootPath, Signature,
                                  CachedInventory, CacheReadError))
        {
            if (!InQuiet && InVerbose)
            {
                std::cerr << "[cache hit] " << CachePath.string() << "\n";
            }
            CachedInventory.mGeneratedUtc = GetUtcNow();
            if (!InQuiet)
            {
                PrintScanInfo(CachedInventory.mPlans.size() +
                              CachedInventory.mPlaybooks.size() +
                              CachedInventory.mImplementations.size() +
                              CachedInventory.mSidecars.size());
            }
            return CachedInventory;
        }
    }

    if (!InQuiet && InVerbose)
    {
        std::cerr << "[cache miss] rebuilding inventory\n";
    }
    Inventory FreshInventory = BuildInventoryFresh(RepoRootPath);
    if (InUseCache && SignatureOk && !CacheReadError.empty())
    {
        AddWarning(FreshInventory.mWarnings, CacheReadError);
    }

    if (InUseCache && SignatureOk)
    {
        std::string CacheWriteError;
        if (!TryWriteInventoryCache(CachePath, FreshInventory, Signature,
                                    CacheWriteError))
        {
            AddWarning(FreshInventory.mWarnings, CacheWriteError);
        }
    }
    else if (InUseCache && !SignatureOk && !SignatureError.empty())
    {
        AddWarning(FreshInventory.mWarnings,
                   "Inventory cache bypassed: " + SignatureError);
    }

    NormalizeWarnings(FreshInventory.mWarnings);
    if (!InQuiet)
    {
        PrintScanInfo(FreshInventory.mPlans.size() +
                      FreshInventory.mPlaybooks.size() +
                      FreshInventory.mImplementations.size() +
                      FreshInventory.mSidecars.size());
    }
    return FreshInventory;
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

int CountFileLines(const fs::path &InPath, std::string &OutError)
{
    std::ifstream Input(InPath);
    if (!Input.is_open())
    {
        OutError = "Unable to open file.";
        return -1;
    }

    int LineCount = 0;
    std::string Line;
    while (std::getline(Input, Line))
    {
        LineCount += 1;
    }

    if (Input.bad())
    {
        OutError = "File read failure.";
        return -1;
    }

    return LineCount;
}

bool TryReadFileText(const fs::path &InPath, std::string &OutText,
                     std::string &OutError)
{
    std::ifstream Input(InPath);
    if (!Input.is_open())
    {
        OutError = "Unable to open file.";
        return false;
    }

    std::ostringstream Buffer;
    Buffer << Input.rdbuf();
    if (Input.bad())
    {
        OutError = "File read failure.";
        return false;
    }

    OutText = Buffer.str();
    return true;
}

std::string EscapeShellDoubleQuoted(const std::string &InValue)
{
    std::string Result;
    Result.reserve(InValue.size() + 2);
    Result.push_back('"');
    for (const char Character : InValue)
    {
        if (Character == '"' || Character == '\\' || Character == '$' ||
            Character == '`')
        {
            Result.push_back('\\');
        }
        Result.push_back(Character);
    }
    Result.push_back('"');
    return Result;
}

bool RunCommandCapture(const std::string &InCommand, std::string &OutStdout,
                       int &OutExitCode)
{
    OutStdout.clear();
    OutExitCode = -1;

#ifdef _WIN32
    FILE *Pipe = _popen(InCommand.c_str(), "r");
#else
    FILE *Pipe = popen(InCommand.c_str(), "r");
#endif
    if (Pipe == nullptr)
    {
        return false;
    }

    char Buffer[256];
    while (fgets(Buffer, sizeof(Buffer), Pipe) != nullptr)
    {
        OutStdout.append(Buffer);
    }

#ifdef _WIN32
    OutExitCode = _pclose(Pipe);
#else
    OutExitCode = pclose(Pipe);
#endif
    return true;
}

std::string GetLastCommitDate(const fs::path &InRepoRoot,
                              const std::string &InRelativePath)
{
#ifdef _WIN32
    const std::string StderrRedirection = " 2>nul";
#else
    const std::string StderrRedirection = " 2>/dev/null";
#endif

    const std::string Command =
        "git -C " + EscapeShellDoubleQuoted(InRepoRoot.string()) +
        " log -1 --format=%cs -- " + EscapeShellDoubleQuoted(InRelativePath) +
        StderrRedirection;

    std::string Output;
    int ExitCode = -1;
    if (!RunCommandCapture(Command, Output, ExitCode))
    {
        return "";
    }
    if (ExitCode != 0)
    {
        return "";
    }
    return Trim(Output);
}

bool IsPathWithinRoot(const fs::path &InPath, const fs::path &InRoot)
{
    const std::string RootString = ToGenericPath(InRoot);
    const std::string PathString = ToGenericPath(InPath);
    if (PathString == RootString)
    {
        return true;
    }

    std::string RootPrefix = RootString;
    if (!RootPrefix.empty() && RootPrefix.back() != '/')
    {
        RootPrefix.push_back('/');
    }
    return PathString.rfind(RootPrefix, 0) == 0;
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

std::string NormalizeSectionId(const std::string &InHeadingText)
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
        Heading.mSectionId = NormalizeSectionId(HeadingText);
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
    std::string CurrentSectionId;
    std::string CurrentHeading;
    int TableId = 1;

    for (size_t Index = 0; Index < InLines.size(); ++Index)
    {
        const int LineNumber = static_cast<int>(Index) + 1;
        while (HeadingIndex < InHeadings.size() &&
               InHeadings[HeadingIndex].mLine <= LineNumber)
        {
            CurrentSectionId = InHeadings[HeadingIndex].mSectionId;
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
        Table.mTableId = TableId;
        Table.mStartLine = LineNumber;
        Table.mSectionId = CurrentSectionId;
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
        TableId += 1;
        Index = (RowIndex == 0) ? Index : RowIndex - 1;
    }

    return Tables;
}

SectionResolution
ResolveSectionByQuery(const std::vector<std::string> &InLines,
                      const std::vector<HeadingRecord> &InHeadings,
                      const std::string &InSectionQuery)
{
    SectionResolution Result;
    Result.mSectionQuery = InSectionQuery;
    const std::string NormalizedQuery = NormalizeSectionId(InSectionQuery);
    const std::string LowerQuery = ToLower(Trim(InSectionQuery));

    for (size_t Index = 0; Index < InHeadings.size(); ++Index)
    {
        const HeadingRecord &Heading = InHeadings[Index];
        if (Heading.mSectionId != NormalizedQuery &&
            ToLower(Heading.mText) != LowerQuery)
        {
            continue;
        }

        Result.mbFound = true;
        Result.mSectionId = Heading.mSectionId;
        Result.mSectionHeading = Heading.mText;
        Result.mLevel = Heading.mLevel;
        Result.mStartLine = Heading.mLine;
        Result.mEndLine = static_cast<int>(InLines.size());
        for (size_t NextIndex = Index + 1; NextIndex < InHeadings.size();
             ++NextIndex)
        {
            if (InHeadings[NextIndex].mLevel <= Heading.mLevel)
            {
                Result.mEndLine = InHeadings[NextIndex].mLine - 1;
                break;
            }
        }
        if (Result.mEndLine < Result.mStartLine)
        {
            Result.mEndLine = Result.mStartLine;
        }
        break;
    }

    return Result;
}

fs::path ResolveDocumentAbsolutePath(const fs::path &InRepoRoot,
                                     const std::string &InDocPath)
{
    if (Trim(InDocPath).empty())
    {
        throw UsageError("Missing value for --doc");
    }

    std::vector<fs::path> Candidates;
    const fs::path InputPath(InDocPath);
    if (InputPath.is_absolute())
    {
        Candidates.push_back(InputPath);
    }
    else
    {
        Candidates.push_back(InRepoRoot / InputPath);
        Candidates.push_back(fs::current_path() / InputPath);
    }

    for (const fs::path &Candidate : Candidates)
    {
        std::error_code Error;
        if (!fs::exists(Candidate, Error) || Error)
        {
            continue;
        }
        if (!fs::is_regular_file(Candidate, Error) || Error)
        {
            continue;
        }

        const fs::path Canonical = fs::weakly_canonical(Candidate, Error);
        if (Error)
        {
            continue;
        }
        if (!IsPathWithinRoot(Canonical, InRepoRoot))
        {
            continue;
        }
        return Canonical;
    }

    throw std::runtime_error("Document does not exist under repo root: " +
                             InDocPath);
}

std::string ToRelativePathOrAbsolute(const fs::path &InAbsolutePath,
                                     const fs::path &InRepoRoot)
{
    try
    {
        return ToGenericPath(fs::relative(InAbsolutePath, InRepoRoot));
    }
    catch (const fs::filesystem_error &)
    {
        return ToGenericPath(InAbsolutePath);
    }
}

std::string NormalizeLookupKey(const std::string &InValue)
{
    std::string Result;
    for (const char Character : InValue)
    {
        const unsigned char AsUnsigned = static_cast<unsigned char>(Character);
        if (std::isalnum(AsUnsigned) != 0)
        {
            Result.push_back(static_cast<char>(std::tolower(AsUnsigned)));
        }
    }
    return Result;
}

std::string ExtractTopicTokenFromInput(const std::string &InTopicValue)
{
    const std::string Trimmed = Trim(InTopicValue);
    if (Trimmed.empty())
    {
        return Trimmed;
    }

    const std::string Filename = fs::path(Trimmed).filename().string();
    static const std::regex PlanRegex(
        R"(^([A-Za-z0-9]+)\.Plan(\.(ChangeLog|Verification))?\.md$)");
    static const std::regex ImplementationRegex(
        R"(^([A-Za-z0-9]+)\.Impl(\.(ChangeLog|Verification))?\.md$)");
    static const std::regex PlaybookRegex(
        R"(^([A-Za-z0-9]+)\.([^.]+)\.Playbook(\.(ChangeLog|Verification))?\.md$)");

    std::smatch Match;
    if (std::regex_match(Filename, Match, PlanRegex) ||
        std::regex_match(Filename, Match, ImplementationRegex) ||
        std::regex_match(Filename, Match, PlaybookRegex))
    {
        return Match[1].str();
    }

    return Trimmed;
}

std::string ResolveTopicKeyFromInventory(const Inventory &InInventory,
                                         const std::string &InTopicValue)
{
    const std::string QueryKey =
        NormalizeLookupKey(ExtractTopicTokenFromInput(InTopicValue));
    if (QueryKey.empty())
    {
        throw UsageError("Missing required option --topic");
    }

    std::unordered_map<std::string, std::string> TopicByKey;
    const auto AddTopic = [&TopicByKey](const std::string &InTopicKey)
    {
        const std::string Normalized = NormalizeLookupKey(InTopicKey);
        if (Normalized.empty())
        {
            return;
        }
        if (TopicByKey.count(Normalized) == 0)
        {
            TopicByKey[Normalized] = InTopicKey;
        }
    };

    for (const DocumentRecord &Record : InInventory.mPlans)
    {
        AddTopic(Record.mTopicKey);
    }
    for (const DocumentRecord &Record : InInventory.mImplementations)
    {
        AddTopic(Record.mTopicKey);
    }
    for (const DocumentRecord &Record : InInventory.mPlaybooks)
    {
        AddTopic(Record.mTopicKey);
    }
    for (const SidecarRecord &Record : InInventory.mSidecars)
    {
        AddTopic(Record.mTopicKey);
    }

    const auto Found = TopicByKey.find(QueryKey);
    if (Found == TopicByKey.end())
    {
        throw UsageError("Unable to resolve topic from --topic value: " +
                         InTopicValue);
    }
    return Found->second;
}

const DocumentRecord *
FindSingleRecordByTopic(const std::vector<DocumentRecord> &InRecords,
                        const std::string &InTopicKey)
{
    for (const DocumentRecord &Record : InRecords)
    {
        if (Record.mTopicKey == InTopicKey)
        {
            return &Record;
        }
    }
    return nullptr;
}

std::vector<DocumentRecord>
CollectRecordsByTopic(const std::vector<DocumentRecord> &InRecords,
                      const std::string &InTopicKey)
{
    std::vector<DocumentRecord> Result;
    for (const DocumentRecord &Record : InRecords)
    {
        if (Record.mTopicKey == InTopicKey)
        {
            Result.push_back(Record);
        }
    }
    SortRecords(Result);
    return Result;
}

std::vector<SidecarRecord>
CollectSidecarsByTopic(const std::vector<SidecarRecord> &InSidecars,
                       const std::string &InTopicKey)
{
    std::vector<SidecarRecord> Result;
    for (const SidecarRecord &Record : InSidecars)
    {
        if (Record.mTopicKey == InTopicKey)
        {
            Result.push_back(Record);
        }
    }
    SortSidecars(Result);
    return Result;
}

std::string ResolvePairStateForTopic(const Inventory &InInventory,
                                     const std::string &InTopicKey)
{
    for (const TopicPairRecord &Pair : InInventory.mPairs)
    {
        if (Pair.mTopicKey == InTopicKey)
        {
            return Pair.mPairState;
        }
    }
    return "unknown";
}

std::vector<SidecarRecord>
FilterSidecars(const std::vector<SidecarRecord> &InSidecars,
               const std::string &InOwnerKind, const std::string &InDocKind,
               const std::string &InPhaseKey)
{
    std::vector<SidecarRecord> Result;
    for (const SidecarRecord &Sidecar : InSidecars)
    {
        if (Sidecar.mOwnerKind != InOwnerKind || Sidecar.mDocKind != InDocKind)
        {
            continue;
        }
        if (!InPhaseKey.empty() && Sidecar.mPhaseKey != InPhaseKey)
        {
            continue;
        }
        Result.push_back(Sidecar);
    }
    SortSidecars(Result);
    return Result;
}

std::vector<SidecarRecord>
ResolveEvidenceSidecars(const std::vector<SidecarRecord> &InTopicSidecars,
                        const std::string &InDocClass,
                        const std::string &InPhaseKey,
                        const std::string &InEvidenceKind)
{
    if (InDocClass == "plan")
    {
        return FilterSidecars(InTopicSidecars, "Plan", InEvidenceKind, "");
    }
    if (InDocClass == "implementation")
    {
        return FilterSidecars(InTopicSidecars, "Impl", InEvidenceKind, "");
    }
    if (InDocClass == "playbook")
    {
        return FilterSidecars(InTopicSidecars, "Playbook", InEvidenceKind,
                              InPhaseKey);
    }
    throw UsageError("Invalid value for --for: " + InDocClass);
}

std::vector<EvidenceEntry> ParseEvidenceEntriesFromFile(
    const fs::path &InAbsolutePath, const std::string &InRelativePath,
    const std::string &InPhaseKey, std::vector<std::string> &OutWarnings)
{
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(InAbsolutePath, Lines, ReadError))
    {
        AddWarning(OutWarnings, "Failed to read sidecar '" + InRelativePath +
                                    "': " + ReadError);
        return {};
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);
    if (Tables.empty())
    {
        AddWarning(OutWarnings, "No markdown tables found in sidecar '" +
                                    InRelativePath + "'.");
        return {};
    }

    std::vector<EvidenceEntry> Entries;
    for (const MarkdownTableRecord &Table : Tables)
    {
        if (Table.mHeaders.empty())
        {
            continue;
        }

        bool HasDateHeader = false;
        for (const std::string &Header : Table.mHeaders)
        {
            if (ToLower(Trim(Header)) == "date")
            {
                HasDateHeader = true;
                break;
            }
        }
        if (!HasDateHeader)
        {
            continue;
        }

        for (size_t RowIndex = 0; RowIndex < Table.mRows.size(); ++RowIndex)
        {
            EvidenceEntry Entry;
            Entry.mSourcePath = InRelativePath;
            Entry.mPhaseKey = InPhaseKey;
            Entry.mTableId = Table.mTableId;
            Entry.mRowIndex = static_cast<int>(RowIndex) + 1;
            for (size_t HeaderIndex = 0; HeaderIndex < Table.mHeaders.size();
                 ++HeaderIndex)
            {
                const std::string Value =
                    (HeaderIndex < Table.mRows[RowIndex].size())
                        ? Table.mRows[RowIndex][HeaderIndex]
                        : "";
                Entry.mFields.emplace_back(Table.mHeaders[HeaderIndex], Value);
            }
            Entries.push_back(std::move(Entry));
        }
    }

    return Entries;
}

bool LooksLikeIsoDate(const std::string &InValue)
{
    static const std::regex IsoDatePattern(R"(^\d{4}-\d{2}-\d{2}$)");
    return std::regex_match(Trim(InValue), IsoDatePattern);
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

int FindHeaderIndex(const std::vector<std::string> &InHeaders,
                    const std::string &InHeaderKey)
{
    const std::string HeaderKey = NormalizeHeaderKey(InHeaderKey);
    for (size_t Index = 0; Index < InHeaders.size(); ++Index)
    {
        if (NormalizeHeaderKey(InHeaders[Index]) == HeaderKey)
        {
            return static_cast<int>(Index);
        }
    }
    return -1;
}

int FindFirstHeaderIndex(const std::vector<std::string> &InHeaders,
                         const std::initializer_list<const char *> &InKeys)
{
    for (const char *Key : InKeys)
    {
        const int HeaderIndex = FindHeaderIndex(InHeaders, Key);
        if (HeaderIndex >= 0)
        {
            return HeaderIndex;
        }
    }
    return -1;
}

std::map<std::string, std::string>
BuildFieldMap(const std::vector<std::pair<std::string, std::string>> &InFields)
{
    std::map<std::string, std::string> Result;
    for (const auto &Field : InFields)
    {
        Result[NormalizeHeaderKey(Field.first)] = Field.second;
    }
    return Result;
}

std::string ClassifyRelativeMarkdownPath(const std::string &InRelativePath,
                                         std::string *const InOutTopicKey,
                                         std::string *const InOutPhaseKey,
                                         std::string *const InOutOwnerKind,
                                         std::string *const InOutDocKind)
{
    const std::string Filename = fs::path(InRelativePath).filename().string();
    DocumentRecord CoreRecord;
    if (TryClassifyCoreDocument(InRelativePath, Filename, CoreRecord))
    {
        if (InOutTopicKey != nullptr)
        {
            *InOutTopicKey = CoreRecord.mTopicKey;
        }
        if (InOutPhaseKey != nullptr)
        {
            *InOutPhaseKey = CoreRecord.mPhaseKey;
        }
        if (InOutOwnerKind != nullptr)
        {
            InOutOwnerKind->clear();
        }
        if (InOutDocKind != nullptr)
        {
            InOutDocKind->clear();
        }

        if (CoreRecord.mKind == EDocumentKind::Plan)
        {
            return "plan";
        }
        if (CoreRecord.mKind == EDocumentKind::Playbook)
        {
            return "playbook";
        }
        return "implementation";
    }

    SidecarRecord Sidecar;
    if (TryClassifySidecarDocument(InRelativePath, Filename, Sidecar))
    {
        if (InOutTopicKey != nullptr)
        {
            *InOutTopicKey = Sidecar.mTopicKey;
        }
        if (InOutPhaseKey != nullptr)
        {
            *InOutPhaseKey = Sidecar.mPhaseKey;
        }
        if (InOutOwnerKind != nullptr)
        {
            *InOutOwnerKind = Sidecar.mOwnerKind;
        }
        if (InOutDocKind != nullptr)
        {
            *InOutDocKind = Sidecar.mDocKind;
        }
        return "sidecar";
    }

    if (InOutTopicKey != nullptr)
    {
        InOutTopicKey->clear();
    }
    if (InOutPhaseKey != nullptr)
    {
        InOutPhaseKey->clear();
    }
    if (InOutOwnerKind != nullptr)
    {
        InOutOwnerKind->clear();
    }
    if (InOutDocKind != nullptr)
    {
        InOutDocKind->clear();
    }
    return "reference";
}

std::string BuildGraphNodeId(const std::string &InType,
                             const std::string &InPath)
{
    return InType + ":" + InPath;
}

std::set<std::string>
ExtractReferencedMarkdownPaths(const fs::path &InRepoRoot,
                               const fs::path &InSourceAbsolutePath,
                               const std::string &InText)
{
    std::set<std::string> Result;
    const fs::path SourceDirectory = InSourceAbsolutePath.parent_path();

    for (std::sregex_iterator
             MatchIt(InText.begin(), InText.end(), kMarkdownPathRegex),
         EndIt;
         MatchIt != EndIt; ++MatchIt)
    {
        std::string Raw = MatchIt->str();
        std::replace(Raw.begin(), Raw.end(), '\\', '/');

        fs::path CandidateAbsolute;
        bool Found = false;
        std::error_code Error;

        const fs::path RootCandidate = InRepoRoot / fs::path(Raw);
        if (fs::exists(RootCandidate, Error) &&
            fs::is_regular_file(RootCandidate, Error))
        {
            CandidateAbsolute = fs::weakly_canonical(RootCandidate, Error);
            Found = !Error;
        }

        if (!Found)
        {
            Error.clear();
            const fs::path LocalCandidate = SourceDirectory / fs::path(Raw);
            if (fs::exists(LocalCandidate, Error) &&
                fs::is_regular_file(LocalCandidate, Error))
            {
                CandidateAbsolute = fs::weakly_canonical(LocalCandidate, Error);
                Found = !Error;
            }
        }

        if (!Found || !IsPathWithinRoot(CandidateAbsolute, InRepoRoot))
        {
            continue;
        }

        try
        {
            Result.insert(
                ToGenericPath(fs::relative(CandidateAbsolute, InRepoRoot)));
        }
        catch (const fs::filesystem_error &)
        {
            continue;
        }
    }

    return Result;
}

std::string ToDocClassFromOwnerKind(const std::string &InOwnerKind)
{
    if (InOwnerKind == "Plan")
    {
        return "plan";
    }
    if (InOwnerKind == "Impl")
    {
        return "implementation";
    }
    if (InOwnerKind == "Playbook")
    {
        return "playbook";
    }
    return "unknown";
}

std::string
DerivePlaybookPairState(const DocumentRecord &InPlaybook,
                        const std::set<std::string> &InPlanTopics,
                        const std::set<std::string> &InImplementationTopics)
{
    const bool HasPlan = InPlanTopics.count(InPlaybook.mTopicKey) > 0;
    const bool HasImplementation =
        InImplementationTopics.count(InPlaybook.mTopicKey) > 0;
    if (HasPlan && HasImplementation)
    {
        return "paired";
    }
    if (HasPlan)
    {
        return "missing_implementation";
    }
    return "orphan_playbook";
}

// ResolvedDocument moved to DocTypes.h

ResolvedDocument ReadAndParseDocument(const BaseOptions &InOptions,
                                      const std::string &InDocPath)
{
    ResolvedDocument Result;
    Result.mRepoRoot = NormalizeRepoRootPath(InOptions.mRepoRoot);
    Result.mAbsolutePath =
        ResolveDocumentAbsolutePath(Result.mRepoRoot, InDocPath);
    Result.mRelativePath =
        ToRelativePathOrAbsolute(Result.mAbsolutePath, Result.mRepoRoot);

    std::string ReadError;
    if (!TryReadFileLines(Result.mAbsolutePath, Result.mLines, ReadError))
    {
        throw std::runtime_error("Failed to read document '" +
                                 Result.mRelativePath + "': " + ReadError);
    }

    Result.mHeadings = ParseHeadingRecords(Result.mLines);
    return Result;
}

// ---------------------------------------------------------------------------
// Runtime schema parsing from *.Schema.md files
// ---------------------------------------------------------------------------

std::vector<SectionSchemaEntry>
TryParseSectionSchemaFromFile(const fs::path &InSchemaPath)
{
    std::vector<SectionSchemaEntry> Entries;

    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(InSchemaPath, Lines, ReadError))
    {
        return Entries;
    }

    // Find ## canonical_section_order heading
    int TableStart = -1;
    for (size_t LineIndex = 0; LineIndex < Lines.size(); ++LineIndex)
    {
        const std::string &Line = Lines[LineIndex];
        if (Line.size() >= 3 && Line[0] == '#' && Line[1] == '#' &&
            Line[2] == ' ')
        {
            std::string HeadingText = Trim(Line.substr(3));
            std::transform(HeadingText.begin(), HeadingText.end(),
                           HeadingText.begin(), [](unsigned char InChar)
                           { return static_cast<char>(std::tolower(InChar)); });
            std::replace(HeadingText.begin(), HeadingText.end(), ' ', '_');
            if (HeadingText == "canonical_section_order")
            {
                TableStart = static_cast<int>(LineIndex) + 1;
                break;
            }
        }
    }

    if (TableStart < 0)
    {
        return Entries;
    }

    // Skip blank lines until we find the table header
    size_t Cursor = static_cast<size_t>(TableStart);
    while (Cursor < Lines.size() && Trim(Lines[Cursor]).empty())
    {
        ++Cursor;
    }

    if (Cursor >= Lines.size() || Lines[Cursor].find('|') == std::string::npos)
    {
        return Entries;
    }

    // Parse header row to find column indices
    const std::string &HeaderLine = Lines[Cursor];
    std::vector<std::string> Headers;
    {
        std::istringstream Stream(HeaderLine);
        std::string Cell;
        while (std::getline(Stream, Cell, '|'))
        {
            std::string Trimmed = Trim(Cell);
            if (!Trimmed.empty())
            {
                std::transform(
                    Trimmed.begin(), Trimmed.end(), Trimmed.begin(),
                    [](unsigned char InChar)
                    { return static_cast<char>(std::tolower(InChar)); });
                Headers.push_back(Trimmed);
            }
        }
    }

    int OrderCol = -1;
    int SectionIdCol = -1;
    int RequirementCol = -1;
    for (size_t ColIndex = 0; ColIndex < Headers.size(); ++ColIndex)
    {
        if (Headers[ColIndex] == "order")
        {
            OrderCol = static_cast<int>(ColIndex);
        }
        else if (Headers[ColIndex] == "section id")
        {
            SectionIdCol = static_cast<int>(ColIndex);
        }
        else if (Headers[ColIndex] == "requirement")
        {
            RequirementCol = static_cast<int>(ColIndex);
        }
    }

    if (SectionIdCol < 0)
    {
        return Entries;
    }

    // Skip separator row (--- | --- | ---)
    ++Cursor;
    if (Cursor < Lines.size() && Lines[Cursor].find("---") != std::string::npos)
    {
        ++Cursor;
    }

    // Parse data rows
    while (Cursor < Lines.size())
    {
        const std::string &RowLine = Lines[Cursor];
        if (RowLine.find('|') == std::string::npos || Trim(RowLine).empty())
        {
            break; // End of table
        }

        std::vector<std::string> Cells;
        {
            std::istringstream Stream(RowLine);
            std::string Cell;
            while (std::getline(Stream, Cell, '|'))
            {
                std::string Trimmed = Trim(Cell);
                if (!Trimmed.empty() || Cells.size() > 0)
                {
                    Cells.push_back(Trimmed);
                }
            }
            // Remove trailing empty cell from "| a | b | c |" pattern
            if (!Cells.empty() && Cells.back().empty())
            {
                Cells.pop_back();
            }
        }

        if (static_cast<int>(Cells.size()) <= SectionIdCol)
        {
            ++Cursor;
            continue;
        }

        SectionSchemaEntry Entry;

        // Extract section ID (strip backticks)
        std::string RawId = Cells[static_cast<size_t>(SectionIdCol)];
        RawId.erase(std::remove(RawId.begin(), RawId.end(), '`'), RawId.end());
        Entry.mSectionId = Trim(RawId);

        // Extract order
        if (OrderCol >= 0 && static_cast<int>(Cells.size()) > OrderCol)
        {
            try
            {
                Entry.mOrder =
                    std::stoi(Trim(Cells[static_cast<size_t>(OrderCol)]));
            }
            catch (...)
            {
                Entry.mOrder = static_cast<int>(Entries.size()) + 1;
            }
        }
        else
        {
            Entry.mOrder = static_cast<int>(Entries.size()) + 1;
        }

        // Extract requirement — only exact "required" maps to
        // required=true
        if (RequirementCol >= 0 &&
            static_cast<int>(Cells.size()) > RequirementCol)
        {
            std::string Req = Trim(Cells[static_cast<size_t>(RequirementCol)]);
            std::transform(Req.begin(), Req.end(), Req.begin(),
                           [](unsigned char InChar)
                           { return static_cast<char>(std::tolower(InChar)); });
            Entry.mbRequired = (Req == "required");
        }

        if (!Entry.mSectionId.empty())
        {
            Entries.push_back(std::move(Entry));
        }
        ++Cursor;
    }

    return Entries;
}

} // namespace UniPlan
