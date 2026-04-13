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

void Fnv1aUpdateByte(uint64_t &InOutState, const uint8_t InValue)
{
    InOutState ^= static_cast<uint64_t>(InValue);
    InOutState *= 1099511628211ull;
}

void Fnv1aUpdateString(uint64_t &InOutState, const std::string &InValue)
{
    for (const char Character : InValue)
    {
        Fnv1aUpdateByte(InOutState, static_cast<uint8_t>(Character));
    }
}

void Fnv1aUpdateUint64(uint64_t &InOutState, uint64_t InValue)
{
    for (int ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
    {
        Fnv1aUpdateByte(InOutState, static_cast<uint8_t>(InValue & 0xFFull));
        InValue >>= 8;
    }
}

std::string ToHexString(const uint64_t InValue)
{
    std::ostringstream Stream;
    Stream << std::hex << std::nouppercase << std::setw(16) << std::setfill('0')
           << InValue;
    return Stream.str();
}

std::string EscapeCacheField(const std::string &InValue)
{
    std::string Result;
    Result.reserve(InValue.size());
    for (const char Character : InValue)
    {
        if (Character == '\\')
        {
            Result += "\\\\";
            continue;
        }
        if (Character == '\t')
        {
            Result += "\\t";
            continue;
        }
        if (Character == '\n')
        {
            Result += "\\n";
            continue;
        }
        if (Character == '\r')
        {
            Result += "\\r";
            continue;
        }
        Result.push_back(Character);
    }
    return Result;
}

std::string UnescapeCacheField(const std::string &InValue)
{
    std::string Result;
    Result.reserve(InValue.size());
    for (size_t Index = 0; Index < InValue.size(); ++Index)
    {
        const char Character = InValue[Index];
        if (Character != '\\' || Index + 1 >= InValue.size())
        {
            Result.push_back(Character);
            continue;
        }

        const char Escape = InValue[Index + 1];
        Index += 1;
        if (Escape == 't')
        {
            Result.push_back('\t');
            continue;
        }
        if (Escape == 'n')
        {
            Result.push_back('\n');
            continue;
        }
        if (Escape == 'r')
        {
            Result.push_back('\r');
            continue;
        }
        Result.push_back(Escape);
    }
    return Result;
}

std::vector<std::string> SplitCacheFields(const std::string &InLine)
{
    std::vector<std::string> Fields;
    std::string Current;
    std::istringstream Stream(InLine);
    while (std::getline(Stream, Current, '\t'))
    {
        Fields.push_back(Current);
    }
    if (!InLine.empty() && InLine.back() == '\t')
    {
        Fields.emplace_back();
    }
    return Fields;
}

fs::path ResolveCodexCacheRoot(const std::string &InConfigCacheDir = "")
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
        return fs::current_path() / fs::path(".tmp/doc/cache");
    }

    return fs::path(HomeEnv) / fs::path(".codex/uni-plan/cache");
}

fs::path BuildInventoryCachePath(const fs::path &InRepoRoot,
                                 const std::string &InConfigCacheDir = "")
{
    uint64_t HashState = 1469598103934665603ull;
    Fnv1aUpdateString(HashState, ToGenericPath(InRepoRoot));
    const std::string RepoKey = ToHexString(HashState);
    return ResolveCodexCacheRoot(InConfigCacheDir) / fs::path(RepoKey) /
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

bool TryWriteInventoryCache(const fs::path &InCachePath,
                            const Inventory &InInventory,
                            const uint64_t InSignature, std::string &OutError)
{
    const fs::path ParentDirectory = InCachePath.parent_path();
    std::error_code Error;
    fs::create_directories(ParentDirectory, Error);
    if (Error)
    {
        OutError = "Failed to create cache directory '" +
                   ParentDirectory.string() + "': " + Error.message();
        return false;
    }

    const fs::path TempPath = InCachePath.string() + ".tmp";
    std::ofstream Stream(TempPath);
    if (!Stream.is_open())
    {
        OutError =
            "Failed to open temp cache file for write: " + TempPath.string();
        return false;
    }

    Stream << "schema\t" << kInventoryCacheSchema << "\n";
    Stream << "signature\t" << InSignature << "\n";
    Stream << "repo_root\t" << EscapeCacheField(InInventory.mRepoRoot) << "\n";
    Stream << "[plans]\n";
    for (const DocumentRecord &Plan : InInventory.mPlans)
    {
        Stream << EscapeCacheField(Plan.mTopicKey) << "\t"
               << EscapeCacheField(Plan.mPhaseKey) << "\t"
               << EscapeCacheField(Plan.mStatusRaw) << "\t"
               << EscapeCacheField(Plan.mStatus) << "\t"
               << EscapeCacheField(Plan.mPath) << "\n";
    }
    Stream << "[/plans]\n";

    Stream << "[playbooks]\n";
    for (const DocumentRecord &Playbook : InInventory.mPlaybooks)
    {
        Stream << EscapeCacheField(Playbook.mTopicKey) << "\t"
               << EscapeCacheField(Playbook.mPhaseKey) << "\t"
               << EscapeCacheField(Playbook.mStatusRaw) << "\t"
               << EscapeCacheField(Playbook.mStatus) << "\t"
               << EscapeCacheField(Playbook.mPath) << "\n";
    }
    Stream << "[/playbooks]\n";

    Stream << "[implementations]\n";
    for (const DocumentRecord &Implementation : InInventory.mImplementations)
    {
        Stream << EscapeCacheField(Implementation.mTopicKey) << "\t"
               << EscapeCacheField(Implementation.mPhaseKey) << "\t"
               << EscapeCacheField(Implementation.mStatusRaw) << "\t"
               << EscapeCacheField(Implementation.mStatus) << "\t"
               << EscapeCacheField(Implementation.mPath) << "\n";
    }
    Stream << "[/implementations]\n";

    Stream << "[sidecars]\n";
    for (const SidecarRecord &Sidecar : InInventory.mSidecars)
    {
        Stream << EscapeCacheField(Sidecar.mTopicKey) << "\t"
               << EscapeCacheField(Sidecar.mPhaseKey) << "\t"
               << EscapeCacheField(Sidecar.mOwnerKind) << "\t"
               << EscapeCacheField(Sidecar.mDocKind) << "\t"
               << EscapeCacheField(Sidecar.mPath) << "\n";
    }
    Stream << "[/sidecars]\n";

    Stream.close();
    if (!Stream.good())
    {
        std::error_code RemoveError;
        fs::remove(TempPath, RemoveError);
        OutError = "Failed while writing temp cache file: " + TempPath.string();
        return false;
    }

    fs::rename(TempPath, InCachePath, Error);
    if (Error)
    {
        std::error_code RemoveError;
        fs::remove(InCachePath, RemoveError);
        Error.clear();
        fs::rename(TempPath, InCachePath, Error);
        if (Error)
        {
            std::error_code CleanupError;
            fs::remove(TempPath, CleanupError);
            OutError = "Failed to finalize cache file '" +
                       InCachePath.string() + "': " + Error.message();
            return false;
        }
    }

    return true;
}

bool TryLoadInventoryCache(const fs::path &InCachePath,
                           const std::string &InRepoRoot,
                           const uint64_t InSignature, Inventory &OutInventory,
                           std::string &OutError)
{
    std::error_code Error;
    if (!fs::exists(InCachePath, Error))
    {
        return false;
    }
    if (Error)
    {
        OutError = "Failed to check cache file '" + InCachePath.string() +
                   "': " + Error.message();
        return false;
    }

    std::ifstream Stream(InCachePath);
    if (!Stream.is_open())
    {
        OutError = "Failed to open cache file: " + InCachePath.string();
        return false;
    }

    enum class ECacheSection
    {
        None,
        Plans,
        Playbooks,
        Implementations,
        Sidecars
    };

    ECacheSection Section = ECacheSection::None;
    std::string Schema;
    std::string CachedRepoRoot;
    uint64_t CachedSignature = 0;
    bool HasSignature = false;

    Inventory Loaded;
    Loaded.mRepoRoot = InRepoRoot;
    Loaded.mGeneratedUtc = GetUtcNow();

    std::string Line;
    while (std::getline(Stream, Line))
    {
        if (Line == "[plans]")
        {
            Section = ECacheSection::Plans;
            continue;
        }
        if (Line == "[/plans]")
        {
            Section = ECacheSection::None;
            continue;
        }
        if (Line == "[playbooks]")
        {
            Section = ECacheSection::Playbooks;
            continue;
        }
        if (Line == "[/playbooks]")
        {
            Section = ECacheSection::None;
            continue;
        }
        if (Line == "[implementations]")
        {
            Section = ECacheSection::Implementations;
            continue;
        }
        if (Line == "[/implementations]")
        {
            Section = ECacheSection::None;
            continue;
        }
        if (Line == "[sidecars]")
        {
            Section = ECacheSection::Sidecars;
            continue;
        }
        if (Line == "[/sidecars]")
        {
            Section = ECacheSection::None;
            continue;
        }

        const std::vector<std::string> Fields = SplitCacheFields(Line);
        if (Section == ECacheSection::None)
        {
            if (Fields.size() != 2)
            {
                continue;
            }
            const std::string Key = Fields[0];
            const std::string Value = Fields[1];
            if (Key == "schema")
            {
                Schema = Value;
            }
            else if (Key == "repo_root")
            {
                CachedRepoRoot = UnescapeCacheField(Value);
            }
            else if (Key == "signature")
            {
                try
                {
                    CachedSignature = static_cast<uint64_t>(std::stoull(Value));
                    HasSignature = true;
                }
                catch (const std::exception &)
                {
                    OutError = "Invalid signature value in cache file: " +
                               InCachePath.string();
                    return false;
                }
            }
            continue;
        }

        if (Section == ECacheSection::Sidecars)
        {
            if (Fields.size() != 5)
            {
                OutError = "Invalid sidecar row in cache file: " +
                           InCachePath.string();
                return false;
            }

            SidecarRecord Sidecar;
            Sidecar.mTopicKey = UnescapeCacheField(Fields[0]);
            Sidecar.mPhaseKey = UnescapeCacheField(Fields[1]);
            Sidecar.mOwnerKind = UnescapeCacheField(Fields[2]);
            Sidecar.mDocKind = UnescapeCacheField(Fields[3]);
            Sidecar.mPath = UnescapeCacheField(Fields[4]);
            Loaded.mSidecars.push_back(std::move(Sidecar));
            continue;
        }

        if (Fields.size() != 5)
        {
            OutError =
                "Invalid document row in cache file: " + InCachePath.string();
            return false;
        }

        DocumentRecord Record;
        Record.mTopicKey = UnescapeCacheField(Fields[0]);
        Record.mPhaseKey = UnescapeCacheField(Fields[1]);
        Record.mStatusRaw = UnescapeCacheField(Fields[2]);
        Record.mStatus = UnescapeCacheField(Fields[3]);
        Record.mPath = UnescapeCacheField(Fields[4]);

        if (Section == ECacheSection::Plans)
        {
            Record.mKind = EDocumentKind::Plan;
            Loaded.mPlans.push_back(std::move(Record));
            continue;
        }
        if (Section == ECacheSection::Playbooks)
        {
            Record.mKind = EDocumentKind::Playbook;
            Loaded.mPlaybooks.push_back(std::move(Record));
            continue;
        }
        if (Section == ECacheSection::Implementations)
        {
            Record.mKind = EDocumentKind::Implementation;
            Loaded.mImplementations.push_back(std::move(Record));
            continue;
        }
    }

    if (Schema != kInventoryCacheSchema)
    {
        return false;
    }
    if (!HasSignature || CachedSignature != InSignature)
    {
        return false;
    }
    if (!CachedRepoRoot.empty() && CachedRepoRoot != InRepoRoot)
    {
        return false;
    }

    SortRecords(Loaded.mPlans);
    SortRecords(Loaded.mPlaybooks);
    SortRecords(Loaded.mImplementations);
    SortSidecars(Loaded.mSidecars);
    AppendSidecarIntegrityWarnings(Loaded.mPlans, Loaded.mPlaybooks,
                                   Loaded.mImplementations, Loaded.mSidecars,
                                   Loaded.mWarnings);
    Loaded.mPairs = BuildTopicPairs(Loaded.mPlans, Loaded.mPlaybooks,
                                    Loaded.mImplementations, Loaded.mWarnings);
    NormalizeWarnings(Loaded.mWarnings);

    OutInventory = std::move(Loaded);
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

    const fs::path CacheRoot = ResolveCodexCacheRoot(InConfig.mCacheDir);
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

    const fs::path CacheRoot = ResolveCodexCacheRoot(InConfig.mCacheDir);
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
         << "# Default (if empty or missing): ${HOME}/.codex/uni-plan/cache\n"
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
