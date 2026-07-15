#include "JsonFileStore.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "../Json/JsonParser.h"
#include "../Json/JsonWriter.h"
#include "SchemaValidator.h"

JsonFileStore::JsonFileStore(std::string filePath, Schema schema)
    : filePath_(std::move(filePath)), schema_(std::move(schema)) {}

JsonValue JsonFileStore::Load() const {
    std::error_code existsEc;
    if (!std::filesystem::exists(filePath_, existsEc)) {
        return JsonValue::MakeArray();
    }

    std::ifstream in(filePath_, std::ios::binary);
    if (!in) {
        throw JsonFileStoreException("JsonFileStore::Load: could not open file \"" + filePath_ + "\"");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    in.close();

    JsonValue data;
    try {
        data = JsonParser::Parse(buffer.str());
    } catch (const JsonParseException& ex) {
        throw JsonFileStoreException("JsonFileStore::Load: failed to parse \"" + filePath_ + "\": " + ex.what());
    }

    std::optional<ValidationError> error = ::Validate(data, schema_);
    if (error.has_value()) {
        throw JsonFileStoreException("JsonFileStore::Load: \"" + filePath_ +
                                      "\" failed schema validation: " + error->message);
    }

    return data;
}

void JsonFileStore::Save(const JsonValue& data) const {
    std::optional<ValidationError> error = ::Validate(data, schema_);
    if (error.has_value()) {
        throw JsonFileStoreException("JsonFileStore::Save: data for \"" + filePath_ +
                                      "\" failed schema validation: " + error->message);
    }

    std::filesystem::path targetPath(filePath_);
    std::filesystem::path parentDir = targetPath.parent_path();
    if (!parentDir.empty()) {
        std::error_code dirEc;
        std::filesystem::create_directories(parentDir, dirEc);
        if (dirEc) {
            throw JsonFileStoreException("JsonFileStore::Save: could not create directory \"" + parentDir.string() +
                                          "\": " + dirEc.message());
        }
    }

    std::filesystem::path tempPath = targetPath;
    tempPath += ".tmp";

    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw JsonFileStoreException("JsonFileStore::Save: could not open temp file \"" + tempPath.string() +
                                          "\" for writing");
        }
        out << JsonWriter::Write(data);
        out.flush();
        if (!out) {
            throw JsonFileStoreException("JsonFileStore::Save: failed to write temp file \"" + tempPath.string() +
                                          "\"");
        }
    }

    std::error_code renameEc;
    std::filesystem::rename(tempPath, targetPath, renameEc);
    if (renameEc) {
        throw JsonFileStoreException("JsonFileStore::Save: could not replace \"" + filePath_ +
                                      "\" with temp file: " + renameEc.message());
    }
}
