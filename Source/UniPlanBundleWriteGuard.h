#pragma once

#include "UniPlanTopicTypes.h"

#include <chrono>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace UniPlan
{

// ===========================================================================
// Bundle write guard — concurrency / atomicity for .Plan.json mutations.
//
// Closes the race where two uni-plan instances (or threads) holding a read
// of the same bundle both serialize their mutation back to disk, the second
// write clobbering the first. The guard funnels every mutation through:
//
//   1. Acquire exclusive advisory lock for the bundle
//      (flock(LOCK_EX) on POSIX; a sibling `.lock` file on Windows because
//      LockFileEx byte-range locks are mandatory and would block the
//      under-lock stale-check read).
//   2. If the caller's FTopicBundle.mReadSession.mbValid is true, re-stat
//      and re-hash the on-disk bytes; if anything changed since the read,
//      fail with a conflict error (no bypass flag).
//   3. Serialize + write to a sibling <bundle>.tmp.<pid>.<tid> file.
//   4. fsync + close.
//   5. Rename before releasing the lock so a peer cannot pass stale-check
//      against the old content and then race the replacement.
//   6. std::filesystem::rename(tmp, final) — atomic on POSIX same-filesystem;
//      MOVEFILE_REPLACE_EXISTING-equivalent on Windows.
//
// No `TryWriteTopicBundle`-is-still-fine bypass is offered; the raw
// serialize-and-write primitive in UniPlanJSONIO.h remains as a test fixture
// primitive and a "bundle built in memory, no prior disk identity" helper,
// but every production mutation path routes through GuardedWriteBundle.
// ===========================================================================

// ---------------------------------------------------------------------------
// Capture the current file-identity snapshot (size, mtime, FNV-1a-64 hash
// of file bytes) into OutSession. Called at the tail of the bundle-read
// funnels (TryLoadBundleByTopic and LoadAllBundles). Sets OutSession.mbValid
// to true on success.
//
// Failure modes (OutError populated, OutSession.mbValid left false):
//   - InPath does not exist
//   - Cannot stat (permissions)
//   - Cannot open for reading (hash pass)
// ---------------------------------------------------------------------------
bool CaptureReadSession(const fs::path &InPath, FBundleReadSession &OutSession,
                        std::string &OutError);

// ---------------------------------------------------------------------------
// Re-stat and re-hash the file at InPath; return true iff every field
// matches InSession. Caller must hold FBundleFileLock for InPath for the
// duration of the call (otherwise a lost-update window reopens between
// this check and the rename).
//
// Fast path: (size, mtime) comparison alone handles ~all negative cases
// without touching file bytes. Only when (size, mtime) match do we hash,
// closing the sub-second-mtime window common on HFS+.
// ---------------------------------------------------------------------------
bool VerifyReadSessionUnderLock(const fs::path &InPath,
                                const FBundleReadSession &InSession,
                                std::string &OutError);

// ---------------------------------------------------------------------------
// The mutation-path write. Locks, verifies (if mbValid), serializes to a
// sibling temp, fsyncs, atomically renames into place. Returns 0 on success
// or 1 on any error; OutError carries a diagnostic suitable for
// `std::cerr << OutError << "\n";` by callers.
//
// Precondition: InBundle.mBundlePath is non-empty. Callers who haven't
// loaded from disk (topic add) must set mBundlePath to the target path
// and leave mReadSession.mbValid=false.
// ---------------------------------------------------------------------------
int GuardedWriteBundle(const FTopicBundle &InBundle, std::string &OutError);

// ---------------------------------------------------------------------------
// FBundleFileLock — RAII exclusive advisory lock for a bundle file.
//
// Used internally by GuardedWriteBundle; exposed for tests that need to
// hold a lock across command invocations (e.g. "migrate blocks while a
// peer holds the lock, then proceeds").
//
// Construction blocks up to InTimeout (default 5 s) acquiring the lock;
// on timeout or error, IsLocked() returns false and OutError (via the
// GetError() accessor) is populated. The destructor releases on both
// platforms — explicit Unlock() is offered for tests and early-release paths.
// ---------------------------------------------------------------------------
class FBundleFileLock
{
  public:
    FBundleFileLock(
        const fs::path &InPath, std::string &OutError,
        std::chrono::milliseconds InTimeout = std::chrono::milliseconds(5000));
    ~FBundleFileLock();

    FBundleFileLock(const FBundleFileLock &) = delete;
    FBundleFileLock &operator=(const FBundleFileLock &) = delete;
    FBundleFileLock(FBundleFileLock &&) = delete;
    FBundleFileLock &operator=(FBundleFileLock &&) = delete;

    bool IsLocked() const
    {
        return mbLocked;
    }

    // Explicit early release. Safe to call multiple times and safe to call
    // from the destructor path.
    void Unlock();

  private:
    bool mbLocked = false;
#ifdef _WIN32
    HANDLE mHandle = INVALID_HANDLE_VALUE;
#else
    int mFd = -1;
#endif
};

} // namespace UniPlan
