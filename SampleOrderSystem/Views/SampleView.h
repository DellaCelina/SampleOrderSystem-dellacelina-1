#pragma once
#include <iostream>
#include <string>
#include <vector>

#include "../Models/Sample.h"

class SampleView {
public:
    explicit SampleView(std::istream& in = std::cin, std::ostream& out = std::cout);

    // Output
    void ShowSampleList(const std::vector<Sample>& samples) const;
    void ShowNoSamples() const;
    void ShowRegistrationSuccess(const Sample& sample) const;
    void ShowError(const std::string& message) const;

    // Input
    std::string PromptLine(const std::string& promptText) const;

private:
    std::istream& in_;
    std::ostream& out_;
};
