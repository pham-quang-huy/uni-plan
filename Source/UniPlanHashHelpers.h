#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// FNV-1a 64-bit hash helpers — cooperative signature use only.
//
// Used as the file-identity digest in FBundleReadSession (bundle-write-guard
// stale-check) and as the cache-signature mixer in UniPlanCache. Both are
// cooperative-concurrency / cache-invalidation signals, not integrity checks.
// An adversary able to craft colliding JSON at the same file size + mtime
// could bypass the stale-check; that attacker can already rewrite the file
// directly, so the trade-off is intentional.
//
// Seed: 1469598103934665603ull. NOTE: the canonical FNV-1a 64-bit offset
// basis is 14695981039346656037ull (two digits longer); the historical
// UniPlanCache seed dropped the trailing digits. Kept as-is here so the
// hash remains stable across calls from both the cache and the write-guard
// — any correction must update both consumers atomically.
// ---------------------------------------------------------------------------

static constexpr std::uint64_t kFnv1aSeed = 1469598103934665603ull;
static constexpr std::uint64_t kFnv1aPrime = 1099511628211ull;

inline void Fnv1aUpdateByte(std::uint64_t &InOutState,
                            const std::uint8_t InValue)
{
    InOutState ^= static_cast<std::uint64_t>(InValue);
    InOutState *= kFnv1aPrime;
}

inline void Fnv1aUpdateString(std::uint64_t &InOutState,
                              const std::string &InValue)
{
    for (const char Character : InValue)
    {
        Fnv1aUpdateByte(InOutState, static_cast<std::uint8_t>(Character));
    }
}

inline void Fnv1aUpdateUint64(std::uint64_t &InOutState, std::uint64_t InValue)
{
    for (int ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
    {
        Fnv1aUpdateByte(InOutState,
                        static_cast<std::uint8_t>(InValue & 0xFFull));
        InValue >>= 8;
    }
}

inline std::string ToHexString(const std::uint64_t InValue)
{
    std::ostringstream Stream;
    Stream << std::hex << std::nouppercase << std::setw(16) << std::setfill('0')
           << InValue;
    return Stream.str();
}

} // namespace UniPlan
