#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJsonIO.h"
#include "UniPlanTopicTypes.h"

#include <fstream>
#include <regex>

// ---------------------------------------------------------------------------
// SetUp / TearDown — temp directory lifecycle
// ---------------------------------------------------------------------------

void FBundleTestFixture::SetUp()
{
    const fs::path TmpRoot = fs::path(UPLAN_TEST_SOURCE_DIR) / ".tmp";
    fs::create_directories(TmpRoot);
    const std::string Template = (TmpRoot / "test-XXXXXX").string();
    std::vector<char> Buffer(Template.begin(), Template.end());
    Buffer.push_back('\0');
    const char *Result = mkdtemp(Buffer.data());
    ASSERT_NE(Result, nullptr) << "mkdtemp failed";
    mRepoRoot = fs::path(Result);
    fs::create_directories(mRepoRoot / "Docs" / "Plans");
}

void FBundleTestFixture::TearDown()
{
    // Restore streams in case test forgot to call StopCapture
    if (rpStdoutOriginal != nullptr)
    {
        std::cout.rdbuf(rpStdoutOriginal);
        rpStdoutOriginal = nullptr;
    }
    if (rpStderrOriginal != nullptr)
    {
        std::cerr.rdbuf(rpStderrOriginal);
        rpStderrOriginal = nullptr;
    }

    if (!mRepoRoot.empty() && fs::exists(mRepoRoot))
    {
        std::error_code Error;
        fs::remove_all(mRepoRoot, Error);
    }
}

// ---------------------------------------------------------------------------
// Fixture helpers
// ---------------------------------------------------------------------------

void FBundleTestFixture::CopyFixture(const std::string &InFixtureName)
{
    const fs::path Source = fs::path(UPLAN_TEST_FIXTURE_DIR) / "Docs" /
                            "Plans" / (InFixtureName + ".Plan.json");
    const fs::path Dest =
        mRepoRoot / "Docs" / "Plans" / (InFixtureName + ".Plan.json");
    ASSERT_TRUE(fs::exists(Source)) << "Fixture not found: " << Source.string();
    fs::copy_file(Source, Dest, fs::copy_options::overwrite_existing);
}

void FBundleTestFixture::CreateMinimalFixture(
    const std::string &InTopicKey, UniPlan::ETopicStatus InTopicStatus,
    int InPhaseCount, UniPlan::EExecutionStatus InPhaseStatus,
    bool InPopulateDesign)
{
    UniPlan::FTopicBundle Bundle;
    Bundle.mTopicKey = InTopicKey;
    Bundle.mStatus = InTopicStatus;
    Bundle.mMetadata.mTitle = "Test topic: " + InTopicKey;
    Bundle.mMetadata.mSummary = "Minimal fixture for testing";

    for (int I = 0; I < InPhaseCount; ++I)
    {
        UniPlan::FPhaseRecord Phase;
        Phase.mScope = "Phase " + std::to_string(I);
        Phase.mLifecycle.mStatus = InPhaseStatus;
        if (InPopulateDesign)
        {
            Phase.mDesign.mInvestigation = "Test investigation";
            Phase.mDesign.mCodeEntityContract = "Test contract";
        }
        Bundle.mPhases.push_back(std::move(Phase));
    }

    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / (InTopicKey + ".Plan.json");
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;
}

// ---------------------------------------------------------------------------
// stdout/stderr capture
// ---------------------------------------------------------------------------

void FBundleTestFixture::StartCapture()
{
    mStdoutStream.str("");
    mStderrStream.str("");
    rpStdoutOriginal = std::cout.rdbuf(mStdoutStream.rdbuf());
    rpStderrOriginal = std::cerr.rdbuf(mStderrStream.rdbuf());
}

void FBundleTestFixture::StopCapture()
{
    if (rpStdoutOriginal != nullptr)
    {
        std::cout.rdbuf(rpStdoutOriginal);
        rpStdoutOriginal = nullptr;
    }
    if (rpStderrOriginal != nullptr)
    {
        std::cerr.rdbuf(rpStderrOriginal);
        rpStderrOriginal = nullptr;
    }
    mCapturedStdout = mStdoutStream.str();
    mCapturedStderr = mStderrStream.str();
}

nlohmann::json FBundleTestFixture::ParseCapturedJSON()
{
    return nlohmann::json::parse(mCapturedStdout);
}

std::string FBundleTestFixture::ReadBundleFile(const std::string &InTopicKey)
{
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / (InTopicKey + ".Plan.json");
    std::ifstream Stream(Path);
    return std::string(std::istreambuf_iterator<char>(Stream),
                       std::istreambuf_iterator<char>());
}

bool FBundleTestFixture::ReloadBundle(const std::string &InTopicKey,
                                      UniPlan::FTopicBundle &OutBundle)
{
    std::string Error;
    return UniPlan::TryLoadBundleByTopic(mRepoRoot, InTopicKey, OutBundle,
                                         Error);
}

// ---------------------------------------------------------------------------
// Timestamp assertions
// ---------------------------------------------------------------------------

void FBundleTestFixture::AssertISOTimestamp(const std::string &InValue)
{
    ASSERT_FALSE(InValue.empty()) << "Timestamp is empty";
    EXPECT_TRUE(std::regex_match(
        InValue, std::regex(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)")))
        << "Not ISO 8601: " << InValue;
}

void FBundleTestFixture::AssertISODate(const std::string &InValue)
{
    ASSERT_FALSE(InValue.empty()) << "Date is empty";
    EXPECT_TRUE(std::regex_match(InValue, std::regex(R"(\d{4}-\d{2}-\d{2})")))
        << "Not ISO date: " << InValue;
}

void FBundleTestFixture::AssertNoLegacyPhasePath(const std::string &InValue)
{
    EXPECT_EQ(InValue.find("phases["), std::string::npos)
        << "Found legacy path in: " << InValue;
}
