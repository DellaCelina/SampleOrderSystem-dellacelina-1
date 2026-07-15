#include "SampleRepository.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

std::string ToLower(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

}  // namespace

SampleRepository::SampleRepository(std::filesystem::path dataPath, std::filesystem::path schemaPath)
    : store_(dataPath.string(), LoadSchemaFromFile(schemaPath.string())) {
    JsonValue data = store_.Load();
    for (const JsonValue& record : data.AsArray()) {
        cache_.push_back(Sample::FromJson(record));
    }
}

void SampleRepository::Persist() {
    JsonValue array = JsonValue::MakeArray();
    for (const Sample& sample : cache_) {
        array.Push(sample.ToJson());
    }
    store_.Save(array);
}

bool SampleRepository::Add(const Sample& sample) {
    for (const Sample& existing : cache_) {
        if (existing.sampleId == sample.sampleId) {
            return false;
        }
    }
    cache_.push_back(sample);
    Persist();
    return true;
}

std::vector<Sample> SampleRepository::FindAll() const {
    return cache_;
}

std::optional<Sample> SampleRepository::FindById(const std::string& sampleId) const {
    for (const Sample& sample : cache_) {
        if (sample.sampleId == sampleId) {
            return sample;
        }
    }
    return std::nullopt;
}

std::vector<Sample> SampleRepository::FindByNameSubstring(const std::string& needle) const {
    std::vector<Sample> results;
    std::string lowerNeedle = ToLower(needle);
    for (const Sample& sample : cache_) {
        if (ToLower(sample.name).find(lowerNeedle) != std::string::npos) {
            results.push_back(sample);
        }
    }
    return results;
}

void SampleRepository::IncreaseStock(const std::string& sampleId, int amount) {
    if (amount <= 0) {
        throw std::invalid_argument("SampleRepository::IncreaseStock: amount must be > 0");
    }
    for (Sample& sample : cache_) {
        if (sample.sampleId == sampleId) {
            sample.currentStock += amount;
            Persist();
            return;
        }
    }
    throw std::invalid_argument("SampleRepository::IncreaseStock: unknown sampleId \"" + sampleId + "\"");
}

void SampleRepository::DecreaseStock(const std::string& sampleId, int amount) {
    if (amount <= 0) {
        throw std::invalid_argument("SampleRepository::DecreaseStock: amount must be > 0");
    }
    for (Sample& sample : cache_) {
        if (sample.sampleId == sampleId) {
            if (sample.currentStock - amount < 0) {
                throw std::invalid_argument("SampleRepository::DecreaseStock: would take stock negative for \"" +
                                             sampleId + "\"");
            }
            sample.currentStock -= amount;
            Persist();
            return;
        }
    }
    throw std::invalid_argument("SampleRepository::DecreaseStock: unknown sampleId \"" + sampleId + "\"");
}
