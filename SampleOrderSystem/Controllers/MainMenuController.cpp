#include "MainMenuController.h"

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

constexpr int kDefaultDummyDataCount = 20;
constexpr char kDummyDataFlag[] = "--dummy-data";
constexpr char kDummyDataFlagPrefix[] = "--dummy-data=";

std::string Trim(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

// Parses `text` as a base-10 integer with no trailing garbage. Returns
// false (leaving *outValue untouched) on any parse failure, including
// empty input or partial matches -- never throws.
bool TryParseInt(const std::string& text, int& outValue) {
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t pos = 0;
        const long long value = std::stoll(text, &pos);
        if (pos != text.size()) {
            return false;
        }
        outValue = static_cast<int>(value);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

CliArgs ParseCliArgs(int argc, char* const argv[]) {
    CliArgs args;

    bool sawHelp = false;
    bool sawDummyData = false;
    bool sawDataMonitor = false;
    int dummyCount = kDefaultDummyDataCount;
    std::vector<std::string> unrecognized;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";

        if (arg == "--help" || arg == "-h") {
            sawHelp = true;
        } else if (arg == kDummyDataFlag) {
            sawDummyData = true;
        } else if (arg.rfind(kDummyDataFlagPrefix, 0) == 0) {
            sawDummyData = true;
            const std::string valueText = arg.substr(std::string(kDummyDataFlagPrefix).size());
            int parsed = 0;
            if (TryParseInt(valueText, parsed)) {
                if (parsed < 0) {
                    args.mode = CliMode::Error;
                    args.errorMessage = "Invalid --dummy-data value (must be non-negative): " + valueText + "\n";
                    return args;
                }
                dummyCount = parsed;
            }
            // Non-numeric: fall back to the default count (kDefaultDummyDataCount); do not error.
        } else if (arg == "--data-monitor") {
            sawDataMonitor = true;
        } else {
            unrecognized.push_back(arg);
        }
    }

    if (sawHelp) {
        args.mode = CliMode::Help;
        return args;
    }

    if (!unrecognized.empty()) {
        std::string message = "Unknown argument(s):";
        for (const std::string& u : unrecognized) {
            message += " " + u;
        }
        message += "\n";
        args.mode = CliMode::Error;
        args.errorMessage = message;
        return args;
    }

    if (sawDummyData && sawDataMonitor) {
        args.mode = CliMode::Error;
        args.errorMessage = "Cannot combine --dummy-data and --data-monitor\n";
        return args;
    }

    if (sawDummyData) {
        args.mode = CliMode::DummyData;
        args.dummyDataCount = dummyCount;
        return args;
    }

    if (sawDataMonitor) {
        args.mode = CliMode::DataMonitor;
        return args;
    }

    args.mode = CliMode::Interactive;
    return args;
}

MainMenuController::MainMenuController(SampleController& sampleController, OrderController& orderController,
                                         MonitoringController& monitoringController,
                                         DataMonitorController& dataMonitorController,
                                         DummyDataController& dummyDataController)
    : sampleController_(sampleController),
      orderController_(orderController),
      monitoringController_(monitoringController),
      dataMonitorController_(dataMonitorController),
      dummyDataController_(dummyDataController) {}

void MainMenuController::PrintMenu() const {
    std::cout << "\n=== S-Semi Sample Order System ===\n"
              << "1. Register sample\n"
              << "2. List all samples\n"
              << "3. Search samples\n"
              << "4. Order management (submit / approve / reject / release)\n"
              << "5. Show monitoring summary\n"
              << "6. Show production line\n"
              << "7. Data monitor\n"
              << "8. Generate dummy data\n"
              << "0. Exit\n"
              << "Choose: ";
}

std::optional<int> MainMenuController::ReadMenuChoice() const {
    for (;;) {
        std::string line;
        if (!std::getline(std::cin, line)) {
            return std::nullopt;
        }

        const std::string trimmed = Trim(line);
        int value = 0;
        if (TryParseInt(trimmed, value)) {
            return value;
        }

        std::cout << "Invalid input. Please enter a number.\n";
        PrintMenu();
    }
}

int MainMenuController::PromptInt(const std::string& promptText, int defaultValue) const {
    std::cout << promptText;
    std::string line;
    if (!std::getline(std::cin, line)) {
        return defaultValue;
    }
    int value = 0;
    if (TryParseInt(Trim(line), value) && value >= 0) {
        return value;
    }
    return defaultValue;
}

void MainMenuController::Dispatch(int choice) {
    try {
        switch (choice) {
            case 1:
                sampleController_.HandleRegister();
                break;
            case 2:
                sampleController_.HandleListAll();
                break;
            case 3:
                sampleController_.HandleSearch();
                break;
            case 4:
                orderController_.Run();
                break;
            case 5:
                monitoringController_.ShowMonitoring();
                break;
            case 6:
                monitoringController_.ShowProductionLine();
                break;
            case 7:
                dataMonitorController_.Run();
                break;
            case 8: {
                const int sampleCount = PromptInt("Enter number of samples to generate [10]: ", 10);
                const int orderCount = PromptInt("Enter number of orders to generate [20]: ", 20);
                dummyDataController_.Run(sampleCount, orderCount);
                break;
            }
            default:
                std::cout << "Invalid choice: " << choice << "\n";
                break;
        }
    } catch (const std::exception& ex) {
        std::cout << "Error: " << ex.what() << "\n";
    } catch (...) {
        std::cout << "Error: an unknown error occurred\n";
    }
}

int MainMenuController::Run() {
    for (;;) {
        PrintMenu();
        const std::optional<int> choice = ReadMenuChoice();
        if (!choice.has_value()) {
            return 0;
        }
        if (*choice == 0) {
            return 0;
        }
        Dispatch(*choice);
    }
}
