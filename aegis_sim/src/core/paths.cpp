#include "core/paths.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace aegis {

static std::string toForward(std::string s) {
    for (char& c : s)
        if (c == '\\') c = '/';
    return s;
}

std::string exeDir() {
    static std::string cached;
    if (!cached.empty()) return cached;
    fs::path p;
#if defined(_WIN32)
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
    if (n > 0) p = fs::path(std::wstring(buf, n)).parent_path();
#elif defined(__linux__)
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = 0;
        p = fs::path(buf).parent_path();
    }
#endif
    if (p.empty()) p = fs::current_path();
    cached = toForward(p.string());
    return cached;
}

std::string findRuntimeDir(const std::string& name) {
    std::error_code ec;
    std::vector<fs::path> candidates;
    fs::path exe = exeDir();
    candidates.push_back(exe / name);
    candidates.push_back(fs::current_path(ec) / name);
    candidates.push_back(exe / ".." / name);
    candidates.push_back(exe / ".." / ".." / name);
#ifdef FITWING_SIM_SOURCE_DIR
    candidates.push_back(fs::path(FITWING_SIM_SOURCE_DIR) / name);
#endif
    for (const auto& c : candidates) {
        if (fs::is_directory(c, ec)) return toForward(fs::weakly_canonical(c, ec).string());
    }
    return "";
}

bool readTextFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool writeTextFileAtomic(const std::string& path, const std::string& text) {
    std::error_code ec;
    fs::path target(path);
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << text;
        f.flush();
        if (!f) return false;
    }
    fs::remove(target, ec);
    fs::rename(tmp, target, ec);
    if (ec) {
        // Fallback: direct write
        std::ofstream f(target, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << text;
        return static_cast<bool>(f);
    }
    return true;
}

}  // namespace aegis
