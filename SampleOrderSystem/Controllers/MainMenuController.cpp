#include "MainMenuController.h"

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../Core/Console.h"

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
                    args.errorMessage = "오류: --dummy-data 값이 올바르지 않습니다 (0 이상이어야 합니다): " + valueText + "\n";
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
        std::string message = "오류: 알 수 없는 인자입니다:";
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
        args.errorMessage = "오류: --dummy-data와 --data-monitor는 함께 사용할 수 없습니다\n";
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
    std::cout << HeaderBlock()
              << "[1] 시료 등록\n"
              << "[2] 시료 목록 조회\n"
              << "[3] 시료 검색\n"
              << "[4] 주문 관리\n"
              << "[5] 모니터링 요약\n"
              << "[6] 생산 라인 조회\n"
              << "[7] 데이터 모니터\n"
              << "[8] 더미 데이터 생성\n"
              << "[0] 종료\n"
              << "선택 > ";
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

        std::cout << "오류: 숫자를 입력해 주세요.\n";
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

namespace {
// Read-only/result screens (list, search, monitoring, production line, data
// monitor, register/dummy-data results) render their content and then return
// immediately; without a pause here, the *next* loop iteration's PrintMenu()
// clears the screen before the user has had a chance to read what was just
// shown. Menu item 4 (order submenu) is exempt: it owns its own input loop
// and already blocks on a menu choice before anything gets cleared again.
bool ChoiceNeedsPauseAfterDispatch(int choice) {
    return choice != 4;
}
}  // namespace

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
                const int sampleCount = PromptInt("생성할 시료 개수 [10] > ", 10);
                const int orderCount = PromptInt("생성할 주문 개수 [20] > ", 20);
                dummyDataController_.Run(sampleCount, orderCount);
                break;
            }
            default:
                std::cout << "오류: 올바르지 않은 선택입니다: " << choice << "\n";
                break;
        }
    } catch (const std::exception& ex) {
        std::cout << "오류: " << ex.what() << "\n";
    } catch (...) {
        std::cout << "오류: 알 수 없는 오류가 발생했습니다\n";
    }

    if (ChoiceNeedsPauseAfterDispatch(choice)) {
        PressEnterToContinue();
    }
}

void MainMenuController::PressEnterToContinue() const {
    std::cout << "\n계속하려면 Enter 키를 누르세요...";
    std::string discarded;
    std::getline(std::cin, discarded);
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
