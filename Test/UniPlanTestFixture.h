#pragma once

#include "UniPlanEnums.h"
#include "UniPlanTopicTypes.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// FBundleTestFixture — shared test harness for command tests
// ---------------------------------------------------------------------------

class FBundleTestFixture : public ::testing::Test
{
  protected:
    // Repo root pointing to temp dir with Docs/Plans/ structure
    fs::path mRepoRoot;

    // Captured output after StopCapture()
    std::string mCapturedStdout;
    std::string mCapturedStderr;

    void SetUp() override;
    void TearDown() override;

    // Copy Example/Docs/Plans/<name>.Plan.json into temp repo
    void CopyFixture(const std::string &InFixtureName);

    // Build a minimal bundle in memory and write it to temp repo
    void CreateMinimalFixture(const std::string &InTopicKey,
                              UniPlan::ETopicStatus InTopicStatus,
                              int InPhaseCount,
                              UniPlan::EExecutionStatus InPhaseStatus =
                                  UniPlan::EExecutionStatus::NotStarted,
                              bool InPopulateDesign = false);

    // Redirect std::cout and std::cerr to internal buffers
    void StartCapture();
    void StopCapture();

    // Parse mCapturedStdout as JSON
    nlohmann::json ParseCapturedJSON();

    // Expected canonical repo root emitted by JSON envelopes
    std::string ExpectedJsonRepoRoot() const;
    void ExpectJsonRepoRoot(const nlohmann::json &InJson) const;

    // Read a bundle file back from the temp repo
    std::string ReadBundleFile(const std::string &InTopicKey);

    // Re-load a bundle from the temp repo into memory
    bool ReloadBundle(const std::string &InTopicKey,
                      UniPlan::FTopicBundle &OutBundle);

    // Assert ISO 8601 timestamp format
    static void AssertISOTimestamp(const std::string &InValue);
    static void AssertISODate(const std::string &InValue);
    static void AssertNoLegacyPhasePath(const std::string &InValue);

  private:
    std::ostringstream mStdoutStream;
    std::ostringstream mStderrStream;
    std::streambuf *rpStdoutOriginal = nullptr;
    std::streambuf *rpStderrOriginal = nullptr;
};
