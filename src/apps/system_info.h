// System Information: a read-only summary of the (made-up) hardware and
// software, laid out like msinfo32 with categories on the left.
#pragma once

#include "apps/app.h"

class SystemInfo : public App {
public:
    SystemInfo() : App(AppID::SystemInfo) {}

    ImVec2 DefaultSize() const override { return ImVec2(700, 440); }
    ImVec2 MinSize() const override     { return ImVec2(520, 320); }
    void   Render(Shell& shell) override;

private:
    int category = 0;
};
