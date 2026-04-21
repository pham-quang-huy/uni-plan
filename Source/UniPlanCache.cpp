#include "UniPlanTypes.h"
#include "UniPlanHashHelpers.h"
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

fs::path ResolveExecutableDirectory()
{
#ifdef __APPLE__
    uint32_t BufferSize = 1024u;
    std::vector<char> ExePath(static_cast<size_t>(BufferSize), '\0');
    if (_NSGetExecutablePath(ExePath.data(), &BufferSize) != 0)
    {
        ExePath.assign(static_cast<size_t>(BufferSize), '\0');
        if (_NSGetExecutablePath(ExePath.data(), &BufferSize) != 0)
        {
            return fs::current_path();
        }
    }

    std::error_code Error;
    const fs::path RawPath(ExePath.data());
    const fs::path CanonicalPath = fs::weakly_canonical(RawPath, Error);
    const fs::path ExeDir = (Error ? RawPath : CanonicalPath).parent_path();
    return ExeDir.empty() ? fs::current_path() : ExeDir;

#elif defined(_WIN32)
    wchar_t Buffer[MAX_PATH];
    const DWORD Length = GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
    if (Length == 0 || Length >= MAX_PATH)
    {
        return fs::current_path();
    }

    const fs::path ExeDir = fs::path(Buffer).parent_path();
    return ExeDir.empty() ? fs::current_path() : ExeDir;

#else
    return fs::current_path();
#endif
}

// ---------------------------------------------------------------------------
// INI parser and configuration
// ---------------------------------------------------------------------------

std::string ExpandEnvVars(const std::string &InValue)
{
    std::string Result;
    Result.reserve(InValue.size());
    size_t Pos = 0;

    while (Pos < InValue.size())
    {
        if (InValue[Pos] == '$' && Pos + 1 < InValue.size() &&
            InValue[Pos + 1] == '{')
        {
            const size_t ClosePos = InValue.find('}', Pos + 2);
            if (ClosePos != std::string::npos)
            {
                const std::string VarName =
                    InValue.substr(Pos + 2, ClosePos - Pos - 2);
                const char *VarValue = std::getenv(VarName.c_str());
                if (VarValue != nullptr)
                {
                    Result += VarValue;
                }
                Pos = ClosePos + 1;
                continue;
            }
        }
        Result += InValue[Pos];
        ++Pos;
    }

    return Result;
}

// IniData typedef moved to DocTypes.h

IniData ParseIniFile(const fs::path &InPath)
{
    IniData Sections;
    std::ifstream File(InPath);
    if (!File.is_open())
    {
        return Sections;
    }

    std::string CurrentSection;
    std::string Line;
    while (std::getline(File, Line))
    {
        // Trim leading whitespace
        const size_t Start = Line.find_first_not_of(" \t");
        if (Start == std::string::npos)
        {
            continue; // Empty line
        }
        Line = Line.substr(Start);

        // Comment
        if (Line[0] == '#' || Line[0] == ';')
        {
            continue;
        }

        // Section header
        if (Line[0] == '[')
        {
            const size_t End = Line.find(']');
            if (End != std::string::npos)
            {
                CurrentSection = Line.substr(1, End - 1);
            }
            continue;
        }

        // Key = Value
        const size_t EqPos = Line.find('=');
        if (EqPos == std::string::npos)
        {
            continue; // Malformed line, skip
        }

        std::string Key = Line.substr(0, EqPos);
        std::string Value = Line.substr(EqPos + 1);

        // Trim
        const size_t KeyEnd = Key.find_last_not_of(" \t");
        Key = (KeyEnd == std::string::npos) ? "" : Key.substr(0, KeyEnd + 1);

        const size_t ValueStart = Value.find_first_not_of(" \t");
        Value =
            (ValueStart == std::string::npos) ? "" : Value.substr(ValueStart);
        const size_t ValueEnd = Value.find_last_not_of(" \t\r\n");
        Value = (ValueEnd == std::string::npos) ? ""
                                                : Value.substr(0, ValueEnd + 1);

        if (!Key.empty())
        {
            Sections[CurrentSection][Key] = Value;
        }
    }

    return Sections;
}

// DocConfig moved to DocTypes.h

DocConfig LoadConfig(const fs::path &InExeDir)
{
    DocConfig Config;
    const fs::path IniPath = InExeDir / "uni-plan.ini";

    const IniData Data = ParseIniFile(IniPath);
    const auto CacheIt = Data.find("cache");
    if (CacheIt != Data.end())
    {
        const auto DirIt = CacheIt->second.find("dir");
        if (DirIt != CacheIt->second.end() && !DirIt->second.empty())
        {
            Config.mCacheDir = ExpandEnvVars(DirIt->second);
        }

        const auto EnabledIt = CacheIt->second.find("enabled");
        if (EnabledIt != CacheIt->second.end())
        {
            std::string Val = EnabledIt->second;
            std::transform(Val.begin(), Val.end(), Val.begin(), ::tolower);
            Config.mbCacheEnabled =
                (Val != "false" && Val != "0" && Val != "no");
        }

        const auto VerboseIt = CacheIt->second.find("verbose");
        if (VerboseIt != CacheIt->second.end())
        {
            std::string Val = VerboseIt->second;
            std::transform(Val.begin(), Val.end(), Val.begin(), ::tolower);
            Config.mbCacheVerbose =
                (Val == "true" || Val == "1" || Val == "yes");
        }
    }

    return Config;
}

// FNV-1a helpers and ToHexString moved to UniPlanHashHelpers.h (inline).
// Kept here as a pointer for greppers who land on the old locations.

fs::path ResolveCacheRoot(const std::string &InConfigCacheDir = "")
{
    if (!InConfigCacheDir.empty())
    {
        const fs::path ConfigPath(InConfigCacheDir);
        if (ConfigPath.is_absolute())
        {
            return ConfigPath;
        }
        // Relative paths resolve against the executable directory
        return ResolveExecutableDirectory() / ConfigPath;
    }

    const char *HomeEnv = std::getenv("HOME");
#ifdef _WIN32
    if (HomeEnv == nullptr || *HomeEnv == '\0')
    {
        HomeEnv = std::getenv("USERPROFILE");
    }
#endif
    if (HomeEnv == nullptr || *HomeEnv == '\0')
    {
        return fs::current_path() / fs::path(".tmp/uni-plan/cache");
    }

    return fs::path(HomeEnv) / fs::path(".uni-plan/cache");
}

fs::path BuildInventoryCachePath(const fs::path &InRepoRoot,
                                 const std::string &InConfigCacheDir = "")
{
    uint64_t HashState = 1469598103934665603ull;
    Fnv1aUpdateString(HashState, ToGenericPath(InRepoRoot));
    const std::string RepoKey = ToHexString(HashState);
    return ResolveCacheRoot(InConfigCacheDir) / fs::path(RepoKey) /
           fs::path("inventory.cache");
}

bool TryComputeMarkdownCorpusSignature(const fs::path &InRepoRoot,
                                       uint64_t &OutSignature,
                                       std::string &OutError)
{
    std::vector<MarkdownSignatureEntry> Entries;
    const fs::directory_options IteratorOptions =
        fs::directory_options::skip_permission_denied;
    std::error_code Error;
    fs::recursive_directory_iterator Iterator(InRepoRoot, IteratorOptions,
                                              Error);
    fs::recursive_directory_iterator EndIterator;
    if (Error)
    {
        OutError =
            "Signature traversal initialization failed: " + Error.message();
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
                OutError = "Signature traversal advance failed at '" +
                           AbsolutePath.string() +
                           "': " + AdvanceError.message();
                Iterator = EndIterator;
            }
        };

        std::error_code PathTypeError;
        const bool IsDirectory = Entry.is_directory(PathTypeError);
        if (PathTypeError)
        {
            OutError = "Signature traversal directory-type read failed for '" +
                       AbsolutePath.string() + "': " + PathTypeError.message();
            return false;
        }

        if (IsDirectory && ShouldSkipRecursionDirectory(AbsolutePath))
        {
            Iterator.disable_recursion_pending();
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        const bool IsRegularFile = Entry.is_regular_file(PathTypeError);
        if (PathTypeError)
        {
            OutError = "Signature traversal file-type read failed for '" +
                       AbsolutePath.string() + "': " + PathTypeError.message();
            return false;
        }

        if (!IsRegularFile || AbsolutePath.extension() != ".json")
        {
            AdvanceIterator();
            if (!OutError.empty())
            {
                return false;
            }
            continue;
        }

        fs::path RelativePath;
        try
        {
            RelativePath = fs::relative(AbsolutePath, InRepoRoot);
        }
        catch (const fs::filesystem_error &InError)
        {
            OutError =
                "Signature traversal relative-path resolution failed for '" +
                AbsolutePath.string() + "': " + InError.what();
            return false;
        }

        std::error_code WriteTimeError;
        const fs::file_time_type WriteTime =
            fs::last_write_time(AbsolutePath, WriteTimeError);
        if (WriteTimeError)
        {
            OutError = "Signature traversal write-time read failed for '" +
                       AbsolutePath.string() + "': " + WriteTimeError.message();
            return false;
        }

        std::error_code FileSizeError;
        const uint64_t FileSize =
            static_cast<uint64_t>(Entry.file_size(FileSizeError));
        if (FileSizeError)
        {
            OutError = "Signature traversal file-size read failed for '" +
                       AbsolutePath.string() + "': " + FileSizeError.message();
            return false;
        }

        MarkdownSignatureEntry SignatureEntry;
        SignatureEntry.mPath = ToGenericPath(RelativePath);
        SignatureEntry.mWriteTime =
            static_cast<uint64_t>(WriteTime.time_since_epoch().count());
        SignatureEntry.mFileSize = FileSize;
        Entries.push_back(std::move(SignatureEntry));

        AdvanceIterator();
        if (!OutError.empty())
        {
            return false;
        }
    }

    std::sort(Entries.begin(), Entries.end(),
              [](const MarkdownSignatureEntry &InLeft,
                 const MarkdownSignatureEntry &InRight)
              { return InLeft.mPath < InRight.mPath; });

    uint64_t HashState = 1469598103934665603ull;
    Fnv1aUpdateString(HashState, ToGenericPath(InRepoRoot));
    Fnv1aUpdateUint64(HashState, static_cast<uint64_t>(Entries.size()));
    for (const MarkdownSignatureEntry &Entry : Entries)
    {
        Fnv1aUpdateString(HashState, Entry.mPath);
        Fnv1aUpdateByte(HashState, 0x1F);
        Fnv1aUpdateUint64(HashState, Entry.mWriteTime);
        Fnv1aUpdateByte(HashState, 0x1E);
        Fnv1aUpdateUint64(HashState, Entry.mFileSize);
        Fnv1aUpdateByte(HashState, 0x1D);
    }

    OutSignature = HashState;
    return true;
}

// ---------------------------------------------------------------------------
// Cache utility helpers
// ---------------------------------------------------------------------------

uint64_t ComputeDirectorySize(const fs::path &InPath)
{
    uint64_t Total = 0;
    std::error_code Error;
    if (!fs::exists(InPath, Error))
    {
        return 0;
    }
    for (const auto &Entry : fs::recursive_directory_iterator(
             InPath, fs::directory_options::skip_permission_denied, Error))
    {
        if (Entry.is_regular_file(Error))
        {
            Total += static_cast<uint64_t>(Entry.file_size(Error));
        }
    }
    return Total;
}

int CountCacheEntries(const fs::path &InCacheRoot)
{
    int Count = 0;
    std::error_code Error;
    if (!fs::exists(InCacheRoot, Error))
    {
        return 0;
    }
    for (const auto &Entry : fs::directory_iterator(
             InCacheRoot, fs::directory_options::skip_permission_denied, Error))
    {
        if (Entry.is_directory(Error))
        {
            ++Count;
        }
    }
    return Count;
}

std::string FormatBytesHuman(uint64_t InBytes)
{
    if (InBytes < 1024)
    {
        return std::to_string(InBytes) + " B";
    }
    const char *Units[] = {"KB", "MB", "GB", "TB"};
    double Value = static_cast<double>(InBytes);
    int UnitIndex = -1;
    while (Value >= 1024.0 && UnitIndex < 3)
    {
        Value /= 1024.0;
        ++UnitIndex;
    }
    std::ostringstream Stream;
    Stream << std::fixed << std::setprecision(1) << Value << " "
           << Units[UnitIndex];
    return Stream.str();
}

// ---------------------------------------------------------------------------
// Cache core logic
// ---------------------------------------------------------------------------

CacheInfoResult BuildCacheInfo(const std::string &InRepoRoot,
                               const DocConfig &InConfig)
{
    CacheInfoResult Result;
    Result.mGeneratedUtc = GetUtcNow();
    Result.mConfigCacheDir = InConfig.mCacheDir;
    Result.mbCacheEnabled = InConfig.mbCacheEnabled;
    Result.mbCacheVerbose = InConfig.mbCacheVerbose;

    const fs::path ExeDir = ResolveExecutableDirectory();
    Result.mIniPath = (ExeDir / "uni-plan.ini").string();

    const fs::path CacheRoot = ResolveCacheRoot(InConfig.mCacheDir);
    Result.mCacheDir = CacheRoot.string();

    std::error_code Error;
    Result.mbCacheExists = fs::exists(CacheRoot, Error);
    if (Result.mbCacheExists)
    {
        Result.mCacheSizeBytes = ComputeDirectorySize(CacheRoot);
        Result.mCacheEntryCount = CountCacheEntries(CacheRoot);
    }

    const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);
    const fs::path RepoCachePath =
        BuildInventoryCachePath(RepoRoot, InConfig.mCacheDir);
    Result.mCurrentRepoCachePath = RepoCachePath.string();
    Result.mbCurrentRepoCacheExists = fs::exists(RepoCachePath, Error);

    return Result;
}

CacheClearResult ClearCache(const std::string &InRepoRoot,
                            const DocConfig &InConfig)
{
    CacheClearResult Result;
    Result.mGeneratedUtc = GetUtcNow();

    const fs::path CacheRoot = ResolveCacheRoot(InConfig.mCacheDir);
    Result.mCacheDir = CacheRoot.string();

    std::error_code Error;
    if (!fs::exists(CacheRoot, Error))
    {
        Result.mEntriesRemoved = 0;
        Result.mBytesFreed = 0;
        return Result;
    }

    Result.mBytesFreed = ComputeDirectorySize(CacheRoot);
    Result.mEntriesRemoved = CountCacheEntries(CacheRoot);

    fs::remove_all(CacheRoot, Error);
    if (Error)
    {
        Result.mbSuccess = false;
        Result.mError = "Failed to remove cache directory: " + Error.message();
    }

    return Result;
}

bool TryWriteDocIni(const fs::path &InPath, const std::string &InCacheDir,
                    const std::string &InCacheEnabled,
                    const std::string &InCacheVerbose, std::string &OutError)
{
    const fs::path TmpPath = fs::path(InPath.string() + ".tmp");
    std::ofstream File(TmpPath);
    if (!File.is_open())
    {
        OutError = "Cannot open for writing: " + TmpPath.string();
        return false;
    }

    File << "# Doc CLI tool configuration\n"
         << "# This file is read from the same directory as the doc "
            "executable.\n\n"
         << "[cache]\n"
         << "# Inventory cache directory.\n"
         << "# Supports: absolute paths, paths relative to this ini file's "
            "directory,\n"
         << "#           and ${ENV_VAR} expansion (e.g. ${HOME}).\n"
         << "# Default (if empty or missing): ${HOME}/.uni-plan/cache\n"
         << "dir = " << InCacheDir << "\n\n"
         << "# Enable or disable inventory caching globally.\n"
         << "# When false, equivalent to always passing --no-cache.\n"
         << "# Values: true, false (default: true)\n"
         << "enabled = " << InCacheEnabled << "\n\n"
         << "# Print cache hit/miss information to stderr.\n"
         << "# Useful for debugging cache behavior.\n"
         << "# Values: true, false (default: false)\n"
         << "verbose = " << InCacheVerbose << "\n";

    File.close();

    std::error_code Error;
    fs::rename(TmpPath, InPath, Error);
    if (Error)
    {
        fs::remove(TmpPath, Error);
        OutError = "Failed to rename tmp to ini: " + Error.message();
        return false;
    }

    return true;
}

} // namespace UniPlan
