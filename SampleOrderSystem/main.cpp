// Wires the whole console app together: parses CLI flags, resolves data/
// schema paths relative to the executable's own directory (not the
// process's cwd), constructs repositories -> services -> views ->
// controllers, then either runs one CLI-flag mode directly or enters
// MainMenuController's interactive loop. See
// docs/impl/s-semi/phase-09-main-wiring/DETAIL.md for the full spec this
// file implements.

#include <windows.h>

#include <filesystem>
#include <iostream>
#include <random>

#include "Controllers/DataMonitorController.h"
#include "Controllers/DummyDataController.h"
#include "Controllers/MainMenuController.h"
#include "Controllers/MonitoringController.h"
#include "Controllers/OrderController.h"
#include "Controllers/SampleController.h"
#include "Core/Console.h"
#include "Core/SystemClock.h"
#include "Repositories/OrderRepository.h"
#include "Repositories/ProductionQueueRepository.h"
#include "Repositories/SampleRepository.h"
#include "Services/DummyDataGenerator.h"
#include "Services/MonitoringService.h"
#include "Services/OrderService.h"
#include "Services/ProductionLineViewService.h"
#include "Services/ProductionService.h"
#include "Views/DataMonitorView.h"
#include "Views/MonitoringView.h"
#include "Views/OrderView.h"
#include "Views/ProductionLineView.h"
#include "Views/SampleView.h"

namespace {

// Windows-only; project is a Win32/x64 VS console app per CLAUDE.md, so no
// cross-platform fallback is needed.
std::filesystem::path ExecutableDirectory() {
    wchar_t buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        throw std::runtime_error("실행 파일 경로를 확인하지 못했습니다");
    }
    return std::filesystem::path(buffer).parent_path();
}

void PrintUsage() {
    std::cout << "사용법: SampleOrderSystem.exe [--dummy-data[=N]] [--data-monitor] [--help]\n"
              << "  --dummy-data[=N]   더미 시료/주문 데이터를 N개씩 생성하고 종료합니다 (기본값 20)\n"
              << "  --data-monitor     생산 대기열을 정산한 뒤 전체 데이터를 출력하고 종료합니다\n"
              << "  --help, -h         이 도움말을 출력하고 종료합니다\n";
}

}  // namespace

int main(int argc, char** argv) {
    EnableConsoleAnsiAndUtf8();

    CliArgs args = ParseCliArgs(argc, argv);
    if (args.mode == CliMode::Help) {
        PrintUsage();
        return 0;
    }
    if (args.mode == CliMode::Error) {
        std::cerr << args.errorMessage;
        return 1;
    }

    try {
        const std::filesystem::path exeDir = ExecutableDirectory();
        const std::filesystem::path dataDir = exeDir / "data";
        const std::filesystem::path schemaDir = exeDir / "schema";
        // An empty/missing data/ directory is a legitimate first-run state;
        // this only ensures the directory exists, it never fabricates
        // placeholder JSON files (that's the repositories' job).
        std::filesystem::create_directories(dataDir);

        SystemClock clock;

        SampleRepository sampleRepo(dataDir / "samples.json", schemaDir / "sample.schema.json");
        OrderRepository orderRepo(dataDir / "orders.json", schemaDir / "order.schema.json");
        ProductionQueueRepository queueRepo(dataDir / "production_queue.json",
                                             schemaDir / "production_queue.schema.json");

        ProductionService productionService(sampleRepo, orderRepo, queueRepo);
        OrderService orderService(orderRepo, sampleRepo, productionService);
        MonitoringService monitoringService(orderRepo, sampleRepo, queueRepo, clock);
        ProductionLineViewService lineViewService(queueRepo, orderRepo, sampleRepo, clock);

        // seed is mandatory (no default) on DummyDataGenerator so tests can be
        // deterministic; the real app just needs "different every run".
        std::random_device rd;
        DummyDataGenerator dummyGen(sampleRepo, orderRepo, queueRepo, clock, rd());

        SampleView sampleView;
        OrderView orderView;
        MonitoringView monitoringView;
        ProductionLineView lineView;
        DataMonitorView dataMonitorView;

        SampleController sampleController(sampleRepo, sampleView);
        OrderController orderController(orderService, orderView, std::cin, clock);
        MonitoringController monitoringController(
            [&monitoringService]() { return monitoringService.GetOrderStatusCounts(); },
            [&monitoringService]() { return monitoringService.GetSampleStockLevels(); },
            [&lineViewService]() { return lineViewService.GetSnapshot(); }, monitoringView, lineView);
        DataMonitorController dataMonitorController(clock, productionService, sampleRepo, orderRepo, queueRepo,
                                                      dataMonitorView);
        DummyDataController dummyDataController(dummyGen);

        switch (args.mode) {
            case CliMode::DummyData:
                // The CLI's single --dummy-data=<N> knob maps to both the
                // sample count and order count DummyDataController::Run
                // actually takes, so --dummy-data=0 truly generates nothing
                // (per DETAIL.md's edge cases) and the default N=20 covers
                // both in one flag.
                dummyDataController.Run(args.dummyDataCount, args.dummyDataCount);
                return 0;
            case CliMode::DataMonitor:
                dataMonitorController.Run();
                return 0;
            default: {
                MainMenuController menu(sampleController, orderController, monitoringController,
                                         dataMonitorController, dummyDataController);
                return menu.Run();
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "치명적 오류: " << ex.what() << "\n";
        return 1;
    }
}
