#include "UniPlanBundleIndex.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHashHelpers.h"
#include "UniPlanHelpers.h"

#include <algorithm>
#include <regex>

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

bool TryBuildBundleFileIndex(const fs::path &InRepoRoot,
                             FBundleFileIndexResult &OutIndex,
                             std::string &OutError)
{
    OutIndex = FBundleFileIndexResult{};
    static const std::regex BundleRegex(R"(^([A-Za-z0-9]+)\.Plan\.json$)");

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

        const std::string Filename = AbsolutePath.filename().string();
        std::smatch Match;
        if (std::regex_match(Filename, Match, BundleRegex))
        {
            FBundleFileIndexEntry Indexed;
            std::string FingerprintError;
            if (TryMakeFingerprint(Entry, InRepoRoot, Indexed.mFingerprint,
                                   FingerprintError))
            {
                Indexed.mTopicKey = Match[1].str();
                OutIndex.mBundles.push_back(std::move(Indexed));
            }
            else
            {
                OutIndex.mWarnings.push_back(FingerprintError);
            }
        }

        AdvanceIterator();
        if (!OutError.empty())
        {
            return false;
        }
    }

    std::sort(OutIndex.mBundles.begin(), OutIndex.mBundles.end(),
              [](const FBundleFileIndexEntry &InLeft,
                 const FBundleFileIndexEntry &InRight)
              {
                  return InLeft.mTopicKey == InRight.mTopicKey
                             ? InLeft.mFingerprint.mRelativePath <
                                   InRight.mFingerprint.mRelativePath
                             : InLeft.mTopicKey < InRight.mTopicKey;
              });

    std::vector<FFileFingerprint> Files;
    Files.reserve(OutIndex.mBundles.size());
    for (const FBundleFileIndexEntry &Entry : OutIndex.mBundles)
    {
        Files.push_back(Entry.mFingerprint);
    }
    OutIndex.mSignature = ComputeFingerprintSignature(InRepoRoot, Files);
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

        if (bIsRegular && AbsolutePath.extension() == ".md")
        {
            FFileFingerprint Fingerprint;
            std::string FingerprintError;
            if (TryMakeFingerprint(Entry, InRepoRoot, Fingerprint,
                                   FingerprintError))
            {
                OutIndex.mFiles.push_back(std::move(Fingerprint));
            }
            else
            {
                OutIndex.mWarnings.push_back(FingerprintError);
            }
        }

        AdvanceIterator();
        if (!OutError.empty())
        {
            return false;
        }
    }

    std::sort(
        OutIndex.mFiles.begin(), OutIndex.mFiles.end(),
        [](const FFileFingerprint &InLeft, const FFileFingerprint &InRight)
        { return InLeft.mRelativePath < InRight.mRelativePath; });
    OutIndex.mSignature =
        ComputeFingerprintSignature(InRepoRoot, OutIndex.mFiles);
    return true;
}

} // namespace UniPlan
