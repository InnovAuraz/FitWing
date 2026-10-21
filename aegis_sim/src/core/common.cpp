#include "core/common.h"

#include <cstdarg>

namespace aegis {

void logLine(const char* level, const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[FitWingSim][%s] %s\n", level, buf);
    std::fflush(stderr);
}

}  // namespace aegis
