#include "presentation/ConsoleApp.h"

#include <iostream>

namespace lms {

int ConsoleApp::run() {
    try {
        ui_.out << "\n  MODERATED LEARNING MANAGEMENT SYSTEM\n  National University of Lesotho - CS4433\n";
        for (;;) {
            std::unique_ptr<Session> session = loginScreen();
            if (!session) break;
            try {
                // After every line typed, check the 10-minute limit and record activity (FR-05).
                Session* sp = session.get();
                ui_.in.setActivityHook([this, sp] {
                    const TimePoint now = ui_.clock.now();
                    if (sp->isExpired(now)) throw SessionExpiredException();
                    sp->touch(now);
                });
                if (session->getUser().mustChangePassword()) forcePasswordChange(*session);
                if (session->getUser().getRole() == Role::STUDENT) {
                    const int n = ui_.submissions.releasedSinceLastLogin(*session);
                    if (n > 0) ui_.out << "\n  " << n << " mark(s) were released since your last login.\n";
                }
                Menu menu = MenuBuilder::build(session->getUser(), buildCatalogue());
                runMenu(menu, *session);
                ui_.auth.logout(*session);
                ui_.out << "\n  You have been logged out.\n";
            } catch (const SessionExpiredException& e) {
                // Half-finished actions are discarded without saving (FR-05).
                ui_.out << "\n  " << e.userMessage() << " Please log in again.\n";
            }
            ui_.in.clearActivityHook();
        }
    } catch (const EndOfInput&) {
        ui_.out << "\n";
    }
    ui_.out << "  Goodbye.\n";
    return 0;
}

std::unique_ptr<Session> ConsoleApp::loginScreen() {
    for (;;) {
        try {
            ui_.out << "\n--- Log in ---\n";
            const std::string user = ui_.in.readLine("Username (blank to exit): ");
            if (trim(user).empty()) return nullptr;
            const std::string pw = ui_.in.readPassword("Password: ");
            return ui_.auth.login(user, pw);
        } catch (const EndOfInput&) {
            throw;
        } catch (const LmsException& e) {
            ui_.out << "  " << e.userMessage() << "\n";
        } catch (const std::exception&) {
            ui_.out << "  An unexpected error occurred. Please try again.\n";
        }
    }
}

void ConsoleApp::forcePasswordChange(Session& s) {
    ui_.out << "\n  You must choose a new password before you continue.\n"
               "  It needs at least 8 characters with a letter and a digit.\n";
    while (s.getUser().mustChangePassword()) {
        try {
            const std::string cur = ui_.in.readPassword("Current (temporary) password: ");
            const std::string n1 = ui_.in.readPassword("New password: ");
            const std::string n2 = ui_.in.readPassword("Repeat new password: ");
            if (n1 != n2) { ui_.out << "  The two passwords are different.\n"; continue; }
            ui_.auth.changePassword(s, cur, n1);
            ui_.out << "  Password changed.\n";
        } catch (const SessionExpiredException&) {
            throw;
        } catch (const LmsException& e) {
            ui_.out << "  " << e.userMessage() << "\n";
        }
    }
}

void ConsoleApp::runCommand(ICommand& cmd, Session& s) {
    try {
        cmd.execute(s, ui_);
    } catch (const SessionExpiredException&) {
        throw;
    } catch (const EndOfInput&) {
        throw;
    } catch (const LmsException& e) {
        ui_.out << "\n  " << e.userMessage() << "\n";
    } catch (const std::exception&) {
        // Logged without sensitive data elsewhere; the user sees a general message (NFR-08).
        ui_.out << "\n  An unexpected error occurred. Nothing was changed. Returning to the menu.\n";
    }
}

void ConsoleApp::runMenu(Menu& menu, Session& s) {
    for (;;) {
        menu.show(ui_.out);
        const int choice = ui_.in.readInt("Choose: ", 0, static_cast<int>(menu.size()));
        if (choice == 0) return;
        const size_t index = static_cast<size_t>(choice - 1);
        if (menu.isSubmenu(index)) runMenu(menu.submenu(index), s);
        else runCommand(menu.command(index), s);
    }
}

} // namespace lms
