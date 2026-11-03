// Menus and Command objects (Command pattern, DD-05, Phase 3 Figure 3.3).
// A user only ever sees the commands whose requiredFunction() is in permittedFunctions().
#pragma once
#include "business/Services.h"
#include "presentation/InputReader.h"

#include <functional>
#include <iosfwd>
#include <memory>
#include <vector>

namespace lms {

// Everything a command needs; main() builds it once.
struct UiContext {
    AuthService& auth;
    UserService& users;
    CourseService& courses;
    AssignmentService& assignments;
    SubmissionService& submissions;
    MarkingService& marking;
    ModerationService& moderation;
    AuditService& audit;
    BackupService& backup;
    ReportExporter& exporter;
    IClock& clock;
    InputReader& in;
    std::ostream& out;
};

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::string label() const = 0;
    virtual Function requiredFunction() const = 0;
    // Validates input, then calls one service method.
    virtual void execute(Session& s, UiContext& ui) = 0;
};

// One command object per menu action; the action body is supplied by the catalogue.
class ActionCommand : public ICommand {
public:
    using Action = std::function<void(Session&, UiContext&)>;
    ActionCommand(std::string label, Function f, Action a)
        : label_(std::move(label)), function_(f), action_(std::move(a)) {}
    std::string label() const override { return label_; }
    Function requiredFunction() const override { return function_; }
    void execute(Session& s, UiContext& ui) override { action_(s, ui); }
private:
    std::string label_;
    Function function_;
    Action action_;
};

class Menu {
public:
    Menu(std::string title, int level) : title_(std::move(title)), level_(level) {}
    void add(std::unique_ptr<ICommand> c) { commands_.push_back(std::move(c)); }
    void addSubmenu(Menu m) { submenus_.push_back(std::move(m)); }

    size_t size() const { return submenus_.size() + commands_.size(); }
    bool empty() const { return size() == 0; }
    int level() const { return level_; }
    void show(std::ostream& out) const;
    bool isSubmenu(size_t index) const { return index < submenus_.size(); }
    Menu& submenu(size_t index) { return submenus_[index]; }
    ICommand& command(size_t index) { return *commands_[index - submenus_.size()]; }

private:
    std::string title_;
    int level_;
    std::vector<std::unique_ptr<ICommand>> commands_;
    std::vector<Menu> submenus_;
};

struct CommandGroup {
    std::string title;
    std::vector<std::unique_ptr<ICommand>> commands;
};

class MenuBuilder {
public:
    // Main menu (level 1) with one submenu (level 2) per group that has a permitted command.
    // No menu item is ever created for a function the role does not have (FR-03).
    static Menu build(const User& user, std::vector<CommandGroup> catalogue);
};

// The full catalogue of commands for all roles; MenuBuilder filters it (Commands.cpp).
std::vector<CommandGroup> buildCatalogue();

} // namespace lms
