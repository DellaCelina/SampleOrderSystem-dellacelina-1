#include "SampleView.h"

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

void SampleView::ShowNoSamples() const {
    out_ << "No samples found.\n";
}

void SampleView::ShowSampleList(const std::vector<Sample>& samples) const {
    if (samples.empty()) {
        ShowNoSamples();
        return;
    }

    out_ << "Sample ID | Name | Avg. Production Time (min) | Yield | Current Stock\n";
    for (const Sample& sample : samples) {
        out_ << sample.sampleId << " | "
             << sample.name << " | "
             << sample.averageProductionTimeMinutes << " | "
             << sample.yield << " | "
             << sample.currentStock << "\n";
    }
}

void SampleView::ShowRegistrationSuccess(const Sample& sample) const {
    out_ << "Sample registered successfully: " << sample.sampleId << " (" << sample.name << ")\n";
}

void SampleView::ShowError(const std::string& message) const {
    out_ << "Error: " << message << "\n";
}

std::string SampleView::PromptLine(const std::string& promptText) const {
    out_ << promptText;
    std::string line;
    std::getline(in_, line);
    return Trim(line);
}
