#include "UniPlanBundleWriteGuard.h"

#include "UniPlanFileHelpers.h"
#include "UniPlanHashHelpers.h"
#include "UniPlanJSONIO.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <system_error>
#include <thread>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Internal: slurp file bytes and hash them with the shared FNV-1a helpers.
// Returns 0 on failure; caller must check OutError.
// ---------------------------------------------------------------------------
static std::uint64_t ComputeFileHashFnv1a(const fs::path &InPath,
                                          std::string &OutError)
{
    std::string Content;
    std::string ReadError;
    if (!TryReadFileToStringShared(InPath, Content, ReadError))
    {
        OutError = "Cannot open for hashing: " + InPath.string() + " (" +
                   ReadError + ")";
        return 0;
    }
    std::uint64_t State = kFnv1aSeed;
    Fnv1aUpdateString(State, Content);
    return State;
}

// ---------------------------------------------------------------------------
// Internal: build `<final>.tmp.<pid>.<tid>` in the same directory so atomic
// rename stays same-filesystem. Thread id suffix prevents two threads in
// one process from fighting over the same sibling filename.
// ---------------------------------------------------------------------------
static fs::path BuildSiblingTmpPath(const fs::path &InFinal)
{
    const auto TID = std::this_thread::get_id();
    std::ostringstream TidStream;
    TidStream << TID;
#ifdef _WIN32
    const int PID = static_cast<int>(GetCurrentProcessId());
#else
    const int PID = static_cast<int>(getpid());
#endif
    std::string Suffix = ".tmp." + std::to_string(PID) + "." + TidStream.str();
    fs::path Out = InFinal;
    Out += Suffix;
    return Out;
}

#ifdef _WIN32
static fs::path BuildSiblingLockPath(const fs::path &InFinal)
{
    fs::path Out = InFinal;
    Out += ".lock";
    return Out;
}
#endif

static bool ShouldInjectPreRenameFault()
{
    const char *Val = std::getenv("UPLAN_FAULT_PRE_RENAME");
    if (Val == nullptr)
        return false;
    if (Val[0] == '\0' || std::strcmp(Val, "0") == 0)
        return false;
    return true;
}

static bool IsCrossDeviceRenameError(const std::error_code &InError)
{
    return InError == std::errc::cross_device_link;
}

static bool IsRetryableRenameError(const std::error_code &InError)
{
#ifdef _WIN32
    if (InError == std::errc::permission_denied ||
        InError == std::errc::device_or_resource_busy)
    {
        return true;
    }
    return InError.value() == ERROR_ACCESS_DENIED ||
           InError.value() == ERROR_SHARING_VIOLATION ||
           InError.value() == ERROR_LOCK_VIOLATION;
#else
    return InError == std::errc::interrupted ||
           InError == std::errc::device_or_resource_busy;
#endif
}

static std::string BuildAtomicReplaceError(const fs::path &InFinalPath,
                                           const std::error_code &InError)
{
    if (IsCrossDeviceRenameError(InError))
    {
        return "Atomic rename failed across filesystems: " +
               InFinalPath.string() +
               ". Do not symlink Docs/Plans across filesystem boundaries; "
               "GuardedWriteBundle refuses non-atomic fallbacks.";
    }
    return "Atomic rename failed: " + InFinalPath.string() + " (" +
           InError.message() + ")";
}

bool TryAtomicReplace(const fs::path &InTmpPath, const fs::path &InFinalPath,
                      std::string &OutError)
{
#ifdef _WIN32
    constexpr int kMaxAttempts = 50;
    const std::chrono::milliseconds RetryDelay(20);
#else
    constexpr int kMaxAttempts = 3;
    const std::chrono::milliseconds RetryDelay(1);
#endif

    std::error_code LastError;
    for (int Attempt = 1; Attempt <= kMaxAttempts; ++Attempt)
    {
        std::error_code RenameError;
        fs::rename(InTmpPath, InFinalPath, RenameError);
        if (!RenameError)
        {
            return true;
        }
        if (IsCrossDeviceRenameError(RenameError))
        {
            OutError = BuildAtomicReplaceError(InFinalPath, RenameError);
            return false;
        }

        LastError = RenameError;
        if (!IsRetryableRenameError(RenameError) || Attempt == kMaxAttempts)
        {
            break;
        }
        std::this_thread::sleep_for(RetryDelay);
    }

    OutError = BuildAtomicReplaceError(InFinalPath, LastError);
    return false;
}

// ---------------------------------------------------------------------------
// CaptureReadSession
// ---------------------------------------------------------------------------
bool CaptureReadSession(const fs::path &InPath, FBundleReadSession &OutSession,
                        std::string &OutError)
{
    OutSession = FBundleReadSession{};

    std::error_code Err;
    if (!fs::exists(InPath, Err) || Err)
    {
        OutError =
            "Bundle path missing for read-session capture: " + InPath.string();
        return false;
    }

    const auto Size = fs::file_size(InPath, Err);
    if (Err)
    {
        OutError =
            "file_size failed for read-session capture: " + InPath.string() +
            " (" + Err.message() + ")";
        return false;
    }
    OutSession.mFileSize = static_cast<std::uint64_t>(Size);

    const auto MTime = fs::last_write_time(InPath, Err);
    if (Err)
    {
        OutError = "last_write_time failed for read-session capture: " +
                   InPath.string() + " (" + Err.message() + ")";
        return false;
    }
    const auto Nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           MTime.time_since_epoch())
                           .count();
    OutSession.mMTimeNanos = static_cast<std::int64_t>(Nanos);

    std::string HashError;
    const std::uint64_t Hash = ComputeFileHashFnv1a(InPath, HashError);
    if (!HashError.empty())
    {
        OutError = HashError;
        return false;
    }
    OutSession.mContentHash = Hash;
    OutSession.mbValid = true;
    return true;
}

// ---------------------------------------------------------------------------
// VerifyReadSessionUnderLock
// ---------------------------------------------------------------------------
bool VerifyReadSessionUnderLock(const fs::path &InPath,
                                const FBundleReadSession &InSession,
                                std::string &OutError)
{
    std::error_code Err;
    const auto Size = fs::file_size(InPath, Err);
    if (Err)
    {
        OutError = "Stale-check file_size failed: " + InPath.string() + " (" +
                   Err.message() + ")";
        return false;
    }
    if (static_cast<std::uint64_t>(Size) != InSession.mFileSize)
    {
        OutError = "bundle changed during mutation; re-read and retry "
                   "(file size differs)";
        return false;
    }

    const auto MTime = fs::last_write_time(InPath, Err);
    if (Err)
    {
        OutError = "Stale-check last_write_time failed: " + InPath.string() +
                   " (" + Err.message() + ")";
        return false;
    }
    const auto Nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           MTime.time_since_epoch())
                           .count();

    // mtime mismatch is only a hint; verify by hash because an out-of-band
    // write within the filesystem's mtime resolution window can produce a
    // false negative. Hash is authoritative.
    if (static_cast<std::int64_t>(Nanos) != InSession.mMTimeNanos)
    {
        std::string HashError;
        const std::uint64_t Hash = ComputeFileHashFnv1a(InPath, HashError);
        if (!HashError.empty())
        {
            OutError = HashError;
            return false;
        }
        if (Hash != InSession.mContentHash)
        {
            OutError = "bundle changed during mutation; re-read and retry "
                       "(content hash differs)";
            return false;
        }
        // mtime changed but bytes are identical — benign (e.g. `touch`).
        return true;
    }

    // mtime matches; hash to close the sub-second ambiguity window.
    std::string HashError;
    const std::uint64_t Hash = ComputeFileHashFnv1a(InPath, HashError);
    if (!HashError.empty())
    {
        OutError = HashError;
        return false;
    }
    if (Hash != InSession.mContentHash)
    {
        OutError = "bundle changed during mutation; re-read and retry "
                   "(content hash differs within mtime resolution window)";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// FBundleFileLock — platform-conditional RAII
// ---------------------------------------------------------------------------
FBundleFileLock::FBundleFileLock(const fs::path &InPath, std::string &OutError,
                                 std::chrono::milliseconds InTimeout)
{
    const auto Deadline = std::chrono::steady_clock::now() + InTimeout;
    const auto Poll = std::chrono::milliseconds(20);

#ifdef _WIN32
    const std::wstring WidePath = BuildSiblingLockPath(InPath).wstring();
    for (;;)
    {
        mHandle =
            CreateFileW(WidePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (mHandle != INVALID_HANDLE_VALUE)
        {
            OVERLAPPED Ovl;
            std::memset(&Ovl, 0, sizeof(Ovl));
            if (LockFileEx(mHandle,
                           LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                           0, MAXDWORD, MAXDWORD, &Ovl))
            {
                mbLocked = true;
                return;
            }
            CloseHandle(mHandle);
            mHandle = INVALID_HANDLE_VALUE;
        }
        if (std::chrono::steady_clock::now() >= Deadline)
        {
            OutError = "Timed out acquiring bundle lock: " + InPath.string();
            return;
        }
        std::this_thread::sleep_for(Poll);
    }
#else
    for (;;)
    {
        mFd = ::open(InPath.c_str(), O_RDONLY | O_CLOEXEC);
        if (mFd >= 0)
        {
            if (::flock(mFd, LOCK_EX | LOCK_NB) == 0)
            {
                mbLocked = true;
                return;
            }
            const int LockErr = errno;
            ::close(mFd);
            mFd = -1;
            if (LockErr != EWOULDBLOCK && LockErr != EAGAIN && LockErr != EINTR)
            {
                OutError = "flock failed: " + InPath.string() + " (" +
                           std::strerror(LockErr) + ")";
                return;
            }
        }
        else
        {
            const int OpenErr = errno;
            OutError = "Cannot open bundle for locking: " + InPath.string() +
                       " (" + std::strerror(OpenErr) + ")";
            return;
        }
        if (std::chrono::steady_clock::now() >= Deadline)
        {
            OutError = "Timed out acquiring bundle lock: " + InPath.string();
            return;
        }
        std::this_thread::sleep_for(Poll);
    }
#endif
}

void FBundleFileLock::Unlock()
{
    if (!mbLocked)
        return;
#ifdef _WIN32
    if (mHandle != INVALID_HANDLE_VALUE)
    {
        OVERLAPPED Ovl;
        std::memset(&Ovl, 0, sizeof(Ovl));
        UnlockFileEx(mHandle, 0, MAXDWORD, MAXDWORD, &Ovl);
        CloseHandle(mHandle);
        mHandle = INVALID_HANDLE_VALUE;
    }
#else
    if (mFd >= 0)
    {
        ::flock(mFd, LOCK_UN);
        ::close(mFd);
        mFd = -1;
    }
#endif
    mbLocked = false;
}

FBundleFileLock::~FBundleFileLock()
{
    Unlock();
}

// ---------------------------------------------------------------------------
// Internal: write a bundle to a specific path via the raw serializer, then
// fsync. Used by the guard after it has picked the tmp path.
// ---------------------------------------------------------------------------
static bool WriteTmpAndFlush(const FTopicBundle &InBundle,
                             const fs::path &InTmpPath, std::string &OutError)
{
    if (!TryWriteTopicBundle(InBundle, InTmpPath, OutError))
        return false;
#ifdef _WIN32
    const std::wstring WidePath = InTmpPath.wstring();
    HANDLE H = CreateFileW(WidePath.c_str(), GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (H != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers(H);
        CloseHandle(H);
    }
#else
    const int Fd = ::open(InTmpPath.c_str(), O_WRONLY);
    if (Fd >= 0)
    {
        ::fsync(Fd);
        ::close(Fd);
    }
#endif
    return true;
}

// ---------------------------------------------------------------------------
// GuardedWriteBundle
// ---------------------------------------------------------------------------
int GuardedWriteBundle(const FTopicBundle &InBundle, std::string &OutError)
{
    if (InBundle.mBundlePath.empty())
    {
        OutError = "GuardedWriteBundle: bundle has no source path";
        return 1;
    }
    const fs::path FinalPath(InBundle.mBundlePath);
    const fs::path TmpPath = BuildSiblingTmpPath(FinalPath);

    std::error_code ExistsErr;
    const bool bTargetExists = fs::exists(FinalPath, ExistsErr);

    if (bTargetExists)
    {
        std::string LockError;
        FBundleFileLock Lock(FinalPath, LockError);
        if (!Lock.IsLocked())
        {
            OutError = LockError;
            return 1;
        }

        if (InBundle.mReadSession.mbValid)
        {
            std::string VerifyError;
            if (!VerifyReadSessionUnderLock(FinalPath, InBundle.mReadSession,
                                            VerifyError))
            {
                OutError = VerifyError;
                return 1;
            }
        }

        std::string WriteError;
        if (!WriteTmpAndFlush(InBundle, TmpPath, WriteError))
        {
            std::error_code RmErr;
            fs::remove(TmpPath, RmErr);
            OutError = WriteError;
            return 1;
        }

        if (ShouldInjectPreRenameFault())
        {
            std::error_code RmErr;
            fs::remove(TmpPath, RmErr);
            OutError = "UPLAN_FAULT_PRE_RENAME: aborting before rename "
                       "(test-only fault injection)";
            return 1;
        }

        // Rename BEFORE releasing the lock. Releasing first opens a race
        // window where a peer can acquire the lock, observe the still-
        // original on-disk content (our rename hasn't landed), pass the
        // stale-check, write their own tmp, rename it in, and release —
        // after which our rename clobbers their write. POSIX flock is
        // per-OFD so holding the lock across the rename is safe: after
        // rename the directory entry points to a new inode, our handle
        // still references the old (unreferenced) inode, and a peer
        // opening the new entry gets a fresh OFD with no conflict.
        // Windows: the lock handle was opened with FILE_SHARE_DELETE so
        // MoveFileEx(REPLACE_EXISTING) — which is what fs::rename calls
        // through — can replace the target even while the handle is held.
        // The RAII destructor releases the lock when the function returns.
        std::string RenameError;
        if (!TryAtomicReplace(TmpPath, FinalPath, RenameError))
        {
            std::error_code RmErr;
            fs::remove(TmpPath, RmErr);
            OutError = RenameError;
            return 1;
        }
        return 0;
    }

    // Target does not yet exist (e.g. `topic add` first write). No stale-check
    // possible. Write tmp, rename into place. Concurrent first-write collisions
    // are caught by topic add's existing collision pre-check upstream.
    std::string WriteError;
    if (!WriteTmpAndFlush(InBundle, TmpPath, WriteError))
    {
        std::error_code RmErr;
        fs::remove(TmpPath, RmErr);
        OutError = WriteError;
        return 1;
    }

    if (ShouldInjectPreRenameFault())
    {
        std::error_code RmErr;
        fs::remove(TmpPath, RmErr);
        OutError = "UPLAN_FAULT_PRE_RENAME: aborting before rename "
                   "(test-only fault injection)";
        return 1;
    }

    std::string RenameError;
    if (!TryAtomicReplace(TmpPath, FinalPath, RenameError))
    {
        std::error_code RmErr;
        fs::remove(TmpPath, RmErr);
        OutError = RenameError;
        return 1;
    }
    return 0;
}

} // namespace UniPlan
