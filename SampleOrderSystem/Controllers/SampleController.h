#pragma once
#include "../Repositories/SampleRepository.h"
#include "../Views/SampleView.h"

class SampleController {
public:
    SampleController(SampleRepository& repository, SampleView& view);

    void HandleRegister();
    void HandleListAll();
    void HandleSearch();

private:
    SampleRepository& repository_;
    SampleView& view_;
};
