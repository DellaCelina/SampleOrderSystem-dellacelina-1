#include <gtest/gtest.h>

#include "Repositories/SampleRepository.h"
#include "Models/Sample.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

// Mirrors schema/sample.schema.json exactly.
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

Sample MakeSample(const std::string& id, const std::string& name, int stock) {
    Sample s;
    s.sampleId = id;
    s.name = name;
    s.averageProductionTimeMinutes = 30;
    s.yield = 0.9;
    s.currentStock = stock;
    return s;
}

// Fixture: every test gets its own scratch temp directory (never the real
// data/samples.json or schema/sample.schema.json the running app uses), torn
// down afterward, per phase-5's test-isolation requirement.
class SampleRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("SampleRepositoryTest_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
        std::filesystem::create_directories(m_testDir, ec);

        m_schemaPath = m_testDir / "sample.schema.json";
        std::ofstream schemaOut(m_schemaPath, std::ios::trunc);
        schemaOut << kSampleSchemaJson;
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
    }

    std::filesystem::path DataPath(const std::string& fileName = "samples.json") const {
        return m_testDir / fileName;
    }

    std::filesystem::path SchemaPath() const { return m_schemaPath; }

    void WriteRawDataFile(const std::string& contents, const std::string& fileName = "samples.json") const {
        std::ofstream out(DataPath(fileName), std::ios::trunc);
        out << contents;
    }

    std::filesystem::path m_testDir;
    std::filesystem::path m_schemaPath;
};

}  // namespace

TEST_F(SampleRepositoryTest, ConstructingAgainstANonExistentFileYieldsAnEmptyTableWithoutThrowing) {
    SampleRepository repo(DataPath(), SchemaPath());

    EXPECT_TRUE(repo.FindAll().empty());
}

TEST_F(SampleRepositoryTest, AddThenFindByIdRoundTripsAllFieldsThroughTheInMemoryCache) {
    SampleRepository repo(DataPath(), SchemaPath());

    Sample sample = MakeSample("SMP-001", "GaAs Wafer", 100);
    EXPECT_TRUE(repo.Add(sample));

    std::optional<Sample> found = repo.FindById("SMP-001");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->sampleId, sample.sampleId);
    EXPECT_EQ(found->name, sample.name);
    EXPECT_EQ(found->averageProductionTimeMinutes, sample.averageProductionTimeMinutes);
    EXPECT_EQ(found->yield, sample.yield);
    EXPECT_EQ(found->currentStock, sample.currentStock);
}

TEST_F(SampleRepositoryTest, AddingADuplicateSampleIdReturnsFalseAndLeavesTheOriginalRecordUnchanged) {
    SampleRepository repo(DataPath(), SchemaPath());

    ASSERT_TRUE(repo.Add(MakeSample("SMP-001", "GaAs Wafer", 100)));

    Sample duplicate = MakeSample("SMP-001", "Different Name", 999);
    EXPECT_FALSE(repo.Add(duplicate));

    std::optional<Sample> found = repo.FindById("SMP-001");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "GaAs Wafer");
    EXPECT_EQ(found->currentStock, 100);
    EXPECT_EQ(repo.FindAll().size(), 1u);
}

TEST_F(SampleRepositoryTest, FindByIdReturnsNulloptForAnUnknownSampleId) {
    SampleRepository repo(DataPath(), SchemaPath());
    ASSERT_TRUE(repo.Add(MakeSample("SMP-001", "GaAs Wafer", 100)));

    EXPECT_EQ(repo.FindById("SMP-999"), std::nullopt);
}

TEST_F(SampleRepositoryTest, FindByNameSubstringMatchesCaseInsensitivelyAndExcludesUnrelatedNames) {
    SampleRepository repo(DataPath(), SchemaPath());
    ASSERT_TRUE(repo.Add(MakeSample("SMP-001", "GaAs Wafer", 100)));
    ASSERT_TRUE(repo.Add(MakeSample("SMP-002", "Silicon Ingot", 50)));

    std::vector<Sample> matches = repo.FindByNameSubstring("gaas");

    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0].sampleId, "SMP-001");
}

TEST_F(SampleRepositoryTest, FindByNameSubstringMatchesMultipleQualifyingSamples) {
    SampleRepository repo(DataPath(), SchemaPath());
    ASSERT_TRUE(repo.Add(MakeSample("SMP-001", "GaAs Wafer A", 100)));
    ASSERT_TRUE(repo.Add(MakeSample("SMP-002", "GaAs Wafer B", 50)));
    ASSERT_TRUE(repo.Add(MakeSample("SMP-003", "Silicon Ingot", 10)));

    std::vector<Sample> matches = repo.FindByNameSubstring("wafer");

    EXPECT_EQ(matches.size(), 2u);
}

TEST_F(SampleRepositoryTest, FindByNameSubstringReturnsEmptyWhenNoSampleQualifies) {
    SampleRepository repo(DataPath(), SchemaPath());
    ASSERT_TRUE(repo.Add(MakeSample("SMP-001", "GaAs Wafer", 100)));

    EXPECT_TRUE(repo.FindByNameSubstring("nonexistent").empty());
}

TEST_F(SampleRepositoryTest, FindByNameSubstringWithAnEmptyNeedleMatchesEverySample) {
    SampleRepository repo(DataPath(), SchemaPath());
    ASSERT_TRUE(repo.Add(MakeSample("SMP-001", "GaAs Wafer", 100)));
    ASSERT_TRUE(repo.Add(MakeSample("SMP-002", "Silicon Ingot", 50)));

    EXPECT_EQ(repo.FindByNameSubstring("").size(), 2u);
}

TEST_F(SampleRepositoryTest, IncreaseStockOnAnExistingSampleUpdatesCurrentStockVisibleViaFindById) {
    SampleRepository repo(DataPath(), SchemaPath());
    ASSERT_TRUE(repo.Add(MakeSample("SMP-001", "GaAs Wafer", 100)));

    repo.IncreaseStock("SMP-001", 25);

    EXPECT_EQ(repo.FindById("SMP-001")->currentStock, 125);
}

TEST_F(SampleRepositoryTest, DecreaseStockOnAnExistingSampleUpdatesCurrentStockVisibleViaFindById) {
    SampleRepository repo(DataPath(), SchemaPath());
    ASSERT_TRUE(repo.Add(MakeSample("SMP-001", "GaAs Wafer", 100)));

    repo.DecreaseStock("SMP-001", 40);

    EXPECT_EQ(repo.FindById("SMP-001")->currentStock, 60);
}

TEST_F(SampleRepositoryTest, IncreaseStockOnAnUnknownSampleIdThrowsInvalidArgument) {
    SampleRepository repo(DataPath(), SchemaPath());

    EXPECT_THROW(repo.IncreaseStock("SMP-DOES-NOT-EXIST", 10), std::invalid_argument);
}

TEST_F(SampleRepositoryTest, DecreaseStockOnAnUnknownSampleIdThrowsInvalidArgument) {
    SampleRepository repo(DataPath(), SchemaPath());

    EXPECT_THROW(repo.DecreaseStock("SMP-DOES-NOT-EXIST", 10), std::invalid_argument);
}

TEST_F(SampleRepositoryTest, DecreaseStockPastZeroThrowsAndLeavesStockUnchanged) {
    SampleRepository repo(DataPath(), SchemaPath());
    ASSERT_TRUE(repo.Add(MakeSample("SMP-001", "GaAs Wafer", 10)));

    EXPECT_THROW(repo.DecreaseStock("SMP-001", 11), std::invalid_argument);

    EXPECT_EQ(repo.FindById("SMP-001")->currentStock, 10);
}

TEST_F(SampleRepositoryTest, PersistenceRoundTripAcrossTwoIndependentRepositoryInstances) {
    {
        SampleRepository writer(DataPath(), SchemaPath());
        ASSERT_TRUE(writer.Add(MakeSample("SMP-001", "GaAs Wafer", 100)));
        ASSERT_TRUE(writer.Add(MakeSample("SMP-002", "Silicon Ingot", 50)));
        writer.IncreaseStock("SMP-002", 15);
    }

    SampleRepository reader(DataPath(), SchemaPath());
    std::vector<Sample> all = reader.FindAll();

    ASSERT_EQ(all.size(), 2u);
    std::optional<Sample> first = reader.FindById("SMP-001");
    std::optional<Sample> second = reader.FindById("SMP-002");
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->name, "GaAs Wafer");
    EXPECT_EQ(first->currentStock, 100);
    EXPECT_EQ(second->name, "Silicon Ingot");
    EXPECT_EQ(second->currentStock, 65);
}

TEST_F(SampleRepositoryTest, ConstructingAgainstAFileWithARecordMissingARequiredFieldPropagatesTheLoadFailure) {
    WriteRawDataFile(R"([{"sampleId":"SMP-001","name":"GaAs Wafer","averageProductionTimeMinutes":30,"currentStock":100}])");
    // Missing required "yield" field.

    EXPECT_THROW(SampleRepository(DataPath(), SchemaPath()), std::exception);
}

TEST_F(SampleRepositoryTest, ConstructingAgainstAFileWithAYieldOutOfTheSchemasAllowedRangePropagatesTheLoadFailure) {
    WriteRawDataFile(
        R"([{"sampleId":"SMP-001","name":"GaAs Wafer","averageProductionTimeMinutes":30,"yield":1.5,"currentStock":100}])");
    // yield above schema's max of 1.

    EXPECT_THROW(SampleRepository(DataPath(), SchemaPath()), std::exception);
}
