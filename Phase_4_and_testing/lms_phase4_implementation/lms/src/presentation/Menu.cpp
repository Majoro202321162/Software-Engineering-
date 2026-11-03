#include "presentation/Menu.h"

#include <ostream>

namespace lms {

void Menu::show(std::ostream& out) const {
    out << "\n=== " << title_ << " ===\n";
    size_t n = 1;
    for (const Menu& m : submenus_) out << "  " << n++ << ". " << m.title_ << "\n";
    for (const auto& c : commands_) out << "  " << n++ << ". " << c->label() << "\n";
    out << "  0. " << (level_ == 1 ? "Log out" : "Back") << "\n";
}

Menu MenuBuilder::build(const User& user, std::vector<CommandGroup> catalogue) {
    const std::set<Function> allowed = user.permittedFunctions();
    Menu main(user.menuTitle(), 1);
    for (CommandGroup& g : catalogue) {
        Menu sub(g.title, 2);
        for (auto& c : g.commands)
            if (allowed.count(c->requiredFunction())) sub.add(std::move(c));
        if (!sub.empty()) main.addSubmenu(std::move(sub));
    }
    return main;
}

} // namespace lms
