#include <gtest/gtest.h>

#include "Controllers/SampleController.h"
#include "Views/SampleView.h"
#include "Repositories/SampleRepository.h"
#include "Models/Sample.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Global namespace per phase-10's DETAIL.md correction note (matches phase-1/2/4/5's
// real committed code: no Views::/Controllers::/Repositories:: wrappers anywhere).

// Mirrors schema/sample.schema.json exactly (same fixture approach as
// SampleRepositoryTests.cpp, since this phase couples directly to the real repository).
const char* kSampleSchemaJson = R"({
  "table": "samples",
  "fields": [
    { "name": "sampleId", "type": "string", "required": true },
    { "name": "name", "type": "string", "required": true },
    { "name": "averageProductionTimeMinutes", "type": "integer", "required": true, "min": 1 },
    { "name": "yield", "type": "number", "required": true, "exclusiveMin": 0, "max": 1 },
    { "name": "currentStock", "type": "integer", "required": true, "min": 0 }
  ]
})";

Sample MakeSample(const std::string& id, const std::string& name, int avgTime, double yield, int stock) {
    Sample s;
    s.sampleId = id;
    s.name = name;
    s.averageProductionTimeMinutes = avgTime;
    s.yield = yield;
    s.currentStock = stock;
    return s;
}

// Joins lines with '\n' to script sequential PromptLine() calls, e.g. for
// HandleRegister's [sampleId, name, avgTime, yield] prompt sequence.
std::string ScriptedInput(const std::vector<std::string>& lines) {
    std::string result;
    for (const std::string& line : lines) {
        result += line;
        result += '\n';
    }
    return result;
}

class SampleControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("SampleControllerTest_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
        std::filesystem::create_directories(m_testDir, ec);

        m_schemaPath = m_testDir / "sample.schema.json";
        std::ofstream schemaOut(m_schemaPath, std::ios::trunc);
        schemaOut << kSampleSchemaJson;

        m_dataPath = m_testDir / "samples.json";
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
    }

    std::filesystem::path m_testDir;
    std::filesystem::path m_schemaPath;
    std::filesystem::path m_dataPath;
};

}  // namespace

// ---- HandleRegister ----

TEST_F(SampleControllerTest, HandleRegisterWithValidInputCreatesSampleWithZeroStockAndShowsSuccess) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    std::istringstream in(ScriptedInput({ "SMP-001", "GaAs Wafer", "10", "0.9" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleRegister();

    std::optional<Sample> found = repository.FindById("SMP-001");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "GaAs Wafer");
    EXPECT_EQ(found->averageProductionTimeMinutes, 10);
    EXPECT_DOUBLE_EQ(found->yield, 0.9);
    EXPECT_EQ(found->currentStock, 0);
    EXPECT_FALSE(out.str().empty());
}

TEST_F(SampleControllerTest, HandleRegisterWithDuplicateSampleIdLeavesOriginalRecordUnchangedAndShowsError) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    ASSERT_TRUE(repository.Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));

    std::istringstream in(ScriptedInput({ "SMP-001", "Different Name", "99", "0.1" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleRegister();

    std::optional<Sample> found = repository.FindById("SMP-001");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "GaAs Wafer");
    EXPECT_EQ(found->averageProductionTimeMinutes, 30);
    EXPECT_DOUBLE_EQ(found->yield, 0.9);
    EXPECT_EQ(found->currentStock, 100);
    EXPECT_EQ(repository.FindAll().size(), 1u);
}

TEST_F(SampleControllerTest, HandleRegisterWithNonNumericAverageProductionTimeRejectsWithoutCreatingARecord) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    std::istringstream in(ScriptedInput({ "SMP-001", "GaAs Wafer", "abc", "0.9" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleRegister();

    EXPECT_EQ(repository.FindById("SMP-001"), std::nullopt);
    EXPECT_TRUE(repository.FindAll().empty());
    EXPECT_FALSE(out.str().empty());
}

TEST_F(SampleControllerTest, HandleRegisterWithNonPositiveAverageProductionTimeRejectsForEachInvalidValue) {
    const std::vector<std::string> invalidValues = { "0", "-5" };
    int counter = 0;
    for (const std::string& value : invalidValues) {
        const std::string id = "SMP-BAD-TIME-" + std::to_string(counter++);
        SampleRepository repository(m_dataPath, m_schemaPath);
        std::istringstream in(ScriptedInput({ id, "Some Name", value, "0.9" }));
        std::ostringstream out;
        SampleView view(in, out);
        SampleController controller(repository, view);

        controller.HandleRegister();

        EXPECT_EQ(repository.FindById(id), std::nullopt) << "average production time value: " << value;
        EXPECT_FALSE(out.str().empty()) << "average production time value: " << value;
    }
}

TEST_F(SampleControllerTest, HandleRegisterWithNonNumericYieldRejectsWithoutCreatingARecord) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    std::istringstream in(ScriptedInput({ "SMP-001", "GaAs Wafer", "10", "abc" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleRegister();

    EXPECT_EQ(repository.FindById("SMP-001"), std::nullopt);
    EXPECT_TRUE(repository.FindAll().empty());
    EXPECT_FALSE(out.str().empty());
}

TEST_F(SampleControllerTest, HandleRegisterWithYieldOutOfRangeRejectsForEachInvalidValue) {
    const std::vector<std::string> invalidValues = { "0", "-0.2", "1.5" };
    int counter = 0;
    for (const std::string& value : invalidValues) {
        const std::string id = "SMP-BAD-YIELD-" + std::to_string(counter++);
        SampleRepository repository(m_dataPath, m_schemaPath);
        std::istringstream in(ScriptedInput({ id, "Some Name", "10", value }));
        std::ostringstream out;
        SampleView view(in, out);
        SampleController controller(repository, view);

        controller.HandleRegister();

        EXPECT_EQ(repository.FindById(id), std::nullopt) << "yield value: " << value;
        EXPECT_FALSE(out.str().empty()) << "yield value: " << value;
    }
}

TEST_F(SampleControllerTest, HandleRegisterWithYieldExactlyOneIsAcceptedAsTheValidBoundary) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    std::istringstream in(ScriptedInput({ "SMP-001", "GaAs Wafer", "10", "1" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleRegister();

    std::optional<Sample> found = repository.FindById("SMP-001");
    ASSERT_TRUE(found.has_value());
    EXPECT_DOUBLE_EQ(found->yield, 1.0);
}

TEST_F(SampleControllerTest, HandleRegisterWithBlankSampleIdRejectsWithoutCreatingARecord) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    std::istringstream in(ScriptedInput({ "   ", "GaAs Wafer", "10", "0.9" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleRegister();

    EXPECT_TRUE(repository.FindAll().empty());
    EXPECT_FALSE(out.str().empty());
}

TEST_F(SampleControllerTest, HandleRegisterWithBlankNameRejectsWithoutCreatingARecord) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    std::istringstream in(ScriptedInput({ "SMP-001", "   ", "10", "0.9" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleRegister();

    EXPECT_EQ(repository.FindById("SMP-001"), std::nullopt);
    EXPECT_TRUE(repository.FindAll().empty());
    EXPECT_FALSE(out.str().empty());
}

// ---- HandleListAll ----

TEST_F(SampleControllerTest, HandleListAllWithAnEmptyRepositoryShowsTheNoSamplesMessage) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    std::istringstream in;
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleListAll();

    std::ostringstream expected;
    SampleView expectedView(in, expected);
    expectedView.ShowNoSamples();
    EXPECT_EQ(out.str(), expected.str());
}

TEST_F(SampleControllerTest, HandleListAllWithASeededRepositoryShowsEverySample) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    ASSERT_TRUE(repository.Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    ASSERT_TRUE(repository.Add(MakeSample("SMP-002", "Silicon Ingot", 45, 0.8, 50)));

    std::istringstream in;
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleListAll();

    const std::string text = out.str();
    EXPECT_NE(text.find("SMP-001"), std::string::npos);
    EXPECT_NE(text.find("SMP-002"), std::string::npos);
}

// ---- HandleSearch ----

TEST_F(SampleControllerTest, HandleSearchByIdWithAnExistingIdShowsExactlyThatSample) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    ASSERT_TRUE(repository.Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    ASSERT_TRUE(repository.Add(MakeSample("SMP-002", "Silicon Ingot", 45, 0.8, 50)));

    std::istringstream in(ScriptedInput({ "1", "SMP-001" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleSearch();

    const std::string text = out.str();
    EXPECT_NE(text.find("SMP-001"), std::string::npos);
    EXPECT_EQ(text.find("SMP-002"), std::string::npos);
}

TEST_F(SampleControllerTest, HandleSearchByIdWithANonExistentIdShowsTheNoSamplesMessage) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    ASSERT_TRUE(repository.Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));

    std::istringstream in(ScriptedInput({ "1", "SMP-999" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleSearch();

    std::ostringstream expectedTail;
    SampleView expectedView(in, expectedTail);
    expectedView.ShowNoSamples();
    EXPECT_NE(out.str().find(expectedTail.str()), std::string::npos);
}

TEST_F(SampleControllerTest, HandleSearchByNameSubstringMatchesCaseInsensitivelyAndExcludesUnrelatedNames) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    ASSERT_TRUE(repository.Add(MakeSample("SMP-001", "Widget", 30, 0.9, 100)));
    ASSERT_TRUE(repository.Add(MakeSample("SMP-002", "widget-2", 20, 0.5, 20)));
    ASSERT_TRUE(repository.Add(MakeSample("SMP-003", "Gadget", 15, 0.7, 10)));

    std::istringstream in(ScriptedInput({ "2", "widget" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleSearch();

    const std::string text = out.str();
    EXPECT_NE(text.find("Widget"), std::string::npos);
    EXPECT_NE(text.find("widget-2"), std::string::npos);
    EXPECT_EQ(text.find("Gadget"), std::string::npos);
}

TEST_F(SampleControllerTest, HandleSearchByNameSubstringWithNoMatchesShowsTheNoSamplesMessage) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    ASSERT_TRUE(repository.Add(MakeSample("SMP-001", "Widget", 30, 0.9, 100)));

    std::istringstream in(ScriptedInput({ "2", "nonexistent" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleSearch();

    std::ostringstream expectedTail;
    SampleView expectedView(in, expectedTail);
    expectedView.ShowNoSamples();
    EXPECT_NE(out.str().find(expectedTail.str()), std::string::npos);
}

TEST_F(SampleControllerTest, HandleSearchWithAnInvalidModeShowsAnErrorAndNeverQueriesTheRepository) {
    SampleRepository repository(m_dataPath, m_schemaPath);
    ASSERT_TRUE(repository.Add(MakeSample("SMP-001", "Widget", 30, 0.9, 100)));

    std::istringstream in(ScriptedInput({ "3" }));
    std::ostringstream out;
    SampleView view(in, out);
    SampleController controller(repository, view);

    controller.HandleSearch();

    const std::string text = out.str();
    EXPECT_EQ(text.find("SMP-001"), std::string::npos);
    EXPECT_EQ(text.find("Widget"), std::string::npos);
    EXPECT_FALSE(text.empty());
}
