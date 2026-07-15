#include <gtest/gtest.h>

#include "Controllers/MainMenuController.h"

#include <vector>

namespace {

// Builds an argv-style char* const[] out of the given string literals and
// runs ParseCliArgs on it. argv[0] is the (unused) program name slot, per
// the usual C main() convention.
CliArgs Parse(std::vector<std::string> flags) {
    std::vector<std::string> owned;
    owned.push_back("SampleOrderSystem.exe");
    for (const std::string& flag : flags) {
        owned.push_back(flag);
    }

    std::vector<char*> argv;
    for (std::string& s : owned) {
        argv.push_back(s.data());
    }

    return ParseCliArgs(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

TEST(ParseCliArgsTest, NoArgsIsInteractive) {
    CliArgs args = Parse({});
    EXPECT_EQ(args.mode, CliMode::Interactive);
}

TEST(ParseCliArgsTest, DummyDataNoValueDefaultsTo20) {
    CliArgs args = Parse({"--dummy-data"});
    EXPECT_EQ(args.mode, CliMode::DummyData);
    EXPECT_EQ(args.dummyDataCount, 20);
}

TEST(ParseCliArgsTest, DummyDataWithExplicitValue) {
    CliArgs args = Parse({"--dummy-data=50"});
    EXPECT_EQ(args.mode, CliMode::DummyData);
    EXPECT_EQ(args.dummyDataCount, 50);
}

TEST(ParseCliArgsTest, DummyDataWithNonNumericValueFallsBackToDefault) {
    CliArgs args = Parse({"--dummy-data=abc"});
    EXPECT_EQ(args.mode, CliMode::DummyData);
    EXPECT_EQ(args.dummyDataCount, 20);
}

TEST(ParseCliArgsTest, DummyDataWithZeroIsValid) {
    CliArgs args = Parse({"--dummy-data=0"});
    EXPECT_EQ(args.mode, CliMode::DummyData);
    EXPECT_EQ(args.dummyDataCount, 0);
}

TEST(ParseCliArgsTest, DummyDataWithNegativeValueIsError) {
    CliArgs args = Parse({"--dummy-data=-5"});
    EXPECT_EQ(args.mode, CliMode::Error);
    EXPECT_FALSE(args.errorMessage.empty());
}

TEST(ParseCliArgsTest, DataMonitorFlag) {
    CliArgs args = Parse({"--data-monitor"});
    EXPECT_EQ(args.mode, CliMode::DataMonitor);
}

TEST(ParseCliArgsTest, HelpLongFlag) {
    CliArgs args = Parse({"--help"});
    EXPECT_EQ(args.mode, CliMode::Help);
}

TEST(ParseCliArgsTest, HelpShortFlag) {
    CliArgs args = Parse({"-h"});
    EXPECT_EQ(args.mode, CliMode::Help);
}

TEST(ParseCliArgsTest, DummyDataAndDataMonitorTogetherIsError) {
    CliArgs args = Parse({"--dummy-data", "--data-monitor"});
    EXPECT_EQ(args.mode, CliMode::Error);
    EXPECT_FALSE(args.errorMessage.empty());
}

TEST(ParseCliArgsTest, UnrecognizedFlagIsError) {
    CliArgs args = Parse({"--bogus"});
    EXPECT_EQ(args.mode, CliMode::Error);
    EXPECT_FALSE(args.errorMessage.empty());
}

TEST(ParseCliArgsTest, FlagPlusPositionalGarbageIsError) {
    CliArgs args = Parse({"--dummy-data", "somepositionalarg"});
    EXPECT_EQ(args.mode, CliMode::Error);
    EXPECT_FALSE(args.errorMessage.empty());
}
