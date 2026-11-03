// Login screen, main loop, inactivity timeout and the top-level exception handler
// (Phase 3, Table 2.3, Table 2.4). The program never ends because of an error.
#pragma once
#include "presentation/Menu.h"

namespace lms {

class ConsoleApp {
public:
    explicit ConsoleApp(UiContext& ui) : ui_(ui) {}
    int run();
private:
    std::unique_ptr<Session> loginScreen();            // nullptr = user chose to exit
    void forcePasswordChange(Session& s);
    void runMenu(Menu& menu, Session& s);
    void runCommand(ICommand& cmd, Session& s);
    UiContext& ui_;
};

} // namespace lms
