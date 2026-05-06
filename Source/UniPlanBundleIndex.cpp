#include "UniPlanBundleIndex.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHashHelpers.h"
#include "UniPlanHelpers.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace UniPlan
{

bool operator==(const FFileFingerprint &InLeft, const FFileFingerprint &InRight)
{
    return InLeft.mPath == InRight.mPath &&
           InLeft.mRelativePath == InRight.mRelativePath &&
           InLeft.mWriteTime == InRight.mWriteTime &&
           InLeft.mFileSize == InRight.mFileSize;
}

bool operator!=(const FFileFingerprint &InLeft, const FFileFingerprint &InRight)
{
    return !(InLeft == InRight);
}

static std::string NormalizeAbsolutePath(const fs::path &InPath)
{
    std::error_code Error;
    const fs::path Canonical = fs::weakly_canonical(InPath, Error);
    return ToGenericPath(Error ? InPath : Canonical);
}

static bool TryMakeFingerprint(const fs::directory_entry &InEntry,
                               const fs::path &InRepoRoot,
                               FFileFingerprint &OutFingerprint,
                               std::string &OutError)
{
    const fs::path AbsolutePath = InEntry.path();

    fs::path RelativePath;
    try
    {
        RelativePath = fs::relative(AbsolutePath, InRepoRoot);
    }
    catch (const fs::filesystem_error &InError)
    {
        OutError = "Fingerprint relative-path resolution failed for '" +
                   AbsolutePath.string() + "': " + InError.what();
        return false;
    }

    std::error_code WriteTimeError;
    const fs::file_time_type WriteTime =
        fs::last_write_time(AbsolutePath, WriteTimeError);
    if (WriteTimeError)
    {
        OutError = "Fingerprint write-time read failed for '" +
                   AbsolutePath.string() + "': " + WriteTimeError.message();
        return false;
    }

    std::error_code FileSizeError;
    const uint64_t FileSize =
        static_cast<uint64_t>(InEntry.file_size(FileSizeError));
    if (FileSizeError)
    {
        OutError = "Fingerprint file-size read failed for '" +
                   AbsolutePath.string() + "': " + FileSizeError.message();
        return false;
    }

    OutFingerprint.mPath = NormalizeAbsolutePath(AbsolutePath);
    OutFingerprint.mRelativePath = ToGenericPath(RelativePath);
    OutFingerprint.mWriteTime =
        static_cast<uint64_t>(WriteTime.time_since_epoch().count());
    OutFingerprint.mFileSize = FileSize;
    return true;
}

static uint64_t
ComputeFingerprintSignature(const fs::path &InRepoRoot,
                            const std::vector<FFileFingerprint> &InFiles)
{
    uint64_t HashState = kFnv1aSeed;
    Fnv1aUpdateString(HashState, ToGenericPath(InRepoRoot));
    Fnv1aUpdateUint64(HashState, static_cast<uint64_t>(InFiles.size()));
    for (const FFileFingerprint &File : InFiles)
    {
        Fnv1aUpdateString(HashState, File.mRelativePath);
        Fnv1aUpdateByte(HashState, 0x1F);
        Fnv1aUpdateUint64(HashState, File.mWriteTime);
        Fnv1aUpdateByte(HashState, 0x1E);
        Fnv1aUpdateUint64(HashState, File.mFileSize);
        Fnv1aUpdateByte(HashState, 0x1D);
    }
    return HashState;
}

static bool TryParseBundleFilename(const std::string &InFilename,
                                   std::string &OutTopicKey)
{
    static const std::string Suffix = ".Plan.json";
    if (InFilename.size() <= Suffix.size() ||
        InFilename.compare(InFilename.size() - Suffix.size(), Suffix.size(),
                           Suffix) != 0)
    {
        return false;
    }

    const std::string TopicKey =
        InFilename.substr(0, InFilename.size() - Suffix.size());
    if (TopicKey.empty())
    {
        return false;
    }

    for (const char Character : TopicKey)
    {
        if (!std::isalnum(static_cast<unsigned char>(Character)))
        {
            return false;
        }
    }

    OutTopicKey = TopicKey;
    return true;
}

static void SortBundleIndex(FBundleFileIndexResult &InOutIndex)
{
    std::sort(InOutIndex.mBundles.begin(), InOutIndex.mBundles.end(),
              [](const FBundleFileIndexEntry &InLeft,
                 const FBundleFileIndexEntry &InRight)
              {
                  return InLeft.mTopicKey == InRight.mTopicKey
                             ? InLeft.mFingerprint.mRelativePath <
                                   InRight.mFingerprint.mRelativePath
                             : InLeft.mTopicKey < InRight.mTopicKey;
              });
}

static void SortMarkdownIndex(FMarkdownFileIndexResult &InOutIndex)
{
    std::sort(
        InOutIndex.mFiles.begin(), InOutIndex.mFiles.end(),
        [](const FFileFingerprint &InLeft, const FFileFingerprint &InRight)
        { return InLeft.mRelativePath < InRight.mRelativePath; });
}

static void FinalizeBundleIndex(const fs::path &InRepoRoot,
                                FBundleFileIndexResult &InOutIndex)
{
    SortBundleIndex(InOutIndex);

    std::vector<FFileFingerprint> Files;
    Files.reserve(InOutIndex.mBundles.size());
    for (const FBundleFileIndexEntry &Entry : InOutIndex.mBundles)
    {
        Files.push_back(Entry.mFingerprint);
    }
    InOutIndex.mSignature = ComputeFingerprintSignature(InRepoRoot, Files);
}

static void FinalizeMarkdownIndex(const fs::path &InRepoRoot,
                                  FMarkdownFileIndexResult &InOutIndex)
{
    SortMarkdownIndex(InOutIndex);
    InOutIndex.mSignature =
        ComputeFingerprintSignature(InRepoRoot, InOutIndex.mFiles);
}

static void TryAddBundleFile(const fs::directory_entry &InEntry,
                             const fs::path &InRepoRoot,
                             FBundleFileIndexResult &InOutIndex)
{
    std::string TopicKey;
    if (!TryParseBundleFilename(InEntry.path().filename().string(), TopicKey))
    {
        return;
    }

    FBundleFileIndexEntry Indexed;
    std::string FingerprintError;
    if (TryMakeFingerprint(InEntry, InRepoRoot, Indexed.mFingerprint,
                           FingerprintError))
    {
        Indexed.mTopicKey = std::move(TopicKey);
        InOutIndex.mBundles.push_back(std::move(Indexed));
    }
    else
    {
        InOutIndex.mWarnings.push_back(FingerprintError);
    }
}

static void TryAddMarkdownFile(const fs::directory_entry &InEntry,
                               const fs::path &InRepoRoot,
                               FMarkdownFileIndexResult &InOutIndex)
{
    if (InEntry.path().extension() != ".md")
    {
        return;
    }

    FFileFingerprint Fingerprint;
    std::string FingerprintError;
    if (TryMakeFingerprint(InEntry, InRepoRoot, Fingerprint, FingerprintError))
    {
        InOutIndex.mFiles.push_back(std::move(Fingerprint));
    }
    else
    {
        InOutIndex.mWarnings.push_back(FingerprintError);
    }
}

bool TryBuildBundleFileIndex(const fs::path &InRepoRoot,
                             FBundleFileIndexResult &OutIndex,
                             std::string &OutError)
{
    OutIndex = FBundleFileIndexResult{};

    const fs::directory_options IteratorOptions =
        fs::directory_options::skip_permission_denied;
    std::error_code Error;
    fs::recursive_directory_iterator Iterator(InRepoRoot, IteratorOptions,
                                              Error);
    fs::recursive_directory_iterator EndIterator;
    if (Error)
    {
        OutError =
            "Bundle index traversal initialization failed: " + Error.message();
        return false;
    }

    while (Iterator != EndIterator)
    {
        const fs::directory_entry Entry = *Iterator;
        const fs::path AbsolutePath = Entry.path();
        const auto AdvanceIterator =
            [&Iterator, &EndIterator, &OutError, &AbsolutePath]()
        {
            std::error_code AdvanceError;
            Iterator.increment(AdvanceError);
            if (AdvanceError)
            {
                OutError = "Bundle index traversal advance failed at '" +
                           AbsolutePath.string() +
                           "': " + AdvanceError.message();
                Iterator = EndIterator;
            }
        };

        std::error_code TypeError;
        const bool bIsDirectory = Entry.is_directory(TypeError);
        if (TypeError)
        {
            OutIndex.mWarnings.push_back(
                "Bundle index directory-type read failed for '" +
                AbsolutePath.string() + "': " + TypeError.message());
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        if (bIsDirectory && ShouldSkipRecursionDirectory(AbsolutePath))
        {
            Iterator.disable_recursion_pending();
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        const bool bIsRegular = Entry.is_regular_file(TypeError);
        if (TypeError)
        {
            OutIndex.mWarnings.push_back(
                "Bundle index file-type read failed for '" +
                AbsolutePath.string() + "': " + TypeError.message());
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        if (!bIsRegular)
        {
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        TryAddBundleFile(Entry, InRepoRoot, OutIndex);

        AdvanceIterator();
        if (!OutError.empty())
        {
            return false;
        }
    }

    FinalizeBundleIndex(InRepoRoot, OutIndex);
    return true;
}

bool TryBuildMarkdownFileIndex(const fs::path &InRepoRoot,
                               FMarkdownFileIndexResult &OutIndex,
                               std::string &OutError)
{
    OutIndex = FMarkdownFileIndexResult{};
    const fs::directory_options IteratorOptions =
        fs::directory_options::skip_permission_denied;
    std::error_code Error;
    fs::recursive_directory_iterator Iterator(InRepoRoot, IteratorOptions,
                                              Error);
    fs::recursive_directory_iterator EndIterator;
    if (Error)
    {
        OutError = "Markdown index traversal initialization failed: " +
                   Error.message();
        return false;
    }

    while (Iterator != EndIterator)
    {
        const fs::directory_entry Entry = *Iterator;
        const fs::path AbsolutePath = Entry.path();
        const auto AdvanceIterator =
            [&Iterator, &EndIterator, &OutError, &AbsolutePath]()
        {
            std::error_code AdvanceError;
            Iterator.increment(AdvanceError);
            if (AdvanceError)
            {
                OutError = "Markdown index traversal advance failed at '" +
                           AbsolutePath.string() +
                           "': " + AdvanceError.message();
                Iterator = EndIterator;
            }
        };

        std::error_code TypeError;
        const bool bIsDirectory = Entry.is_directory(TypeError);
        if (TypeError)
        {
            OutIndex.mWarnings.push_back(
                "Markdown index directory-type read failed for '" +
                AbsolutePath.string() + "': " + TypeError.message());
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        if (bIsDirectory && ShouldSkipRecursionDirectory(AbsolutePath))
        {
            Iterator.disable_recursion_pending();
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        const bool bIsRegular = Entry.is_regular_file(TypeError);
        if (TypeError)
        {
            OutIndex.mWarnings.push_back(
                "Markdown index file-type read failed for '" +
                AbsolutePath.string() + "': " + TypeError.message());
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        if (bIsRegular)
        {
            TryAddMarkdownFile(Entry, InRepoRoot, OutIndex);
        }

        AdvanceIterator();
        if (!OutError.empty())
        {
            return false;
        }
    }

    FinalizeMarkdownIndex(InRepoRoot, OutIndex);
    return true;
}

bool TryBuildWatchFileIndex(const fs::path &InRepoRoot,
                            FWatchFileIndexResult &OutIndex,
                            std::string &OutError)
{
    OutIndex = FWatchFileIndexResult{};

    const fs::directory_options IteratorOptions =
        fs::directory_options::skip_permission_denied;
    std::error_code Error;
    fs::recursive_directory_iterator Iterator(InRepoRoot, IteratorOptions,
                                              Error);
    fs::recursive_directory_iterator EndIterator;
    if (Error)
    {
        OutError =
            "Watch index traversal initialization failed: " + Error.message();
        return false;
    }

    while (Iterator != EndIterator)
    {
        const fs::directory_entry Entry = *Iterator;
        const fs::path AbsolutePath = Entry.path();
        const auto AdvanceIterator =
            [&Iterator, &EndIterator, &OutError, &AbsolutePath]()
        {
            std::error_code AdvanceError;
            Iterator.increment(AdvanceError);
            if (AdvanceError)
            {
                OutError = "Watch index traversal advance failed at '" +
                           AbsolutePath.string() +
                           "': " + AdvanceError.message();
                Iterator = EndIterator;
            }
        };

        std::error_code TypeError;
        const bool bIsDirectory = Entry.is_directory(TypeError);
        if (TypeError)
        {
            const std::string Warning =
                "Watch index directory-type read failed for '" +
                AbsolutePath.string() + "': " + TypeError.message();
            OutIndex.mBundleIndex.mWarnings.push_back(Warning);
            OutIndex.mMarkdownIndex.mWarnings.push_back(Warning);
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        if (bIsDirectory && ShouldSkipRecursionDirectory(AbsolutePath))
        {
            Iterator.disable_recursion_pending();
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        const bool bIsRegular = Entry.is_regular_file(TypeError);
        if (TypeError)
        {
            const std::string Warning =
                "Watch index file-type read failed for '" +
                AbsolutePath.string() + "': " + TypeError.message();
            OutIndex.mBundleIndex.mWarnings.push_back(Warning);
            OutIndex.mMarkdownIndex.mWarnings.push_back(Warning);
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        if (bIsRegular)
        {
            TryAddBundleFile(Entry, InRepoRoot, OutIndex.mBundleIndex);
            TryAddMarkdownFile(Entry, InRepoRoot, OutIndex.mMarkdownIndex);
        }

        AdvanceIterator();
        if (!OutError.empty())
        {
            return false;
        }
    }

    FinalizeBundleIndex(InRepoRoot, OutIndex.mBundleIndex);
    FinalizeMarkdownIndex(InRepoRoot, OutIndex.mMarkdownIndex);
    return true;
}

static void RefreshCachedBundlePath(
    const fs::path &InRepoRoot, const std::string &InPath,
    FBundleFileIndexResult &InOutIndex)
{
    std::error_code StatusError;
    if (!fs::is_regular_file(InPath, StatusError) || StatusError)
    {
        return;
    }

    const fs::directory_entry Entry(InPath);
    TryAddBundleFile(Entry, InRepoRoot, InOutIndex);
}

static void RefreshCachedMarkdownPath(
    const fs::path &InRepoRoot, const std::string &InPath,
    FMarkdownFileIndexResult &InOutIndex, std::set<std::string> &InOutSeenPaths)
{
    std::error_code StatusError;
    if (!fs::is_regular_file(InPath, StatusError) || StatusError)
    {
        return;
    }

    const fs::directory_entry Entry(InPath);
    FMarkdownFileIndexResult Candidate;
    TryAddMarkdownFile(Entry, InRepoRoot, Candidate);
    for (FFileFingerprint &File : Candidate.mFiles)
    {
        if (InOutSeenPaths.insert(File.mPath).second)
        {
            InOutIndex.mFiles.push_back(std::move(File));
        }
    }
    InOutIndex.mWarnings.insert(InOutIndex.mWarnings.end(),
                                Candidate.mWarnings.begin(),
                                Candidate.mWarnings.end());
}

static void RefreshCanonicalPlanDirectory(
    const fs::path &InRepoRoot, FBundleFileIndexResult &InOutIndex,
    std::set<std::string> &InOutSeenPaths, std::string &OutError)
{
    const fs::path PlansDir = InRepoRoot / "Docs" / "Plans";
    std::error_code ExistsError;
    if (!fs::exists(PlansDir, ExistsError) || ExistsError)
    {
        return;
    }

    std::error_code IteratorError;
    fs::directory_iterator Iterator(PlansDir, IteratorError);
    fs::directory_iterator EndIterator;
    if (IteratorError)
    {
        InOutIndex.mWarnings.push_back(
            "Watch fast index Docs/Plans scan failed: " +
            IteratorError.message());
        return;
    }

    while (Iterator != EndIterator)
    {
        const fs::directory_entry Entry = *Iterator;
        std::error_code TypeError;
        if (Entry.is_regular_file(TypeError) && !TypeError)
        {
            FBundleFileIndexResult Candidate;
            TryAddBundleFile(Entry, InRepoRoot, Candidate);
            for (FBundleFileIndexEntry &Bundle : Candidate.mBundles)
            {
                if (InOutSeenPaths.insert(Bundle.mFingerprint.mPath).second)
                {
                    InOutIndex.mBundles.push_back(std::move(Bundle));
                }
            }
            InOutIndex.mWarnings.insert(InOutIndex.mWarnings.end(),
                                        Candidate.mWarnings.begin(),
                                        Candidate.mWarnings.end());
        }

        std::error_code AdvanceError;
        Iterator.increment(AdvanceError);
        if (AdvanceError)
        {
            OutError = "Watch fast index Docs/Plans advance failed: " +
                       AdvanceError.message();
            return;
        }
    }
}

static void RefreshDocsMarkdownDirectory(
    const fs::path &InRepoRoot, FMarkdownFileIndexResult &InOutIndex,
    std::set<std::string> &InOutSeenPaths, std::string &OutError)
{
    const fs::path DocsDir = InRepoRoot / "Docs";
    std::error_code ExistsError;
    if (!fs::exists(DocsDir, ExistsError) || ExistsError)
    {
        return;
    }

    const fs::directory_options IteratorOptions =
        fs::directory_options::skip_permission_denied;
    std::error_code IteratorError;
    fs::recursive_directory_iterator Iterator(DocsDir, IteratorOptions,
                                              IteratorError);
    fs::recursive_directory_iterator EndIterator;
    if (IteratorError)
    {
        InOutIndex.mWarnings.push_back("Watch fast index Docs scan failed: " +
                                       IteratorError.message());
        return;
    }

    while (Iterator != EndIterator)
    {
        const fs::directory_entry Entry = *Iterator;
        const fs::path AbsolutePath = Entry.path();
        const auto AdvanceIterator =
            [&Iterator, &EndIterator, &OutError, &AbsolutePath]()
        {
            std::error_code AdvanceError;
            Iterator.increment(AdvanceError);
            if (AdvanceError)
            {
                OutError = "Watch fast index Docs advance failed at '" +
                           AbsolutePath.string() +
                           "': " + AdvanceError.message();
                Iterator = EndIterator;
            }
        };

        std::error_code TypeError;
        if (Entry.is_directory(TypeError) && !TypeError &&
            ShouldSkipRecursionDirectory(AbsolutePath))
        {
            Iterator.disable_recursion_pending();
            AdvanceIterator();
            if (!OutError.empty())
            {
                return;
            }
            continue;
        }

        if (Entry.is_regular_file(TypeError) && !TypeError)
        {
            FMarkdownFileIndexResult Candidate;
            TryAddMarkdownFile(Entry, InRepoRoot, Candidate);
            for (FFileFingerprint &File : Candidate.mFiles)
            {
                if (InOutSeenPaths.insert(File.mPath).second)
                {
                    InOutIndex.mFiles.push_back(std::move(File));
                }
            }
            InOutIndex.mWarnings.insert(InOutIndex.mWarnings.end(),
                                        Candidate.mWarnings.begin(),
                                        Candidate.mWarnings.end());
        }

        AdvanceIterator();
        if (!OutError.empty())
        {
            return;
        }
    }
}

bool TryRefreshWatchFileIndexFast(const fs::path &InRepoRoot,
                                  const FWatchFileIndexResult &InCachedIndex,
                                  FWatchFileIndexResult &OutIndex,
                                  std::string &OutError)
{
    OutIndex = FWatchFileIndexResult{};

    std::set<std::string> SeenBundlePaths;
    for (const FBundleFileIndexEntry &Entry :
         InCachedIndex.mBundleIndex.mBundles)
    {
        RefreshCachedBundlePath(InRepoRoot, Entry.mFingerprint.mPath,
                                OutIndex.mBundleIndex);
    }
    for (const FBundleFileIndexEntry &Entry : OutIndex.mBundleIndex.mBundles)
    {
        SeenBundlePaths.insert(Entry.mFingerprint.mPath);
    }

    RefreshCanonicalPlanDirectory(InRepoRoot, OutIndex.mBundleIndex,
                                  SeenBundlePaths, OutError);
    if (!OutError.empty())
    {
        return false;
    }

    std::set<std::string> SeenMarkdownPaths;
    for (const FFileFingerprint &File : InCachedIndex.mMarkdownIndex.mFiles)
    {
        RefreshCachedMarkdownPath(InRepoRoot, File.mPath,
                                  OutIndex.mMarkdownIndex,
                                  SeenMarkdownPaths);
    }
    RefreshDocsMarkdownDirectory(InRepoRoot, OutIndex.mMarkdownIndex,
                                 SeenMarkdownPaths, OutError);
    if (!OutError.empty())
    {
        return false;
    }

    FinalizeBundleIndex(InRepoRoot, OutIndex.mBundleIndex);
    FinalizeMarkdownIndex(InRepoRoot, OutIndex.mMarkdownIndex);
    return true;
}

} // namespace UniPlan
