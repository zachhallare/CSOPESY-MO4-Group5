#include "task_manager.h"
#include "apps/file_explorer.h"
#include "core/strings.h"
#include "fs/virtual_fs.h"
#include "os/shell.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <vector>

namespace {

enum ColumnId { COL_NAME, COL_STATUS, COL_CPU, COL_MEM, COL_DISK, COL_NET, COL_PID, COL_USER, COL_DESC, COL_IMAGE };

const ImVec4 ERROR_COLOR(1.0f, 0.45f, 0.45f, 1.0f);
const float  LINK_SPEED_MBPS = 54.0f;   // 802.11g

// Graph colors per resource, as in the Windows Performance tab.
const ImU32 GRAPH_COLORS[4] = {
    IM_COL32(60, 160, 230, 255),   // CPU
    IM_COL32(190, 90, 230, 255),   // Memory
    IM_COL32(110, 200, 60, 255),   // Disk
    IM_COL32(230, 140, 50, 255),   // Network
};

ImU32 WithAlpha(ImU32 color, int alpha) {
    return (color & ~IM_COL32_A_MASK) | ((ImU32)alpha << IM_COL32_A_SHIFT);
}

// Heat-map cell background like the Windows Task Manager (pale yellow to orange).
ImU32 HeatColor(float fraction) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    return IM_COL32(255, (int)(200 - 90 * fraction), 40, (int)(25 + 150 * fraction));
}

void RightAlignedText(const char* text) {
    float width = ImGui::CalcTextSize(text).x;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);
    ImGui::TextUnformatted(text);
}

void HeatCell(float fraction, const char* text) {
    ImGui::TableNextColumn();
    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, HeatColor(fraction));
    RightAlignedText(text);
}

std::string WithCommas(long long value) {
    std::string s = std::to_string(value);
    for (int i = (int)s.size() - 3; i > 0; i -= 3)
        s.insert((size_t)i, ",");
    return s;
}

float CpuSpeedGHz(float utilization) {
    return 0.60f + 0.133f * utilization / 100.0f;   // Pentium III, 733 MHz when busy
}

int CompareProcesses(const Process* a, const Process* b, ImGuiID column) {
    auto cmp = [](float x, float y) { return x < y ? -1 : (x > y ? 1 : 0); };
    switch (column) {
    case COL_CPU:    return cmp(a->cpu, b->cpu);
    case COL_MEM:    return cmp(a->mem, b->mem);
    case COL_DISK:   return cmp(a->disk, b->disk);
    case COL_NET:    return cmp(a->net, b->net);
    case COL_PID:    return a->pid - b->pid;
    case COL_USER:   return ToLower(a->user).compare(ToLower(b->user));
    case COL_DESC:   return ToLower(a->description).compare(ToLower(b->description));
    case COL_IMAGE:  return ToLower(a->image).compare(ToLower(b->image));
    case COL_STATUS: return 0;
    default:         return ToLower(a->name).compare(ToLower(b->name));
    }
}

void SortProcesses(std::vector<const Process*>& list) {
    ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
    if (!specs || specs->SpecsCount == 0)
        return;
    const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
    bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
    std::stable_sort(list.begin(), list.end(), [&](const Process* a, const Process* b) {
        int c = CompareProcesses(a, b, spec.ColumnUserID);
        return ascending ? c < 0 : c > 0;
    });
}

Icon ProcessIcon(const Process& p) {
    if (p.appId >= 0)                return GetAppInfo((AppID)p.appId).icon;
    if (p.group == ProcGroup::System) return Icon::SystemFile;
    return Icon::Program;
}

// Line graph with a grid and a filled area, like the Performance tab.
void DrawGraph(ImDrawList* dl, ImVec2 min, ImVec2 max, const std::vector<float>& values,
               float maxValue, ImU32 color, bool grid)
{
    dl->AddRectFilled(min, max, IM_COL32(18, 20, 26, 255));
    if (grid) {
        ImU32 gridColor = WithAlpha(color, 35);
        for (int i = 1; i < 10; ++i) {
            float y = min.y + (max.y - min.y) * i / 10.0f;
            dl->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), gridColor);
        }
        for (int i = 1; i < 20; ++i) {
            float x = min.x + (max.x - min.x) * i / 20.0f;
            dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), gridColor);
        }
    }

    size_t n = values.size();
    if (n >= 2) {
        // Newest sample on the right edge, one slot per sample.
        float stepX  = (max.x - min.x) / (ProcessManager::HISTORY - 1);
        float startX = max.x - stepX * (float)(n - 1);
        std::vector<ImVec2> points(n);
        for (size_t i = 0; i < n; ++i) {
            float v   = std::clamp(values[i] / maxValue, 0.0f, 1.0f);
            points[i] = ImVec2(startX + stepX * (float)i, max.y - v * (max.y - min.y));
        }

        // Filled area under the line (anti-aliasing off so the slices don't show seams).
        ImDrawListFlags backup = dl->Flags;
        dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        for (size_t i = 0; i + 1 < n; ++i) {
            ImVec2 quad[4] = { points[i], points[i + 1], ImVec2(points[i + 1].x, max.y), ImVec2(points[i].x, max.y) };
            dl->AddConvexPolyFilled(quad, 4, WithAlpha(color, 55));
        }
        dl->Flags = backup;
        dl->AddPolyline(points.data(), (int)n, color, ImDrawFlags_None, 1.5f);
    }
    dl->AddRect(min, max, WithAlpha(color, 160));
}

void StatRow(const char* label, const char* value) {
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(value);
}

} // namespace

// ---------------------------------------------------------------------------

void TaskManager::Render(Shell& shell) {
    RenderMenuBar(shell);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F5, false))
        shell.procs.Refresh();

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Processes")) {
            RenderProcessesTab(shell);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Performance")) {
            RenderPerformanceTab(shell);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Details")) {
            RenderDetailsTab(shell);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // Ending a process is deferred until the tables are done iterating the list.
    if (pendingKill >= 0) {
        std::string error;
        if (shell.EndProcess(pendingKill, &error)) {
            if (selectedPid == pendingKill)
                selectedPid = -1;
        } else {
            shell.Notify("Task Manager", error, ToastKind::Error);
        }
        pendingKill = -1;
    }

    RenderRunDialog(shell);
}

void TaskManager::RenderMenuBar(Shell& shell) {
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Run new task")) {
            openRunDialog = true;
            runBuf[0]     = '\0';
            runError.clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            shell.CloseApp(AppID::TaskManager);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Refresh now", "F5"))
            shell.procs.Refresh();
        if (ImGui::BeginMenu("Update speed")) {
            float& interval = shell.procs.updateInterval;
            if (ImGui::MenuItem("High",   nullptr, interval == 0.5f)) interval = 0.5f;
            if (ImGui::MenuItem("Normal", nullptr, interval == 1.0f)) interval = 1.0f;
            if (ImGui::MenuItem("Low",    nullptr, interval == 4.0f)) interval = 4.0f;
            if (ImGui::MenuItem("Paused", nullptr, interval == 0.0f)) interval = 0.0f;
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    // Scheduler status on the right side of the menu bar.
    const ProcessManager& pm = shell.procs;
    char status[96];
    switch (pm.State()) {
    case SchedulerState::Uninitialized:
        snprintf(status, sizeof(status), "Scheduler: not initialized");
        break;
    case SchedulerState::Idle:
        snprintf(status, sizeof(status), "Scheduler: stopped  (%d generated)", pm.JobsSpawned());
        break;
    case SchedulerState::Running:
        snprintf(status, sizeof(status), "Scheduler: running  (%d active, %d generated)", pm.ActiveJobs(), pm.JobsSpawned());
        break;
    }
    float width = ImGui::CalcTextSize(status).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - width - 16.0f);
    ImGui::TextDisabled("%s", status);

    ImGui::EndMenuBar();
}

void TaskManager::RenderProcessesTab(Shell& shell) {
    const ProcessManager& pm      = shell.procs;
    const float           footerH = ImGui::GetFrameHeightWithSpacing() + 4.0f;

    // Two-line headers showing the totals, like Windows ("23%" over "CPU").
    char cpuHeader[32], memHeader[32], diskHeader[32], netHeader[32];
    snprintf(cpuHeader,  sizeof(cpuHeader),  "%.0f%%\nCPU",     pm.TotalCpu());
    snprintf(memHeader,  sizeof(memHeader),  "%.0f%%\nMemory",  pm.TotalMem() / ProcessManager::TOTAL_RAM_MB * 100.0f);
    snprintf(diskHeader, sizeof(diskHeader), "%.0f%%\nDisk",    std::min(100.0f, pm.TotalDisk() * 12.0f));
    snprintf(netHeader,  sizeof(netHeader),  "%.0f%%\nNetwork", std::min(100.0f, pm.TotalNet() / LINK_SPEED_MBPS * 100.0f));

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuterH |
                            ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("##processes", 6, flags, ImVec2(0, -footerH))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("\nName",    ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.0f, COL_NAME);
        ImGui::TableSetupColumn("\nStatus",  ImGuiTableColumnFlags_WidthFixed, 70.0f, COL_STATUS);
        ImGui::TableSetupColumn(cpuHeader,   ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 72.0f, COL_CPU);
        ImGui::TableSetupColumn(memHeader,   ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 82.0f, COL_MEM);
        ImGui::TableSetupColumn(diskHeader,  ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 84.0f, COL_DISK);
        ImGui::TableSetupColumn(netHeader,   ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 84.0f, COL_NET);
        ImGui::TableHeadersRow();

        static const char*     GROUP_NAMES[3] = { "Apps", "Background processes", "System processes" };
        static const ProcGroup GROUPS[3]      = { ProcGroup::App, ProcGroup::Background, ProcGroup::System };

        for (int g = 0; g < 3; ++g) {
            std::vector<const Process*> list;
            for (const Process& p : pm.Processes())
                if (p.group == GROUPS[g])
                    list.push_back(&p);
            if (list.empty())
                continue;
            SortProcesses(list);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char label[64];
            snprintf(label, sizeof(label), "%s (%d)###group%d", GROUP_NAMES[g], (int)list.size(), g);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.75f, 1.0f, 1.0f));
            bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAllColumns |
                                                 ImGuiTreeNodeFlags_NoTreePushOnOpen);
            ImGui::PopStyleColor();
            if (!open)
                continue;

            for (const Process* p : list) {
                ImGui::PushID(p->pid);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Indent(16.0f);
                if (IconSelectable(p->name.c_str(), ProcessIcon(*p), p->pid == selectedPid,
                                   ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    selectedPid = p->pid;
                    if (p->appId >= 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        shell.OpenApp((AppID)p->appId);   // double-click switches to the app
                }
                ImGui::Unindent(16.0f);
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    selectedPid = p->pid;
                if (ImGui::BeginPopupContextItem("##ctx")) {
                    ProcessContextMenu(shell, *p);
                    ImGui::EndPopup();
                }

                ImGui::TableNextColumn();
                ImGui::TextDisabled("Running");

                char buf[32];
                snprintf(buf, sizeof(buf), "%.1f%%", p->cpu);
                HeatCell(p->cpu / 25.0f, buf);
                snprintf(buf, sizeof(buf), "%.1f MB", p->mem);
                HeatCell(p->mem / 8.0f, buf);
                snprintf(buf, sizeof(buf), "%.1f MB/s", p->disk);
                HeatCell(p->disk / 1.5f, buf);
                snprintf(buf, sizeof(buf), "%.1f Mbps", p->net);
                HeatCell(p->net, buf);
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    RenderFooter(shell);
}

void TaskManager::RenderPerformanceTab(Shell& shell) {
    const ProcessManager& pm = shell.procs;
    const bool  online  = pm.networkOnline;
    const float cpu     = pm.TotalCpu();
    const float memPct  = pm.TotalMem() / ProcessManager::TOTAL_RAM_MB * 100.0f;
    const float diskPct = std::min(100.0f, pm.TotalDisk() * 12.0f);
    const float recvKb  = pm.TotalNet() * 1000.0f * 0.7f;
    const float sendKb  = pm.TotalNet() * 1000.0f * 0.3f;

    float diskMax = 2.0f, netMax = 1.0f;
    for (float v : pm.DiskHistory()) diskMax = std::max(diskMax, v);
    for (float v : pm.NetHistory())  netMax  = std::max(netMax, v);

    struct Resource {
        const char*               name;
        const char*               model;
        const char*               caption;
        const std::vector<float>* history;
        float                     maxValue;
        char                      summary[64];
    };
    Resource res[4] = {
        { "CPU",         "Pentium III @ 733 MHz",     "% Utilization",               &pm.CpuHistory(),  100.0f,  "" },
        { "Memory",      "64,000 KB SDRAM",           "Memory usage",                &pm.MemHistory(),  100.0f,  "" },
        { "Disk 0 (C:)", "WDCAC2420H (420 MB)",       "Transfer rate (MB/s)",        &pm.DiskHistory(), diskMax, "" },
        { "Wi-Fi",       "CSOPESY Wireless 802.11g",  "Throughput (Mbps)",           &pm.NetHistory(),  netMax,  "" },
    };
    snprintf(res[0].summary, sizeof(res[0].summary), "%.0f%%  %.2f GHz", cpu, CpuSpeedGHz(cpu));
    snprintf(res[1].summary, sizeof(res[1].summary), "%.1f/%.1f MB (%.0f%%)", pm.TotalMem(), ProcessManager::TOTAL_RAM_MB, memPct);
    snprintf(res[2].summary, sizeof(res[2].summary), "HDD  %.0f%%", diskPct);
    if (online)
        snprintf(res[3].summary, sizeof(res[3].summary), "S: %.0f  R: %.0f Kbps", sendKb, recvKb);
    else
        snprintf(res[3].summary, sizeof(res[3].summary), "Not connected");

    // Left: one row per resource with a small live graph.
    ImGui::BeginChild("##perflist", ImVec2(230, 0), ImGuiChildFlags_Border);
    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        ImVec2 pos  = ImGui::GetCursorScreenPos();
        float  rowH = 56.0f;
        if (ImGui::Selectable("##resource", perfView == i, 0, ImVec2(0, rowH)))
            perfView = i;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawGraph(dl, pos + ImVec2(4, 6), pos + ImVec2(68, rowH - 6), *res[i].history, res[i].maxValue,
                  GRAPH_COLORS[i], false);
        dl->AddText(pos + ImVec2(78, 8), ImGui::GetColorU32(ImGuiCol_Text), res[i].name);
        dl->AddText(pos + ImVec2(78, 28), ImGui::GetColorU32(ImGuiCol_TextDisabled), res[i].summary);
        ImGui::PopID();
    }
    ImGui::EndChild();

    // Right: the selected resource in detail.
    ImGui::SameLine();
    ImGui::BeginChild("##perfdetail", ImVec2(0, 0));
    const Resource& r = res[perfView];

    ImGui::PushFont(shell.fonts.uiLarge);
    ImGui::TextUnformatted(r.name);
    ImGui::PopFont();
    ImGui::SameLine();
    RightAlignedText(r.model);

    ImGui::TextDisabled("%s", r.caption);
    ImGui::SameLine();
    char maxLabel[32];
    if (perfView < 2) snprintf(maxLabel, sizeof(maxLabel), "100%%");
    else              snprintf(maxLabel, sizeof(maxLabel), "%.1f", r.maxValue);
    RightAlignedText(maxLabel);

    const float statsH = ImGui::GetTextLineHeightWithSpacing() * 6.0f + 8.0f;
    ImVec2 graphMin = ImGui::GetCursorScreenPos();
    ImVec2 avail    = ImGui::GetContentRegionAvail();
    float  graphH   = std::max(80.0f, avail.y - statsH - ImGui::GetTextLineHeightWithSpacing());
    DrawGraph(ImGui::GetWindowDrawList(), graphMin, graphMin + ImVec2(avail.x, graphH), *r.history, r.maxValue,
              GRAPH_COLORS[perfView], true);
    ImGui::Dummy(ImVec2(avail.x, graphH));
    ImGui::TextDisabled("60 seconds");
    ImGui::SameLine();
    RightAlignedText("0");
    ImGui::Spacing();

    if (ImGui::BeginTable("##stats", 4, ImGuiTableFlags_SizingStretchProp)) {
        char a[64], b[64];
        switch (perfView) {
        case 0:
            snprintf(a, sizeof(a), "%.0f%%", cpu);
            snprintf(b, sizeof(b), "%.2f GHz", CpuSpeedGHz(cpu));
            StatRow("Utilization", a);      StatRow("Speed", b);
            snprintf(a, sizeof(a), "%d", (int)pm.Processes().size());
            snprintf(b, sizeof(b), "%d", pm.TotalThreads());
            StatRow("Processes", a);        StatRow("Threads", b);
            snprintf(a, sizeof(a), "%s", WithCommas(pm.TotalThreads() * 31LL).c_str());
            StatRow("Handles", a);          StatRow("Up time", ProcessManager::FormatUptime(pm.Uptime()).c_str());
            StatRow("Base speed", "733 MHz"); StatRow("Sockets / Cores", "1 / 1");
            StatRow("L1 cache", "32 KB");   StatRow("L2 cache", "256 KB");
            break;
        case 1:
            snprintf(a, sizeof(a), "%.1f MB", pm.TotalMem());
            snprintf(b, sizeof(b), "%.1f MB", ProcessManager::TOTAL_RAM_MB - pm.TotalMem());
            StatRow("In use", a);           StatRow("Available", b);
            snprintf(a, sizeof(a), "%.1f/%.1f MB", pm.TotalMem() * 1.3f, ProcessManager::TOTAL_RAM_MB * 1.5f);
            snprintf(b, sizeof(b), "%.1f MB", (ProcessManager::TOTAL_RAM_MB - pm.TotalMem()) * 0.4f);
            StatRow("Committed", a);        StatRow("Cached", b);
            StatRow("Total", "64,000 KB");  StatRow("Speed", "133 MHz");
            StatRow("Slots used", "1 of 2"); StatRow("Form factor", "DIMM");
            break;
        case 2:
            snprintf(a, sizeof(a), "%.0f%%", diskPct);
            snprintf(b, sizeof(b), "%.1f ms", 8.0f + pm.TotalDisk() * 3.0f);
            StatRow("Active time", a);      StatRow("Avg. response time", b);
            snprintf(a, sizeof(a), "%.1f MB/s", pm.TotalDisk() * 0.65f);
            snprintf(b, sizeof(b), "%.1f MB/s", pm.TotalDisk() * 0.35f);
            StatRow("Read speed", a);       StatRow("Write speed", b);
            StatRow("Capacity", "420 MB");  StatRow("Type", "HDD (IDE)");
            StatRow("Model", "WDCAC2420H"); StatRow("System disk", "Yes");
            break;
        default:
            snprintf(a, sizeof(a), "%.0f Kbps", sendKb);
            snprintf(b, sizeof(b), "%.0f Kbps", recvKb);
            StatRow("Status", online ? "Connected" : "Disconnected");
            StatRow("SSID", online ? shell.settings.network.c_str() : "-");
            StatRow("Send", a);             StatRow("Receive", b);
            StatRow("Link speed", "54 Mbps");
            StatRow("IPv4 address", online ? "192.168.1.42" : "-");
            StatRow("Adapter", "802.11g");  StatRow("Signal", online ? "Excellent" : "-");
            break;
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void TaskManager::RenderDetailsTab(Shell& shell) {
    const ProcessManager& pm      = shell.procs;
    const float           footerH = ImGui::GetFrameHeightWithSpacing() + 4.0f;

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuterH |
                            ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("##details", 7, flags, ImVec2(0, -footerH))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name",        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 160.0f, COL_IMAGE);
        ImGui::TableSetupColumn("PID",         ImGuiTableColumnFlags_WidthFixed, 50.0f, COL_PID);
        ImGui::TableSetupColumn("Status",      ImGuiTableColumnFlags_WidthFixed, 60.0f, COL_STATUS);
        ImGui::TableSetupColumn("User name",   ImGuiTableColumnFlags_WidthFixed, 110.0f, COL_USER);
        ImGui::TableSetupColumn("CPU",         ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 50.0f, COL_CPU);
        ImGui::TableSetupColumn("Memory",      ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 80.0f, COL_MEM);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.0f, COL_DESC);
        ImGui::TableHeadersRow();

        std::vector<const Process*> list;
        for (const Process& p : pm.Processes())
            list.push_back(&p);
        SortProcesses(list);

        for (const Process* p : list) {
            ImGui::PushID(p->pid);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (IconSelectable(p->image.c_str(), ProcessIcon(*p), p->pid == selectedPid,
                               ImGuiSelectableFlags_SpanAllColumns))
                selectedPid = p->pid;
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                selectedPid = p->pid;
            if (ImGui::BeginPopupContextItem("##ctx")) {
                ProcessContextMenu(shell, *p);
                ImGui::EndPopup();
            }

            char buf[32];
            ImGui::TableNextColumn();
            ImGui::Text("%d", p->pid);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Running");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(p->user.c_str());
            ImGui::TableNextColumn();
            snprintf(buf, sizeof(buf), "%02d", (int)(p->cpu + 0.5f));
            RightAlignedText(buf);
            ImGui::TableNextColumn();
            RightAlignedText((WithCommas((long long)(p->mem * 1024.0f)) + " K").c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", p->description.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    RenderFooter(shell);
}

void TaskManager::RenderFooter(Shell& shell) {
    const ProcessManager& pm = shell.procs;
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Processes: %d   |   CPU usage: %.0f%%   |   Physical memory: %.0f%%",
                        (int)pm.Processes().size(), pm.TotalCpu(),
                        pm.TotalMem() / ProcessManager::TOTAL_RAM_MB * 100.0f);

    ImGui::SameLine();
    const float buttonW = 110.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > buttonW)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - buttonW);
    ImGui::BeginDisabled(pm.Find(selectedPid) == nullptr);
    if (ImGui::Button("End task", ImVec2(buttonW, 0)))
        pendingKill = selectedPid;
    ImGui::EndDisabled();
}

void TaskManager::ProcessContextMenu(Shell& shell, const Process& p) {
    if (p.appId >= 0 && ImGui::MenuItem("Switch to"))
        shell.OpenApp((AppID)p.appId);
    if (ImGui::MenuItem("End task"))
        pendingKill = p.pid;
    ImGui::Separator();
    if (ImGui::MenuItem("Open file location")) {
        std::string path;
        for (const char* folder : { "/Programs/", "/System/" })
            if (shell.fs.Resolve(folder + p.image))
                path = folder + p.image;
        if (path.empty()) {
            shell.Notify("Task Manager", "'" + p.image + "' has no file on disk (it was generated by the scheduler).");
        } else {
            shell.Explorer().Reveal(path);
            shell.OpenApp(AppID::FileExplorer);
        }
    }
    if (ImGui::MenuItem("Copy name"))
        ImGui::SetClipboardText(p.name.c_str());
}

void TaskManager::RenderRunDialog(Shell& shell) {
    if (openRunDialog) {
        ImGui::OpenPopup("Create new task");
        openRunDialog = false;
    }
    if (!ImGui::BeginPopupModal("Create new task", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted("Type the name of a program, folder or document,");
    ImGui::TextUnformatted("and CSOPESY OS will open it for you.");
    ImGui::Spacing();
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(320.0f);
    bool submit = ImGui::InputTextWithHint("Open", "e.g. notepad, terminal, /Documents", runBuf, sizeof(runBuf),
                                           ImGuiInputTextFlags_EnterReturnsTrue);
    if (!runError.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", runError.c_str());

    submit |= ImGui::Button("OK", ImVec2(110, 0));
    ImGui::SameLine();
    bool cancel = ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape);

    if (submit) {
        if (shell.Launch(runBuf))
            ImGui::CloseCurrentPopup();
        else
            runError = "CSOPESY OS can't find '" + Trim(runBuf) + "'.";
    }
    if (cancel)
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
