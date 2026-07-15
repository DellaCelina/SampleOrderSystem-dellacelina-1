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
    const std::string sampleId = view_.PromptLine("시료 ID > ");
    if (sampleId.empty()) {
        view_.ShowError("시료 ID는 비어 있을 수 없습니다");
        return;
    }

    const std::string name = view_.PromptLine("시료명 > ");
    if (name.empty()) {
        view_.ShowError("시료명은 비어 있을 수 없습니다");
        return;
    }

    const std::string avgTimeText = view_.PromptLine("평균 생산시간(분) > ");
    int averageProductionTimeMinutes = 0;
    if (!TryParseInt(avgTimeText, averageProductionTimeMinutes) || averageProductionTimeMinutes <= 0) {
        view_.ShowError("평균 생산시간은 양의 정수여야 합니다");
        return;
    }

    const std::string yieldText = view_.PromptLine("수율(0~1) > ");
    double yield = 0.0;
    if (!TryParseDouble(yieldText, yield) || !(yield > 0.0 && yield <= 1.0)) {
        view_.ShowError("수율은 0보다 크고 1 이하인 숫자여야 합니다");
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
        view_.ShowError("이미 존재하는 시료 ID입니다: " + sampleId);
    }
}

void SampleController::HandleListAll() {
    view_.ShowSampleList(repository_.FindAll());
}

void SampleController::HandleSearch() {
    const std::string mode = view_.PromptLine("1) ID로 검색  2) 시료명 일부로 검색 - 선택 > ");
    if (mode != "1" && mode != "2") {
        view_.ShowError("올바르지 않은 검색 옵션입니다");
        return;
    }

    const std::string term = view_.PromptLine("검색어 > ");

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
