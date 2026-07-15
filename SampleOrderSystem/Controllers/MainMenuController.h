#pragma once
#include <optional>
#include <string>

#include "DataMonitorController.h"
#include "DummyDataController.h"
#include "MonitoringController.h"
#include "OrderController.h"
#include "SampleController.h"

// ---- CLI flag parsing ------------------------------------------------
// Free, pure function: no dependency on any controller/service/repository,
// so it is unit-testable from SampleOrderSystemTests even though the rest
// of this header (MainMenuController itself) is not compiled there.

enum class CliMode { Interactive, DummyData, DataMonitor, Help, Error };

struct CliArgs {
    CliMode mode = CliMode::Interactive;
    int dummyDataCount = 20;  // only meaningful when mode == DummyData
    std::string errorMessage;  // only meaningful when mode == Error
};

CliArgs ParseCliArgs(int argc, char* const argv[]);

// ---- Interactive menu loop --------------------------------------------

class MainMenuController {
public:
    MainMenuController(SampleController& sampleController,
                        OrderController& orderController,
                        MonitoringController& monitoringController,
                        DataMonitorController& dataMonitorController,
                        DummyDataController& dummyDataController);

    // Runs the interactive loop until the user exits or input is exhausted (EOF).
    // Returns a process exit code (0 on normal exit).
    int Run();

private:
    void PrintMenu() const;
    // Returns std::nullopt on EOF/unreadable input (caller treats this as "exit").
    // Reprompts internally (printing an error) on non-numeric input, so a
    // returned value is always either a genuine EOF (nullopt) or a valid int.
    std::optional<int> ReadMenuChoice() const;
    void Dispatch(int choice);  // no-op + "invalid choice" message for unmapped ints
    int PromptInt(const std::string& promptText, int defaultValue) const;

    SampleController& sampleController_;
    OrderController& orderController_;
    MonitoringController& monitoringController_;
    DataMonitorController& dataMonitorController_;
    DummyDataController& dummyDataController_;
};
