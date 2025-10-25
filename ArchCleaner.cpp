#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>

class ArchCleaner {
private:
    bool verbose;
    std::string homeDir;
    
    std::string getHomeDir() {
        const char* home = getenv("HOME");
        if (home) return std::string(home);
        
        struct passwd* pw = getpwuid(getuid());
        if (pw) return std::string(pw->pw_dir);
        
        return "";
    }
    
    bool checkRoot() {
        return geteuid() == 0;
    }
    
    bool requestRootAccess() {
        if (checkRoot()) return true;
        
        std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
        std::cout << "║   Root access required for cleaning   ║" << std::endl;
        std::cout << "╚════════════════════════════════════════╝\n" << std::endl;
        
        std::cout << "This program requires administrator privileges." << std::endl;
        std::cout << "Please enter your password to continue.\n" << std::endl;
        
        int result = system("sudo -v");
        
        if (result != 0) {
            std::cerr << "\n✗ Authentication failed!" << std::endl;
            std::cerr << "Cannot proceed without root access." << std::endl;
            return false;
        }
        
        std::cout << "\n✓ Authentication successful!\n" << std::endl;
        return true;
    }
    
    void printHeader(const std::string& text) {
        std::cout << "\n=== " << text << " ===" << std::endl;
    }
    
    long long getDirectorySize(const std::string& path) {
        std::string cmd = "sudo du -sb " + path + " 2>/dev/null | cut -f1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return 0;
        
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        
        return result.empty() ? 0 : std::stoll(result);
    }
    
    std::string formatSize(long long bytes) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int i = 0;
        double size = bytes;
        
        while (size >= 1024 && i < 3) {
            size /= 1024;
            i++;
        }
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[i]);
        return std::string(buffer);
    }
    
    int executeCommand(const std::string& cmd) {
        if (verbose) {
            std::cout << "Executing: " << cmd << std::endl;
        }
        return system(cmd.c_str());
    }
    
public:
    ArchCleaner(bool v = true) : verbose(v) {
        homeDir = getHomeDir();
    }
    
    bool initialize() {
        return requestRootAccess();
    }
    
    void cleanPackageCache() {
        printHeader("Cleaning package cache");
        
        long long before = getDirectorySize("/var/cache/pacman/pkg");
        std::cout << "Cache size before cleaning: " << formatSize(before) << std::endl;
        
        std::cout << "Removing all uninstalled packages..." << std::endl;
        executeCommand("sudo pacman -Sc --noconfirm");
        
        long long after = getDirectorySize("/var/cache/pacman/pkg");
        std::cout << "Cache size after cleaning: " << formatSize(after) << std::endl;
        std::cout << "Freed: " << formatSize(before - after) << std::endl;
    }
    
    void removeOrphans() {
        printHeader("Removing orphaned packages");
        
        FILE* pipe = popen("pacman -Qdtq 2>/dev/null", "r");
        if (!pipe) {
            std::cout << "Error checking orphaned packages" << std::endl;
            return;
        }
        
        std::vector<std::string> orphans;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string pkg = buffer;
            pkg.erase(pkg.find_last_not_of("\n\r") + 1);
            if (!pkg.empty()) orphans.push_back(pkg);
        }
        pclose(pipe);
        
        if (orphans.empty()) {
            std::cout << "No orphaned packages found" << std::endl;
            return;
        }
        
        std::cout << "Found orphaned packages: " << orphans.size() << std::endl;
        for (const auto& pkg : orphans) {
            std::cout << "  - " << pkg << std::endl;
        }
        
        executeCommand("sudo pacman -Rns $(pacman -Qdtq) --noconfirm 2>/dev/null");
        std::cout << "Orphaned packages removed" << std::endl;
    }
    
    void cleanUserCache() {
        printHeader("Cleaning user cache");
        
        std::string cachePath = homeDir + "/.cache";
        long long before = getDirectorySize(cachePath);
        
        std::cout << "Size of ~/.cache before cleaning: " << formatSize(before) << std::endl;
        std::cout << "Cleaning old cache files..." << std::endl;
        
        executeCommand("find " + cachePath + " -type f -atime +30 -delete 2>/dev/null");
        executeCommand("find " + cachePath + " -type d -empty -delete 2>/dev/null");
        
        long long after = getDirectorySize(cachePath);
        std::cout << "Size of ~/.cache after cleaning: " << formatSize(after) << std::endl;
        std::cout << "Freed: " << formatSize(before - after) << std::endl;
    }
    
    void cleanJournalLogs() {
        printHeader("Cleaning systemd journal logs");
        
        long long before = getDirectorySize("/var/log/journal");
        std::cout << "Log size before cleaning: " << formatSize(before) << std::endl;
        
        std::cout << "Limiting logs to 50MB..." << std::endl;
        executeCommand("sudo journalctl --vacuum-size=50M");
        
        long long after = getDirectorySize("/var/log/journal");
        std::cout << "Log size after cleaning: " << formatSize(after) << std::endl;
        std::cout << "Freed: " << formatSize(before - after) << std::endl;
    }
    
    void cleanTempFiles() {
        printHeader("Cleaning temporary files");
        
        long long before = getDirectorySize("/tmp");
        std::cout << "Size of /tmp before cleaning: " << formatSize(before) << std::endl;
        
        executeCommand("sudo find /tmp -type f -atime +7 -delete 2>/dev/null");
        executeCommand("sudo find /tmp -type d -empty -delete 2>/dev/null");
        
        long long after = getDirectorySize("/tmp");
        std::cout << "Size of /tmp after cleaning: " << formatSize(after) << std::endl;
        std::cout << "Freed: " << formatSize(before - after) << std::endl;
    }
    
    void cleanTrash() {
        printHeader("Cleaning trash");
        
        std::string trashPath = homeDir + "/.local/share/Trash";
        long long before = getDirectorySize(trashPath);
        
        std::cout << "Trash size before cleaning: " << formatSize(before) << std::endl;
        executeCommand("rm -rf " + trashPath + "/* 2>/dev/null");
        
        std::cout << "Trash cleaned, freed: " << formatSize(before) << std::endl;
    }
    
    void cleanAURCache() {
        printHeader("Cleaning AUR helper cache");
        
        std::vector<std::string> aurPaths = {
            homeDir + "/.cache/yay",
            homeDir + "/.cache/paru",
            homeDir + "/.cache/pikaur"
        };
        
        long long totalFreed = 0;
        for (const auto& path : aurPaths) {
            struct stat info;
            if (stat(path.c_str(), &info) == 0) {
                long long size = getDirectorySize(path);
                std::cout << "Cleaning " << path << " (" << formatSize(size) << ")" << std::endl;
                executeCommand("rm -rf " + path + "/* 2>/dev/null");
                totalFreed += size;
            }
        }
        
        if (totalFreed > 0) {
            std::cout << "Freed: " << formatSize(totalFreed) << std::endl;
        } else {
            std::cout << "AUR helper cache not found" << std::endl;
        }
    }
    
    void fullClean() {
        std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
        std::cout << "║   Arch Linux System Cleaner v1.0      ║" << std::endl;
        std::cout << "╚════════════════════════════════════════╝" << std::endl;
        
        cleanPackageCache();
        removeOrphans();
        cleanJournalLogs();
        cleanTempFiles();
        cleanUserCache();
        cleanTrash();
        cleanAURCache();
        
        printHeader("Cleaning completed");
        std::cout << "✓ System cleaned successfully!" << std::endl;
    }
};

void printHelp() {
    std::cout << "Usage: arch-cleaner [options]\n" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help          Show this help message" << std::endl;
    std::cout << "  -a, --all           Full cleaning (default)" << std::endl;
    std::cout << "  -p, --packages      Clean only package cache" << std::endl;
    std::cout << "  -o, --orphans       Remove only orphaned packages" << std::endl;
    std::cout << "  -c, --cache         Clean only user cache" << std::endl;
    std::cout << "  -j, --journal       Clean only systemd journal" << std::endl;
    std::cout << "  -t, --temp          Clean only temporary files" << std::endl;
    std::cout << "  -r, --trash         Clean only trash" << std::endl;
    std::cout << "  -u, --aur           Clean only AUR cache" << std::endl;
    std::cout << "  -q, --quiet         Quiet mode" << std::endl;
    std::cout << "\nNote: This program requires administrator privileges" << std::endl;
}

int main(int argc, char* argv[]) {
    bool verbose = true;
    bool runFull = true;
    
    ArchCleaner cleaner(verbose);
    
    if (!cleaner.initialize()) {
        return 1;
    }
    
    if (argc > 1) {
        runFull = false;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                printHelp();
                return 0;
            } else if (arg == "-q" || arg == "--quiet") {
                verbose = false;
                cleaner = ArchCleaner(verbose);
            } else if (arg == "-a" || arg == "--all") {
                runFull = true;
            } else if (arg == "-p" || arg == "--packages") {
                cleaner.cleanPackageCache();
            } else if (arg == "-o" || arg == "--orphans") {
                cleaner.removeOrphans();
            } else if (arg == "-c" || arg == "--cache") {
                cleaner.cleanUserCache();
            } else if (arg == "-j" || arg == "--journal") {
                cleaner.cleanJournalLogs();
            } else if (arg == "-t" || arg == "--temp") {
                cleaner.cleanTempFiles();
            } else if (arg == "-r" || arg == "--trash") {
                cleaner.cleanTrash();
            } else if (arg == "-u" || arg == "--aur") {
                cleaner.cleanAURCache();
            } else {
                std::cerr << "Unknown option: " << arg << std::endl;
                printHelp();
                return 1;
            }
        }
    }
    
    if (runFull) {
        cleaner.fullClean();
    }
    
    return 0;
}