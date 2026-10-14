#include "app/App.hpp"

#ifdef IAN_PRESENTATION_SMOKE_TEST
#include <string_view>
namespace ian { int runPresentationEventsSmoke(); }
#endif
int main(int argc, char** argv) {
#ifdef IAN_PRESENTATION_SMOKE_TEST
    if(argc==2 && std::string_view(argv[1])=="--test-presentation-events") return ian::runPresentationEventsSmoke();
#else
    static_cast<void>(argc);static_cast<void>(argv);
#endif
    ian::App app;
    return app.run();
}
