#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <glob.h>
#include <unistd.h>
#include <sys/wait.h>

// Strict whitelist — any input not here is rejected before touching sysfs
static const char* VALID_GOVERNORS[] = {
    "performance",
    "powersave",
    "schedutil",
    "ondemand",
    "conservative",
    nullptr
};

bool isValidGovernor(const char* gov) {
    for (int i = 0; VALID_GOVERNORS[i] != nullptr; i++) {
        if (strcmp(gov, VALID_GOVERNORS[i]) == 0) return true;
    }
    return false;
}

std::string readSysfs(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string val;
    std::getline(f, val);
    return val;
}

bool writeSysfs(const std::string& path, const std::string& val) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << val << std::endl;
    return f.good();
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;

    const char* governor = argv[1];
    if (!isValidGovernor(governor)) return 2;

    // Override ACPI _PPC (Performance Present Capabilities) limit.
    // Some BIOSes (notably Acer Nitro and similar) use _PPC to cap
    // CPU frequency on battery, making governor selection irrelevant.
    // We own frequency management — ignore the BIOS cap.
    writeSysfs("/sys/module/processor/parameters/ignore_ppc", "1");

    // Glob all CPU cpufreq paths
    glob_t globbuf;
    if (glob("/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor",
             GLOB_NOSORT, nullptr, &globbuf) != 0) return 3;

    int result = 0;
    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
        std::string govPath = globbuf.gl_pathv[i];
        std::string basePath = govPath.substr(0, govPath.rfind('/') + 1);

        // Read hardware frequency bounds for this core
        std::string maxFreq = readSysfs(basePath + "cpuinfo_max_freq");
        std::string minFreq = readSysfs(basePath + "cpuinfo_min_freq");

        if (maxFreq.empty() || minFreq.empty()) {
            result = 4;
            continue;
        }

        // Restore full frequency range — undo any pinning left by
        // power-profiles-daemon or ACPI _PPC. The governor then
        // manages actual frequency within the full hardware range.
        if (!writeSysfs(basePath + "scaling_min_freq", minFreq)) result = 4;
        if (!writeSysfs(basePath + "scaling_max_freq", maxFreq)) result = 4;

        // Set the governor
        if (!writeSysfs(govPath, governor)) result = 4;
    }

    // Force hardware TDP limits via ryzenadj.
    // Must use fork()+execv() — system() launches bash which drops setuid root,
    // so ryzenadj would silently fail without /dev/mem access.
    //
    // Values differ by power source:
    //   AC:      chip can draw its rated spec (15W STAPM, 25W fast burst)
    //   Battery: physically limited by AN515-42 battery delivery (~9W APU max)

    bool onAC = (readSysfs("/sys/class/power_supply/ACAD/online") == "1");

    const char* stapm;
    const char* fast;
    const char* slow;
    const char* temp;

    if (strcmp(governor, "performance") == 0) {
        stapm = onAC ? "--stapm-limit=15000" : "--stapm-limit=9000";
        fast  = onAC ? "--fast-limit=25000"  : "--fast-limit=12000";
        slow  = onAC ? "--slow-limit=15000"  : "--slow-limit=9000";
        temp  = "--tctl-temp=85";
    } else if (strcmp(governor, "schedutil") == 0 || strcmp(governor, "ondemand") == 0) {
        stapm = onAC ? "--stapm-limit=10000" : "--stapm-limit=6000";
        fast  = onAC ? "--fast-limit=15000"  : "--fast-limit=8000";
        slow  = onAC ? "--slow-limit=10000"  : "--slow-limit=6000";
        temp  = "--tctl-temp=75";
    } else if (strcmp(governor, "powersave") == 0 || strcmp(governor, "conservative") == 0) {
        stapm = onAC ? "--stapm-limit=6000"  : "--stapm-limit=3000";
        fast  = onAC ? "--fast-limit=8000"   : "--fast-limit=4000";
        slow  = onAC ? "--slow-limit=6000"   : "--slow-limit=3000";
        temp  = "--tctl-temp=65";
    } else {
        stapm = nullptr;
    }

    if (stapm != nullptr) {
        const char* args[] = {
            "/usr/bin/ryzenadj", stapm, fast, slow, temp, nullptr
        };
        pid_t pid = fork();
        if (pid == 0) {
            close(1); close(2);
            execv("/usr/bin/ryzenadj", const_cast<char* const*>(args));
            _exit(1);
        } else if (pid > 0) {
            waitpid(pid, nullptr, 0);
        }
    }

    globfree(&globbuf);
    return result;
}
