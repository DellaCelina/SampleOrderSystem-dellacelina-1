#include "SampleController.h"

#include <optional>
#include <string>
#include <vector>

namespace {

bool TryParseInt(const std::string& text, int& outValue) {
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t pos = 0;
        const int value = std::stoi(text, &pos);
        if (pos != text.size()) {
            return false;
        }
        outValue = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool TryParseDouble(const std::string& text, double& outValue) {
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t pos = 0;
        const double value = std::stod(text, &pos);
        if (pos != text.size()) {
            return false;
        }
        outValue = value;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

SampleController::SampleController(SampleRepository& repository, SampleView& view)
    : repository_(repository), view_(view) {}

void SampleController::HandleRegister() {
    const std::string sampleId = view_.PromptLine("Enter sample ID: ");
    if (sampleId.empty()) {
        view_.ShowError("Sample ID must not be empty");
        return;
    }

    const std::string name = view_.PromptLine("Enter sample name: ");
    if (name.empty()) {
        view_.ShowError("Sample name must not be empty");
        return;
    }

    const std::string avgTimeText = view_.PromptLine("Enter average production time (minutes): ");
    int averageProductionTimeMinutes = 0;
    if (!TryParseInt(avgTimeText, averageProductionTimeMinutes) || averageProductionTimeMinutes <= 0) {
        view_.ShowError("Average production time must be a positive integer");
        return;
    }

    const std::string yieldText = view_.PromptLine("Enter yield (0 < yield <= 1): ");
    double yield = 0.0;
    if (!TryParseDouble(yieldText, yield) || !(yield > 0.0 && yield <= 1.0)) {
        view_.ShowError("Yield must be a number greater than 0 and at most 1");
        return;
    }

    Sample sample;
    sample.sampleId = sampleId;
    sample.name = name;
    sample.averageProductionTimeMinutes = averageProductionTimeMinutes;
    sample.yield = yield;
    sample.currentStock = 0;

    if (repository_.Add(sample)) {
        view_.ShowRegistrationSuccess(sample);
    } else {
        view_.ShowError("Sample ID already exists: " + sampleId);
    }
}

void SampleController::HandleListAll() {
    view_.ShowSampleList(repository_.FindAll());
}

void SampleController::HandleSearch() {
    const std::string mode = view_.PromptLine("1) By ID  2) By name substring - choose: ");
    if (mode != "1" && mode != "2") {
        view_.ShowError("Invalid search option");
        return;
    }

    const std::string term = view_.PromptLine("Enter search term: ");

    if (mode == "1") {
        std::optional<Sample> found = repository_.FindById(term);
        std::vector<Sample> results;
        if (found.has_value()) {
            results.push_back(*found);
        }
        view_.ShowSampleList(results);
    } else {
        view_.ShowSampleList(repository_.FindByNameSubstring(term));
    }
}
