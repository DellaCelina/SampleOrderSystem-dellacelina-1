#include "SampleView.h"

#include "../Core/Console.h"

namespace {

std::string Trim(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

}  // namespace

SampleView::SampleView(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

void SampleView::WriteHeader(const std::string& title) const {
    out_ << HeaderBlock() << title << "\n" << SeparatorLine() << "\n";
}

void SampleView::ShowNoSamples() const {
    WriteHeader("시료 목록 조회");
    out_ << "등록된 시료가 없습니다.\n";
}

void SampleView::ShowSampleList(const std::vector<Sample>& samples) const {
    if (samples.empty()) {
        ShowNoSamples();
        return;
    }

    WriteHeader("시료 목록 조회");
    out_ << "ID | 시료명 | 평균 생산시간 | 수율 | 현재 재고\n";
    for (const Sample& sample : samples) {
        out_ << sample.sampleId << " | "
             << sample.name << " | "
             << sample.averageProductionTimeMinutes << " | "
             << sample.yield << " | "
             << sample.currentStock << "\n";
    }
}

void SampleView::ShowRegistrationSuccess(const Sample& sample) const {
    out_ << "시료가 등록되었습니다: " << sample.sampleId << " (" << sample.name << ")\n";
}

void SampleView::ShowError(const std::string& message) const {
    out_ << "오류: " << message << "\n";
}

std::string SampleView::PromptLine(const std::string& promptText) const {
    out_ << promptText;
    std::string line;
    std::getline(in_, line);
    return Trim(line);
}
