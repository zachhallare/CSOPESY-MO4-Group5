// Dummy process table behind the Task Manager. Nothing here actually runs:
// every entry has made-up CPU / memory / disk / network numbers that drift a
// little on every tick so the Task Manager feels alive.
//
// It also hosts the mock scheduler driven by the taskbar's INIT / START /
// STOP buttons (a nod to the CSOPESY process scheduler): once started it
// keeps spawning short-lived "process_XX" jobs until it is stopped.
#pragma once

#include <random>
#include <string>
#include <vector>

enum class ProcGroup { App, Background, System };

struct Process {
    int         pid = 0;
    std::string name;          // friendly name (Processes tab)
    std::string image;         // executable name (Details tab)
    std::string user;
    std::string description;
    ProcGroup   group    = ProcGroup::Background;
    bool        critical = false;   // ending it is refused
    bool        usesNet  = false;
    int         appId    = -1;      // AppID of the window it owns, -1 if none
    int         threads  = 1;

    float cpu  = 0.0f;   // percent
    float mem  = 0.0f;   // MB
    float disk = 0.0f;   // MB/s
    float net  = 0.0f;   // Mbps

    float baseCpu  = 0.0f;
    float baseMem  = 0.0f;
    float timeLeft = -1.0f;   // scheduler jobs finish when this reaches zero
};

enum class SchedulerState { Uninitialized, Idle, Running };

class ProcessManager {
public:
    static constexpr float TOTAL_RAM_MB = 62.5f;   // 64000K, as reported by the BIOS
    static constexpr int   HISTORY      = 60;      // samples kept for the graphs

    ProcessManager(const std::string& user, float spawnInterval);

    void Update(float deltaTime);
    void Refresh();   // re-roll the numbers right away

    // App windows own a process while they are open.
    void AddAppProcess(int appId, const std::string& name, const std::string& image,
                       float baseCpu, float baseMem);
    void RemoveAppProcess(int appId);

    // Ends a process. Critical ones are refused with a reason in *error.
    // *ownedAppId receives the window that should close, or -1.
    bool Kill(int pid, int* ownedAppId, std::string* error);

    // Mock scheduler (taskbar INIT / START / STOP).
    bool           Initialize();
    bool           StartScheduler();
    bool           StopScheduler();
    SchedulerState State() const         { return schedState; }
    int            JobsSpawned() const   { return jobsSpawned; }
    int            JobsFinished() const  { return jobsFinished; }
    int            ActiveJobs() const;
    float          SpawnInterval() const { return spawnInterval; }

    const std::vector<Process>& Processes() const { return procs; }
    const Process* Find(int pid) const;

    float TotalCpu() const;
    float TotalMem() const;
    float TotalDisk() const;
    float TotalNet() const;
    int   TotalThreads() const;
    float Uptime() const { return uptime; }
    static std::string FormatUptime(float seconds);   // "d:hh:mm:ss"

    const std::vector<float>& CpuHistory()  const { return cpuHistory; }
    const std::vector<float>& MemHistory()  const { return memHistory; }    // percent of RAM
    const std::vector<float>& DiskHistory() const { return diskHistory; }   // MB/s
    const std::vector<float>& NetHistory()  const { return netHistory; }    // Mbps

    // Seconds between refreshes of the numbers; 0 pauses them
    // (Task Manager > View > Update speed).
    float updateInterval = 1.0f;
    bool  networkOnline  = true;

private:
    std::vector<Process> procs;
    std::mt19937         rng;
    std::string          user;
    int   nextPid       = 4;
    float tickTimer     = 0.0f;
    float spawnTimer    = 0.0f;
    float spawnInterval = 2.0f;
    float uptime        = 0.0f;
    SchedulerState schedState = SchedulerState::Uninitialized;
    int   jobsSpawned  = 0;
    int   jobsFinished = 0;

    std::vector<float> cpuHistory, memHistory, diskHistory, netHistory;

    float    Rand(float lo, float hi);
    Process& Add(const std::string& name, const std::string& image, const std::string& owner,
                 const std::string& description, ProcGroup group, bool critical,
                 float baseCpu, float baseMem, int threads, bool usesNet);
    void     Tick();
    void     SpawnJob();
};
