// Regression guards for the v0.99.0 guarded bundle-write flow.
// Proves: atomic rename leaves no temp detritus; the stale-check detects
// out-of-band mutation; concurrent writers serialize with exactly one
// winner (both in-process threads and cross-process fork); pre-rename
// fault injection via UPLAN_FAULT_PRE_RENAME leaves the original intact;
// the bundle file lock blocks a peer until released; and the raw write
// primitive still works for bundles built in memory (mbValid=false).

#include "UniPlanTestFixture.h"

#include "UniPlanBundleWriteGuard.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanTopicTypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

namespace
{

static bool HasTmpSibling(const fs::path &InDir, const std::string &InStem)
{
    for (const auto &Entry : fs::directory_iterator(InDir))
    {
        const std::string Name = Entry.path().filename().string();
        if (Name.find(InStem + ".Plan.json.tmp.") == 0)
            return true;
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. RenameIsAtomic — happy-path round trip leaves no temp detritus
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, GuardRenameIsAtomic)
{
    CreateMinimalFixture("GuardAtomic", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("GuardAtomic", Bundle));
    ASSERT_TRUE(Bundle.mReadSession.mbValid)
        << "ReadSession should be stamped by TryLoadBundleByTopic";

    Bundle.mMetadata.mSummary = "Mutated via GuardedWriteBundle";

    std::string Error;
    ASSERT_EQ(UniPlan::GuardedWriteBundle(Bundle, Error), 0) << Error;

    const fs::path PlansDir = mRepoRoot / "Docs" / "Plans";
    EXPECT_FALSE(HasTmpSibling(PlansDir, "GuardAtomic"))
        << "Sibling tmp file must be consumed by atomic rename";

    UniPlan::FTopicBundle Reloaded;
    ASSERT_TRUE(ReloadBundle("GuardAtomic", Reloaded));
    EXPECT_EQ(Reloaded.mMetadata.mSummary, "Mutated via GuardedWriteBundle");
}

// ---------------------------------------------------------------------------
// 2. StaleCheckDetectsOutOfBandChange — second writer's mutation is refused
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, GuardStaleCheckDetectsOutOfBandChange)
{
    CreateMinimalFixture("GuardStale", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    // Load our bundle (stamps mReadSession with pre-peer identity).
    UniPlan::FTopicBundle Loaded;
    ASSERT_TRUE(ReloadBundle("GuardStale", Loaded));
    ASSERT_TRUE(Loaded.mReadSession.mbValid);

    // Peer finishes first via the raw write primitive.
    UniPlan::FTopicBundle Peer = Loaded;
    Peer.mMetadata.mSummary = "Peer wrote this first";
    std::string PeerError;
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "GuardStale.Plan.json";
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Peer, Path, PeerError))
        << PeerError;

    // Our mutation tries to write — must refuse.
    Loaded.mMetadata.mSummary = "Our mutation (should not reach disk)";
    std::string GuardError;
    EXPECT_EQ(UniPlan::GuardedWriteBundle(Loaded, GuardError), 1);
    EXPECT_NE(GuardError.find("bundle changed during mutation"),
              std::string::npos)
        << "Error was: " << GuardError;

    // On-disk must match peer, not our rejected write.
    UniPlan::FTopicBundle Final;
    ASSERT_TRUE(ReloadBundle("GuardStale", Final));
    EXPECT_EQ(Final.mMetadata.mSummary, "Peer wrote this first");
}

// ---------------------------------------------------------------------------
// 3. ConcurrentThreadsOneWinsOneLoses — in-process race, stale-check path
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, GuardConcurrentThreadsOneWinsOneLoses)
{
    CreateMinimalFixture("GuardThreads", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    std::mutex BarrierMutex;
    std::condition_variable BarrierCV;
    int BarrierCount = 0;
    const int Expected = 2;

    auto RunWriter = [&](const std::string &InSummary,
                         std::atomic<int> &OutCode, std::string &OutError)
    {
        UniPlan::FTopicBundle Bundle;
        std::string LoadErr;
        if (!UniPlan::TryLoadBundleByTopic(mRepoRoot, "GuardThreads", Bundle,
                                           LoadErr))
        {
            OutError = "load failed: " + LoadErr;
            OutCode.store(-1);
            return;
        }
        Bundle.mMetadata.mSummary = InSummary;

        // Barrier — both threads must finish loading before either writes.
        {
            std::unique_lock<std::mutex> Lock(BarrierMutex);
            ++BarrierCount;
            BarrierCV.notify_all();
            BarrierCV.wait(Lock, [&] { return BarrierCount >= Expected; });
        }

        OutCode.store(UniPlan::GuardedWriteBundle(Bundle, OutError));
    };

    std::atomic<int> CodeA{-99};
    std::atomic<int> CodeB{-99};
    std::string ErrorA;
    std::string ErrorB;

    std::thread A(RunWriter, std::string("Writer A"), std::ref(CodeA),
                  std::ref(ErrorA));
    std::thread B(RunWriter, std::string("Writer B"), std::ref(CodeB),
                  std::ref(ErrorB));
    A.join();
    B.join();

    const int Sum = CodeA.load() + CodeB.load();
    EXPECT_EQ(Sum, 1) << "Expected exactly one winner and one loser. "
                      << "CodeA=" << CodeA.load() << " (" << ErrorA << ") "
                      << "CodeB=" << CodeB.load() << " (" << ErrorB << ")";

    const fs::path PlansDir = mRepoRoot / "Docs" / "Plans";
    EXPECT_FALSE(HasTmpSibling(PlansDir, "GuardThreads"));

    UniPlan::FTopicBundle Final;
    ASSERT_TRUE(ReloadBundle("GuardThreads", Final));
    const std::string WinningSummary =
        (CodeA.load() == 0) ? "Writer A" : "Writer B";
    EXPECT_EQ(Final.mMetadata.mSummary, WinningSummary);
}

// ---------------------------------------------------------------------------
// 3b. ConcurrentThreadsStressLoop — 20 iterations of the thread race
//
// The single-shot ConcurrentThreadsOneWinsOneLoses above would pass even
// with unlock-before-rename because the race window (Unlock → fs::rename)
// is sub-microsecond and the loser's retry poll is 20ms. Looping the race
// gives the scheduler many more chances to interleave through the danger
// window, so if rename ever lands AFTER a peer's unlock+verify+rename
// path completes, the final file would be corrupted or the invariant
// (exactly-one-winner) would break on at least one iteration.
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, GuardConcurrentThreadsStressLoop)
{
    const int kIterations = 20;
    for (int Iter = 0; Iter < kIterations; ++Iter)
    {
        const std::string Key = "GuardStress" + std::to_string(Iter);
        CreateMinimalFixture(Key, UniPlan::ETopicStatus::NotStarted, 1,
                             UniPlan::EExecutionStatus::NotStarted, true);

        std::mutex BarrierMutex;
        std::condition_variable BarrierCV;
        int BarrierCount = 0;

        auto RunWriter = [&](const std::string &InSummary,
                             std::atomic<int> &OutCode, std::string &OutError)
        {
            UniPlan::FTopicBundle Bundle;
            std::string LoadErr;
            if (!UniPlan::TryLoadBundleByTopic(mRepoRoot, Key, Bundle, LoadErr))
            {
                OutError = "load failed: " + LoadErr;
                OutCode.store(-1);
                return;
            }
            Bundle.mMetadata.mSummary = InSummary;
            {
                std::unique_lock<std::mutex> Lock(BarrierMutex);
                ++BarrierCount;
                BarrierCV.notify_all();
                BarrierCV.wait(Lock, [&] { return BarrierCount >= 2; });
            }
            OutCode.store(UniPlan::GuardedWriteBundle(Bundle, OutError));
        };

        std::atomic<int> CodeA{-99};
        std::atomic<int> CodeB{-99};
        std::string ErrorA;
        std::string ErrorB;
        std::thread A(RunWriter, std::string("A"), std::ref(CodeA),
                      std::ref(ErrorA));
        std::thread B(RunWriter, std::string("B"), std::ref(CodeB),
                      std::ref(ErrorB));
        A.join();
        B.join();

        ASSERT_EQ(CodeA.load() + CodeB.load(), 1)
            << "Iteration " << Iter
            << " violated exactly-one-winner: CodeA=" << CodeA.load() << " ("
            << ErrorA << ") CodeB=" << CodeB.load() << " (" << ErrorB << ")";

        UniPlan::FTopicBundle Final;
        ASSERT_TRUE(ReloadBundle(Key, Final))
            << "Iteration " << Iter << ": cannot reload final bundle";
        const std::string Expected = (CodeA.load() == 0) ? "A" : "B";
        ASSERT_EQ(Final.mMetadata.mSummary, Expected)
            << "Iteration " << Iter
            << ": on-disk does not match declared winner";
    }
}

// ---------------------------------------------------------------------------
// 4. ConcurrentProcessesOneWinsOneLoses — cross-process, true flock path
// ---------------------------------------------------------------------------
#ifndef _WIN32
TEST_F(FBundleTestFixture, GuardConcurrentProcessesOneWinsOneLoses)
{
    CreateMinimalFixture("GuardFork", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    const std::string RepoRootString = mRepoRoot.string();

    auto ChildRun = [&](const std::string &InSummary) -> int
    {
        UniPlan::FTopicBundle Bundle;
        std::string LoadErr;
        if (!UniPlan::TryLoadBundleByTopic(fs::path(RepoRootString),
                                           "GuardFork", Bundle, LoadErr))
            return 2;
        Bundle.mMetadata.mSummary = InSummary;
        // Brief sleep to widen the contention window.
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        std::string Err;
        return UniPlan::GuardedWriteBundle(Bundle, Err);
    };

    pid_t PidA = fork();
    ASSERT_NE(PidA, -1);
    if (PidA == 0)
        _exit(ChildRun("Child A"));

    pid_t PidB = fork();
    ASSERT_NE(PidB, -1);
    if (PidB == 0)
        _exit(ChildRun("Child B"));

    int StatusA = 0;
    int StatusB = 0;
    ASSERT_EQ(::waitpid(PidA, &StatusA, 0), PidA);
    ASSERT_EQ(::waitpid(PidB, &StatusB, 0), PidB);
    ASSERT_TRUE(WIFEXITED(StatusA));
    ASSERT_TRUE(WIFEXITED(StatusB));

    const int ExitA = WEXITSTATUS(StatusA);
    const int ExitB = WEXITSTATUS(StatusB);
    EXPECT_EQ(ExitA + ExitB, 1)
        << "Expected one winner one loser. ExitA=" << ExitA
        << " ExitB=" << ExitB;

    UniPlan::FTopicBundle Final;
    ASSERT_TRUE(ReloadBundle("GuardFork", Final));
    EXPECT_TRUE(Final.mMetadata.mSummary == "Child A" ||
                Final.mMetadata.mSummary == "Child B")
        << Final.mMetadata.mSummary;
}
#endif

// ---------------------------------------------------------------------------
// 5. RenameFailureLeavesOriginalIntact — fault injection + non-torn invariant
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, GuardRenameFailureLeavesOriginalIntact)
{
    CreateMinimalFixture("GuardFault", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    const std::string BeforeBytes = ReadBundleFile("GuardFault");
    ASSERT_FALSE(BeforeBytes.empty());

    UniPlan::FTopicBundle Loaded;
    ASSERT_TRUE(ReloadBundle("GuardFault", Loaded));
    Loaded.mMetadata.mSummary = "This should never hit disk";

    ASSERT_EQ(::setenv("UPLAN_FAULT_PRE_RENAME", "1", 1), 0);
    std::string Error;
    const int Code = UniPlan::GuardedWriteBundle(Loaded, Error);
    ::unsetenv("UPLAN_FAULT_PRE_RENAME");

    EXPECT_EQ(Code, 1);
    EXPECT_NE(Error.find("UPLAN_FAULT_PRE_RENAME"), std::string::npos) << Error;

    const std::string AfterBytes = ReadBundleFile("GuardFault");
    EXPECT_EQ(BeforeBytes, AfterBytes)
        << "Original bundle must survive fault injection bit-identically";

    const fs::path PlansDir = mRepoRoot / "Docs" / "Plans";
    EXPECT_FALSE(HasTmpSibling(PlansDir, "GuardFault"))
        << "Fault path must clean up the tmp sibling";
}

// ---------------------------------------------------------------------------
// 6. ExternalLockHolderBlocksGuardedWrite — lock primitive serializes peers
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, GuardExternalLockHolderBlocksGuardedWrite)
{
    CreateMinimalFixture("GuardLock", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "GuardLock.Plan.json";

    std::atomic<bool> LockReleased{false};
    std::thread Holder(
        [&]()
        {
            std::string LockError;
            UniPlan::FBundleFileLock Lock(Path, LockError);
            ASSERT_TRUE(Lock.IsLocked()) << LockError;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            LockReleased.store(true);
            // Lock released by dtor here.
        });

    // Give the holder thread a moment to actually acquire the lock.
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("GuardLock", Bundle));
    Bundle.mMetadata.mSummary = "After lock release";

    const auto Start = std::chrono::steady_clock::now();
    std::string Error;
    const int Code = UniPlan::GuardedWriteBundle(Bundle, Error);
    const auto Elapsed = std::chrono::steady_clock::now() - Start;

    EXPECT_EQ(Code, 0) << Error;
    EXPECT_TRUE(LockReleased.load())
        << "Guard should have waited until the holder released";
    EXPECT_GE(
        std::chrono::duration_cast<std::chrono::milliseconds>(Elapsed).count(),
        100)
        << "Guard returned suspiciously fast; lock may not have blocked it";

    Holder.join();

    UniPlan::FTopicBundle Final;
    ASSERT_TRUE(ReloadBundle("GuardLock", Final));
    EXPECT_EQ(Final.mMetadata.mSummary, "After lock release");
}

// ---------------------------------------------------------------------------
// 7. RawPrimitiveSkipsStaleCheck — mbValid=false path for fresh-in-memory
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, GuardRawPrimitiveSkipsStaleCheck)
{
    // Seed a fresh file via the raw primitive (no prior read).
    UniPlan::FTopicBundle Fresh;
    Fresh.mTopicKey = "GuardFresh";
    Fresh.mMetadata.mTitle = "Fresh in memory";
    Fresh.mStatus = UniPlan::ETopicStatus::NotStarted;
    UniPlan::FPhaseRecord Phase;
    Phase.mScope = "phase 0";
    Fresh.mPhases.push_back(std::move(Phase));

    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "GuardFresh.Plan.json";
    Fresh.mBundlePath = Path.string();

    // mbValid=false by default (not loaded from disk) — guard must still
    // succeed, skipping the stale-check but taking the atomic-rename path.
    ASSERT_FALSE(Fresh.mReadSession.mbValid);
    std::string Error;
    EXPECT_EQ(UniPlan::GuardedWriteBundle(Fresh, Error), 0) << Error;
    EXPECT_TRUE(fs::exists(Path));

    UniPlan::FTopicBundle Reloaded;
    ASSERT_TRUE(ReloadBundle("GuardFresh", Reloaded));
    EXPECT_EQ(Reloaded.mMetadata.mTitle, "Fresh in memory");
    // And now the loaded copy *does* have a valid read-session stamp.
    EXPECT_TRUE(Reloaded.mReadSession.mbValid);
}
