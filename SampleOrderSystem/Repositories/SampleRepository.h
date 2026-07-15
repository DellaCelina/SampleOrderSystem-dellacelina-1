#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../Models/Sample.h"
#include "../Persistence/JsonFileStore.h"

class SampleRepository {
public:
    explicit SampleRepository(std::filesystem::path dataPath = "data/samples.json",
                               std::filesystem::path schemaPath = "schema/sample.schema.json");

    // Returns false (no mutation, no exception) if sampleId already exists.
    bool Add(const Sample& sample);

    std::vector<Sample> FindAll() const;

    std::optional<Sample> FindById(const std::string& sampleId) const;

    std::vector<Sample> FindByNameSubstring(const std::string& needle) const;

    void IncreaseStock(const std::string& sampleId, int amount);
    void DecreaseStock(const std::string& sampleId, int amount);

private:
    JsonFileStore store_;
    std::vector<Sample> cache_;

    void Persist();
};
