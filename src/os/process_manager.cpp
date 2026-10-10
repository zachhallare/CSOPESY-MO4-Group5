#include "process_manager.h"

#include <algorithm>
#include <cstdio>

ProcessManager::ProcessManager(const std::string& user, float spawnInterval)
    : rng(std::random_device{}()), user(user), spawnInterval(spawnInterval)
{
    //   name                      image              owner              description                                 group                 critical cpu   mem  thr  net
    Add("System",                 "kernel.sys",      "SYSTEM",          "CSOPESY Kernel",                           ProcGroup::System,     true,  0.6f, 2.4f, 96, false);
    Add("Session Manager",        "smss.exe",        "SYSTEM",          "Session Manager Subsystem",                ProcGroup::System,     true,  0.0f, 0.3f,  2, false);
    Add("Client Server Runtime",  "csrss.exe",       "SYSTEM",          "Client Server Runtime Process",            ProcGroup::System,     true,  0.2f, 0.7f, 11, false);
    Add("Logon Process",          "logon.exe",       "SYSTEM",          "Logon Application",                        ProcGroup::System,     true,  0.0f, 0.5f,  3, false);
    Add("CSOPESY Compositor",     "compositor.exe",  "SYSTEM",          "Desktop compositor (GLFW + OpenGL + ImGui)", ProcGroup::System,   true,  2.4f, 5.6f, 14, false);
    Add("Shell Experience Host",  "shell.exe",       user,              "Desktop, taskbar and start menu",          ProcGroup::System,     true,  0.8f, 2.8f, 22, false);
    Add("Service Host: Network",  "svchost.exe",     "NETWORK SERVICE", "Host Process for Network Services",        ProcGroup::Background, false, 0.3f, 1.2f,  8, true);
    Add("Service Host: Audio",    "audiosrv.exe",    "LOCAL SERVICE",   "Audio Service",                            ProcGroup::Background, false, 0.2f, 0.9f,  6, false);
    Add("Print Spooler",          "spoolsv.exe",     "SYSTEM",          "Spooler SubSystem App",                    ProcGroup::Background, false, 0.0f, 0.6f,  5, false);
    Add("Time Service",           "timesvc.exe",     "LOCAL SERVICE",   "Keeps the clock in sync",                  ProcGroup::Background, false, 0.1f, 0.3f,  2, true);
    Add("Antimalware Service",    "antimalware.exe", "SYSTEM",          "Antimalware Service Executable",           ProcGroup::Background, false, 1.2f, 3.4f, 30, true);
    Add("Search Indexer",         "searchidx.exe",   "SYSTEM",          "File search indexer",                      ProcGroup::Background, false, 0.4f, 1.1f,  9, false);

    // Pre-fill the graphs so they look like the system has been running a while.
    for (int i = 0; i < HISTORY; ++i)
        Tick();
}

float ProcessManager::Rand(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}

Process& ProcessManager::Add(const std::string& name, const std::string& image, const std::string& owner,
                             const std::string& description, ProcGroup group, bool critical,
                             float baseCpu, float baseMem, int threads, bool usesNet)
{
    Process p;
    p.pid         = nextPid;
    p.name        = name;
    p.image       = image;
    p.user        = owner;
    p.description = description;
    p.group       = group;
    p.critical    = critical;
    p.baseCpu     = baseCpu;
    p.baseMem     = baseMem;
    p.cpu         = baseCpu;
    p.mem         = baseMem;
    p.threads     = threads;
    p.usesNet     = usesNet;
    nextPid += 4 * (1 + (int)Rand(0.0f, 60.0f));   // Windows-style PIDs: multiples of 4
    procs.push_back(p);
    return procs.back();
}

void ProcessManager::Update(float deltaTime) {
    uptime += deltaTime;

    // Scheduler jobs run for a while and then finish on their own.
    for (Process& p : procs)
        if (p.timeLeft > 0.0f)
            p.timeLeft = std::max(0.0f, p.timeLeft - deltaTime);
    size_t before = procs.size();
    procs.erase(std::remove_if(procs.begin(), procs.end(),
                    [](const Process& p) { return p.timeLeft == 0.0f; }),
                procs.end());
    jobsFinished += (int)(before - procs.size());

    if (schedState == SchedulerState::Running) {
        spawnTimer += deltaTime;
        while (spawnTimer >= spawnInterval) {
            spawnTimer -= spawnInterval;
            SpawnJob();
        }
    }

    if (updateInterval > 0.0f) {
        tickTimer += deltaTime;
        if (tickTimer >= updateInterval) {
            tickTimer = 0.0f;
            Tick();
        }
    }
}

void ProcessManager::Refresh() {
    tickTimer = 0.0f;
    Tick();
}

void ProcessManager::Tick() {
    for (Process& p : procs) {
        float spike = (Rand(0.0f, 1.0f) < 0.05f) ? Rand(1.0f, 6.0f) : 0.0f;
        p.cpu  = std::max(0.0f, p.baseCpu * Rand(0.4f, 1.6f) + (p.baseCpu > 0.0f ? spike : 0.0f));
        p.mem  = p.baseMem * Rand(0.97f, 1.03f);
        p.disk = (Rand(0.0f, 1.0f) < (p.timeLeft > 0.0f ? 0.30f : 0.08f)) ? Rand(0.1f, 1.2f) : 0.0f;
        p.net  = (networkOnline && p.usesNet && Rand(0.0f, 1.0f) < 0.35f) ? Rand(0.05f, 0.9f) : 0.0f;
    }

    // One CPU: keep the total at or below 100%.
    float total = TotalCpu();
    if (total > 98.0f)
        for (Process& p : procs)
            p.cpu *= 98.0f / total;

    auto push = [](std::vector<float>& history, float value) {
        history.push_back(value);
        if ((int)history.size() > HISTORY)
            history.erase(history.begin());
    };
    push(cpuHistory,  TotalCpu());
    push(memHistory,  TotalMem() / TOTAL_RAM_MB * 100.0f);
    push(diskHistory, TotalDisk());
    push(netHistory,  TotalNet());
}

void ProcessManager::SpawnJob() {
    float mem = Rand(0.4f, 1.6f);
    if (TotalMem() + mem > TOTAL_RAM_MB * 0.92f)
        return;   // out of memory: skip this cycle, like a real scheduler would wait

    ++jobsSpawned;
    char name[32];
    snprintf(name, sizeof(name), "process_%02d", jobsSpawned);
    Process& p = Add(name, std::string(name) + ".exe", user, "Job generated by the scheduler",
                     ProcGroup::Background, false, Rand(4.0f, 14.0f), mem, 1, false);
    p.timeLeft = Rand(6.0f, 18.0f);
}

void ProcessManager::AddAppProcess(int appId, const std::string& name, const std::string& image,
                                   float baseCpu, float baseMem)
{
    for (const Process& p : procs)
        if (p.appId == appId) return;
    Process& p = Add(name, image, user, name, ProcGroup::App, false, baseCpu, baseMem,
                     4 + (int)Rand(0.0f, 12.0f), appId == 0);
    p.appId = appId;
}

void ProcessManager::RemoveAppProcess(int appId) {
    procs.erase(std::remove_if(procs.begin(), procs.end(),
                    [appId](const Process& p) { return p.appId == appId; }),
                procs.end());
}

bool ProcessManager::Kill(int pid, int* ownedAppId, std::string* error) {
    auto it = std::find_if(procs.begin(), procs.end(), [pid](const Process& p) { return p.pid == pid; });
    if (it == procs.end()) {
        if (error) *error = "No process with PID " + std::to_string(pid) + ".";
        return false;
    }
    if (it->critical) {
        if (error) *error = "Access is denied. '" + it->name + "' is a critical system process.";
        return false;
    }
    if (ownedAppId) *ownedAppId = it->appId;
    if (it->timeLeft > 0.0f) ++jobsFinished;
    procs.erase(it);
    return true;
}

bool ProcessManager::Initialize() {
    if (schedState != SchedulerState::Uninitialized)
        return false;
    schedState = SchedulerState::Idle;
    return true;
}

bool ProcessManager::StartScheduler() {
    if (schedState != SchedulerState::Idle)
        return false;
    schedState = SchedulerState::Running;
    spawnTimer = spawnInterval;   // spawn the first job right away
    return true;
}

bool ProcessManager::StopScheduler() {
    if (schedState != SchedulerState::Running)
        return false;
    schedState = SchedulerState::Idle;
    return true;
}

int ProcessManager::ActiveJobs() const {
    return (int)std::count_if(procs.begin(), procs.end(), [](const Process& p) { return p.timeLeft > 0.0f; });
}

const Process* ProcessManager::Find(int pid) const {
    for (const Process& p : procs)
        if (p.pid == pid) return &p;
    return nullptr;
}

float ProcessManager::TotalCpu() const {
    float total = 0.0f;
    for (const Process& p : procs) total += p.cpu;
    return std::min(total, 100.0f);
}

float ProcessManager::TotalMem() const {
    float total = 0.0f;
    for (const Process& p : procs) total += p.mem;
    return total;
}

float ProcessManager::TotalDisk() const {
    float total = 0.0f;
    for (const Process& p : procs) total += p.disk;
    return total;
}

float ProcessManager::TotalNet() const {
    float total = 0.0f;
    for (const Process& p : procs) total += p.net;
    return total;
}

int ProcessManager::TotalThreads() const {
    int total = 0;
    for (const Process& p : procs) total += p.threads;
    return total;
}

std::string ProcessManager::FormatUptime(float seconds) {
    int  total = (int)seconds;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d:%02d:%02d:%02d",
             total / 86400, (total / 3600) % 24, (total / 60) % 60, total % 60);
    return buf;
}
