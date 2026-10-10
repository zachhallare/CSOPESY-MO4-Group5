#include "quick_settings.h"
#include "os/shell.h"

#include <cfloat>
#include <string>

namespace {

struct WifiNetwork {
    const char* ssid;
    int         bars;
    bool        secured;
};

const WifiNetwork NETWORKS[] = {
    { "CSOPESY-Lab",       4, true  },
    { "DLSU-WiFi",         3, true  },
    { "GAMELab-5G",        3, true  },
    { "Coffee Shop Guest", 2, false },
    { "PLDTHomeFiber_2F",  1, true  },
};

void SignalBars(int bars) {
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       h   = ImGui::GetTextLineHeight();
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    for (int i = 0; i < 4; ++i) {
        float  barH = h * (0.3f + 0.7f * (float)(i + 1) / 4.0f);
        ImVec2 min(pos.x + i * 5.0f, pos.y + h - barH);
        dl->AddRectFilled(min, ImVec2(min.x + 3.0f, pos.y + h),
                          i < bars ? IM_COL32(220, 225, 235, 255) : IM_COL32(90, 95, 105, 255));
    }
    ImGui::Dummy(ImVec2(20.0f, h));
}

} // namespace

void VolumeControls(Shell& shell) {
    ShellSettings& s = shell.settings;
    ImGui::BeginDisabled(s.muted);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderInt("##volume", &s.volume, 0, 100, "Volume: %d%%");
    ImGui::EndDisabled();
    ImGui::Checkbox("Mute", &s.muted);
}

void NetworkControls(Shell& shell) {
    ShellSettings& s = shell.settings;
    if (ImGui::Checkbox("Wi-Fi", &s.wifiEnabled) && s.wifiEnabled)
        s.airplaneMode = false;
    ImGui::SameLine(0, 24);
    if (ImGui::Checkbox("Airplane mode", &s.airplaneMode) && s.airplaneMode)
        s.wifiEnabled = false;
    ImGui::Separator();

    if (!s.wifiEnabled || s.airplaneMode) {
        ImGui::TextDisabled(s.airplaneMode ? "Airplane mode is on." : "Wi-Fi is turned off.");
        return;
    }
    if (s.network.empty())
        ImGui::TextDisabled("Not connected");
    else
        ImGui::Text("Connected to %s", s.network.c_str());
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 0));
    for (const WifiNetwork& net : NETWORKS) {
        ImGui::PushID(net.ssid);
        bool connected = (s.network == net.ssid);
        SignalBars(net.bars);
        ImGui::SameLine();
        ImGui::Text("%s%s", net.ssid, net.secured ? "" : "  (open)");

        ImGui::SameLine();
        const float buttonW = 90.0f;
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail > buttonW)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - buttonW);
        if (ImGui::Button(connected ? "Disconnect" : "Connect", ImVec2(buttonW, 0))) {
            if (connected) {
                s.network.clear();
                shell.Notify("Network", std::string("Disconnected from ") + net.ssid + ".");
            } else {
                s.network = net.ssid;
                shell.Notify("Network", std::string("Connected to ") + net.ssid + ".", ToastKind::Success);
            }
        }
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
}
