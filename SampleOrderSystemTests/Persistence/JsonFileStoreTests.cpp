#include <gtest/gtest.h>

#include "Persistence/JsonFileStore.h"
#include "Persistence/Schema.h"
#include "Json/JsonValue.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

// A small schema unrelated to the three real tables, deliberately kept
// simple: id (string, required), quantity (integer, required, min 1).
// JsonFileStore is a generic table abstraction, so it doesn't need one of
// the "real" schemas to be exercised thoroughly.
Schema MakeTestSchema() {
    Schema schema;
    schema.tableName = "test_items";

    FieldSchema idField;
    idField.name = "id";
    idField.type = FieldType::String;
    idField.required = true;

    FieldSchema quantityField;
    quantityField.name = "quantity";
    quantityField.type = FieldType::Integer;
    quantityField.required = true;
    quantityField.min = 1.0;

    schema.fields = {idField, quantityField};
    return schema;
}

JsonValue MakeValidRecord(const std::string& id, int quantity) {
    JsonValue record = JsonValue::MakeObject();
    record.Set("id", id);
    record.Set("quantity", quantity);
    return record;
}

class JsonFileStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("JsonFileStoreTest_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
        std::filesystem::create_directories(m_testDir, ec);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
    }

    std::string FilePath(const std::string& fileName) const {
        return (m_testDir / fileName).string();
    }

    std::filesystem::path m_testDir;
};

}  // namespace

TEST_F(JsonFileStoreTest, LoadReturnsAnEmptyArrayWhenTheFileDoesNotExist) {
    JsonFileStore store(FilePath("items.json"), MakeTestSchema());

    JsonValue result = store.Load();

    EXPECT_EQ(result.Type(), JsonType::Array);
    EXPECT_TRUE(result.AsArray().empty());
}

TEST_F(JsonFileStoreTest, LoadReturnsParsedContentsWhenTheFileExistsAndIsValid) {
    const std::string path = FilePath("items.json");
    {
        std::ofstream out(path, std::ios::trunc);
        out << R"([{"id":"A","quantity":3},{"id":"B","quantity":7}])";
    }

    JsonFileStore store(path, MakeTestSchema());
    JsonValue result = store.Load();

    ASSERT_EQ(result.Type(), JsonType::Array);
    ASSERT_EQ(result.AsArray().size(), 2u);
    EXPECT_EQ(result.AsArray()[0].Get("id").AsString(), "A");
    EXPECT_EQ(result.AsArray()[0].Get("quantity").AsNumber(), 3.0);
    EXPECT_EQ(result.AsArray()[1].Get("id").AsString(), "B");
    EXPECT_EQ(result.AsArray()[1].Get("quantity").AsNumber(), 7.0);
}

TEST_F(JsonFileStoreTest, LoadThrowsWhenFileContentsAreSyntacticallyInvalidJson) {
    const std::string path = FilePath("items.json");
    {
        std::ofstream out(path, std::ios::trunc);
        out << R"([{"id":"A","quantity":3})";  // truncated: missing closing bracket
    }

    JsonFileStore store(path, MakeTestSchema());

    try {
        store.Load();
        FAIL() << "expected JsonFileStoreException to be thrown";
    } catch (const JsonFileStoreException& ex) {
        EXPECT_NE(std::string(ex.what()).find(path), std::string::npos);
    }
}

TEST_F(JsonFileStoreTest, LoadThrowsWhenOneRecordFailsSchemaValidationEvenIfOthersAreValid) {
    const std::string path = FilePath("items.json");
    {
        std::ofstream out(path, std::ios::trunc);
        // Second record has quantity below the schema's min of 1.
        out << R"([{"id":"A","quantity":3},{"id":"B","quantity":0}])";
    }

    JsonFileStore store(path, MakeTestSchema());

    EXPECT_THROW(store.Load(), JsonFileStoreException);
}

TEST_F(JsonFileStoreTest, LoadThrowsWhenTopLevelValueIsAnObjectInsteadOfAnArray) {
    const std::string path = FilePath("items.json");
    {
        std::ofstream out(path, std::ios::trunc);
        out << R"({"id":"A","quantity":3})";
    }

    JsonFileStore store(path, MakeTestSchema());

    EXPECT_THROW(store.Load(), JsonFileStoreException);
}

TEST_F(JsonFileStoreTest, SaveThenLoadOnAFreshStoreRoundTripsWhenFileDidNotPreviouslyExist) {
    const std::string path = FilePath("items.json");
    ASSERT_FALSE(std::filesystem::exists(path));

    JsonFileStore writer(path, MakeTestSchema());
    JsonValue data = JsonValue::MakeArray();
    data.Push(MakeValidRecord("X", 10));
    writer.Save(data);

    ASSERT_TRUE(std::filesystem::exists(path));

    JsonFileStore reader(path, MakeTestSchema());
    JsonValue loaded = reader.Load();

    ASSERT_EQ(loaded.AsArray().size(), 1u);
    EXPECT_EQ(loaded.AsArray()[0].Get("id").AsString(), "X");
    EXPECT_EQ(loaded.AsArray()[0].Get("quantity").AsNumber(), 10.0);
}

TEST_F(JsonFileStoreTest, SaveReplacesExistingContentsEntirelyRatherThanMerging) {
    const std::string path = FilePath("items.json");
    JsonFileStore store(path, MakeTestSchema());

    JsonValue original = JsonValue::MakeArray();
    original.Push(MakeValidRecord("OLD", 1));
    store.Save(original);

    JsonValue replacement = JsonValue::MakeArray();
    replacement.Push(MakeValidRecord("NEW", 2));
    store.Save(replacement);

    JsonValue loaded = store.Load();

    ASSERT_EQ(loaded.AsArray().size(), 1u);
    EXPECT_EQ(loaded.AsArray()[0].Get("id").AsString(), "NEW");
}

TEST_F(JsonFileStoreTest, SaveOfAnInvalidArrayThrowsAndLeavesExistingFileUntouched) {
    const std::string path = FilePath("items.json");
    JsonFileStore store(path, MakeTestSchema());

    JsonValue valid = JsonValue::MakeArray();
    valid.Push(MakeValidRecord("KEEP", 5));
    store.Save(valid);

    JsonValue invalid = JsonValue::MakeArray();
    invalid.Push(MakeValidRecord("BAD", 0));  // quantity below min of 1

    EXPECT_THROW(store.Save(invalid), JsonFileStoreException);

    JsonValue stillThere = store.Load();
    ASSERT_EQ(stillThere.AsArray().size(), 1u);
    EXPECT_EQ(stillThere.AsArray()[0].Get("id").AsString(), "KEEP");
}

TEST_F(JsonFileStoreTest, SaveCreatesTheParentDirectoryWhenItDoesNotYetExist) {
    const std::filesystem::path nestedDir = m_testDir / "nested" / "sub";
    const std::string path = (nestedDir / "items.json").string();
    ASSERT_FALSE(std::filesystem::exists(nestedDir));

    JsonFileStore store(path, MakeTestSchema());
    JsonValue data = JsonValue::MakeArray();
    data.Push(MakeValidRecord("A", 1));

    store.Save(data);

    ASSERT_TRUE(std::filesystem::exists(path));
    JsonValue loaded = store.Load();
    ASSERT_EQ(loaded.AsArray().size(), 1u);
}

TEST_F(JsonFileStoreTest, SuccessfulSaveLeavesNoStrayTempFileBehind) {
    const std::string path = FilePath("items.json");
    JsonFileStore store(path, MakeTestSchema());

    JsonValue data = JsonValue::MakeArray();
    data.Push(MakeValidRecord("A", 1));
    store.Save(data);

    EXPECT_FALSE(std::filesystem::exists(path + ".tmp"));
}
