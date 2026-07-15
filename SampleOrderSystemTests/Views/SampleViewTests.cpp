#include <gtest/gtest.h>

#include "Views/SampleView.h"
#include "Models/Sample.h"

#include <sstream>
#include <string>
#include <vector>

namespace {

// Global namespace per phase-10's DETAIL.md correction note (matches phase-1/2/4/5's
// real committed code: no Views::/Models::/Repositories:: wrappers anywhere).

Sample MakeSample(const std::string& id, const std::string& name, int avgTime, double yield, int stock) {
    Sample s;
    s.sampleId = id;
    s.name = name;
    s.averageProductionTimeMinutes = avgTime;
    s.yield = yield;
    s.currentStock = stock;
    return s;
}

}  // namespace

TEST(SampleViewTest, ShowSampleListWithEmptyVectorPrintsNoSamplesMessageOnly) {
    std::istringstream in;
    std::ostringstream out;
    SampleView view(in, out);

    view.ShowSampleList({});

    std::ostringstream expected;
    SampleView expectedView(in, expected);
    expectedView.ShowNoSamples();

    EXPECT_EQ(out.str(), expected.str());
    EXPECT_FALSE(out.str().empty());
}

TEST(SampleViewTest, ShowSampleListWithOneSamplePrintsItsFields) {
    std::istringstream in;
    std::ostringstream out;
    SampleView view(in, out);

    Sample sample = MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100);
    view.ShowSampleList({ sample });

    const std::string text = out.str();
    EXPECT_NE(text.find("SMP-001"), std::string::npos);
    EXPECT_NE(text.find("GaAs Wafer"), std::string::npos);
    EXPECT_NE(text.find("30"), std::string::npos);
    EXPECT_NE(text.find("0.9"), std::string::npos);
    EXPECT_NE(text.find("100"), std::string::npos);
}

TEST(SampleViewTest, ShowSampleListWithMultipleSamplesPrintsAllInGivenOrder) {
    std::istringstream in;
    std::ostringstream out;
    SampleView view(in, out);

    Sample first = MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100);
    Sample second = MakeSample("SMP-002", "Silicon Ingot", 45, 0.8, 50);
    view.ShowSampleList({ first, second });

    const std::string text = out.str();
    const auto firstPos = text.find("SMP-001");
    const auto secondPos = text.find("SMP-002");
    ASSERT_NE(firstPos, std::string::npos);
    ASSERT_NE(secondPos, std::string::npos);
    EXPECT_LT(firstPos, secondPos);
    EXPECT_NE(text.find("GaAs Wafer"), std::string::npos);
    EXPECT_NE(text.find("Silicon Ingot"), std::string::npos);
}

TEST(SampleViewTest, ShowRegistrationSuccessPrintsIdAndName) {
    std::istringstream in;
    std::ostringstream out;
    SampleView view(in, out);

    Sample sample = MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 0);
    view.ShowRegistrationSuccess(sample);

    const std::string text = out.str();
    EXPECT_NE(text.find("SMP-001"), std::string::npos);
    EXPECT_NE(text.find("GaAs Wafer"), std::string::npos);
}

TEST(SampleViewTest, ShowErrorPrintsTheExactMessagePassedIn) {
    std::istringstream in;
    std::ostringstream out;
    SampleView view(in, out);

    view.ShowError("Sample ID must not be empty");

    EXPECT_NE(out.str().find("Sample ID must not be empty"), std::string::npos);
}

TEST(SampleViewTest, PromptLineWritesPromptAndReturnsTrimmedLine) {
    std::istringstream in("  ABC  \n");
    std::ostringstream out;
    SampleView view(in, out);

    const std::string result = view.PromptLine("Enter sample ID: ");

    EXPECT_EQ(result, "ABC");
    EXPECT_NE(out.str().find("Enter sample ID: "), std::string::npos);
}

TEST(SampleViewTest, PromptLineCalledTwiceInSequenceReturnsLinesInOrder) {
    std::istringstream in("first line\nsecond line\n");
    std::ostringstream out;
    SampleView view(in, out);

    const std::string firstResult = view.PromptLine("First: ");
    const std::string secondResult = view.PromptLine("Second: ");

    EXPECT_EQ(firstResult, "first line");
    EXPECT_EQ(secondResult, "second line");
}

TEST(SampleViewTest, PromptLineOnAnEmptyLineReturnsEmptyStringWithoutThrowing) {
    std::istringstream in("\n");
    std::ostringstream out;
    SampleView view(in, out);

    std::string result;
    EXPECT_NO_THROW(result = view.PromptLine("Enter: "));
    EXPECT_TRUE(result.empty());
}
