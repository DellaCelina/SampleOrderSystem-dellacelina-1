#pragma once
#include <stdexcept>
#include <string>

#include "../Json/JsonValue.h"
#include "Schema.h"

class JsonFileStoreException : public std::runtime_error {
public:
    explicit JsonFileStoreException(const std::string& message) : std::runtime_error(message) {}
};

class JsonFileStore {
public:
    // filePath: path to the table's JSON file (e.g. "data/samples.json").
    // schema: the already-parsed Schema for this table.
    JsonFileStore(std::string filePath, Schema schema);

    // Reads the file, parses it as JSON, and validates the resulting value
    // as a JSON array against `schema`.
    // - If the file does not exist: returns an empty JSON array.
    // - If the file exists but fails to parse or fails schema validation:
    //   throws JsonFileStoreException naming the file path.
    JsonValue Load() const;

    // Validates `data` against `schema`, then atomically writes it to the
    // table's file (temp file + rename). Throws JsonFileStoreException and
    // leaves the existing file untouched if validation or any I/O step
    // fails.
    void Save(const JsonValue& data) const;

    const Schema& GetSchema() const { return schema_; }
    const std::string& GetFilePath() const { return filePath_; }

private:
    std::string filePath_;
    Schema schema_;
};
