#include "app/App.hpp"

#include <raylib.h>

#include <cstdio>

#ifdef IAN_PRESENTATION_SMOKE_TEST
#include <string_view>
#include <charconv>
namespace ian { int runPresentationEventsSmoke(); int runFrameBenchmark(const char*, int); }
#endif
int main(int argc, char** argv) {
#ifdef IAN_PRESENTATION_SMOKE_TEST
    if (argc == 4 && std::string_view(argv[1]) == "--benchmark-frames") {
        const std::string_view count{argv[2]};
        int frames{};
        const auto parsed = std::from_chars(count.data(), count.data()+count.size(), frames);
        if (parsed.ec != std::errc{} || parsed.ptr != count.data()+count.size() || frames < 1 || frames > 100000) return 2;
        return ian::runFrameBenchmark(argv[3], frames);
    }
    if(argc==2 && std::string_view(argv[1])=="--test-presentation-events") return ian::runPresentationEventsSmoke();
#else
    static_cast<void>(argc);static_cast<void>(argv);
#endif
    // Runtime assets are packaged beside the executable. Finder and shortcuts
    // may start us from any working directory, so resolve relative paths here.
    if (!ChangeDirectory(GetApplicationDirectory())) {
        std::fputs("Failed to locate the game directory.\n", stderr);
        return 1;
    }
    ian::App app;
    return app.run();
}
