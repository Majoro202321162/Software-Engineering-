#include "presentation/InputReader.h"
#include "business/Validation.h"
#include "common/Exceptions.h"

#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace lms {
namespace {

// Switches terminal echo off while a password is typed. The only platform-specific code
// of the presentation layer.
class EchoOff {
public:
    EchoOff() {
#ifdef _WIN32
        h_ = GetStdHandle(STD_INPUT_HANDLE);
        active_ = GetConsoleMode(h_, &mode_) != 0;
        if (active_) SetConsoleMode(h_, mode_ & ~ENABLE_ECHO_INPUT);
#else
        active_ = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &old_) == 0;
        if (active_) {
            termios t = old_;
            t.c_lflag &= ~static_cast<tcflag_t>(ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &t);
        }
#endif
    }
    ~EchoOff() {
        if (!active_) return;
#ifdef _WIN32
        SetConsoleMode(h_, mode_);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &old_);
#endif
    }
    bool active() const { return active_; }
private:
    bool active_ = false;
#ifdef _WIN32
    HANDLE h_ = nullptr;
    DWORD mode_ = 0;
#else
    termios old_{};
#endif
};

} // namespace

std::string InputReader::readLine(const std::string& prompt) {
    out_ << prompt << std::flush;
    std::string line;
    if (!std::getline(in_, line)) throw EndOfInput();
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (hook_) hook_();
    return line;
}

std::string InputReader::readText(const std::string& prompt, size_t minLen, size_t maxLen) {
    for (;;) {
        std::string s = trim(readLine(prompt));
        if (s.size() >= minLen && s.size() <= maxLen) return s;
        out_ << "  Please enter " << minLen << " to " << maxLen << " characters (you entered "
             << s.size() << ").\n";
    }
}

std::string InputReader::readOptionalText(const std::string& prompt, size_t maxLen) {
    for (;;) {
        std::string s = trim(readLine(prompt));
        if (s.size() <= maxLen) return s;
        out_ << "  Please enter at most " << maxLen << " characters.\n";
    }
}

int InputReader::readInt(const std::string& prompt, int lo, int hi) {
    for (;;) {
        const std::string s = trim(readLine(prompt));
        char* end = nullptr;
        const long v = std::strtol(s.c_str(), &end, 10);
        if (!s.empty() && end && *end == '\0' && v >= lo && v <= hi) return static_cast<int>(v);
        out_ << "  Please enter a whole number from " << lo << " to " << hi << ".\n";
    }
}

std::optional<int> InputReader::readId(const std::string& prompt) {
    const int v = readInt(prompt + " (0 = cancel): ", 0, 2000000000);
    if (v == 0) return std::nullopt;
    return v;
}

double InputReader::readMark(const std::string& prompt, int maxMark) {
    for (;;) {
        const std::string s = trim(readLine(prompt));
        char* end = nullptr;
        const double v = std::strtod(s.c_str(), &end);
        if (!s.empty() && end && *end == '\0') {
            try {
                requireMarkValue("The mark", v, maxMark);
                return v;
            } catch (const ValidationException& e) {
                out_ << "  " << e.userMessage() << "\n";
                continue;
            }
        }
        out_ << "  Please enter a number from 0 to " << maxMark << " (steps of 0.5).\n";
    }
}

bool InputReader::confirm(const std::string& prompt) {
    for (;;) {
        std::string s = trim(readLine(prompt + " (yes/no): "));
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (s == "yes" || s == "y") return true;
        if (s == "no" || s == "n") return false;
        out_ << "  Please type yes or no.\n";
    }
}

std::string InputReader::readPassword(const std::string& prompt) {
    std::string s;
    {
        EchoOff guard;
        s = readLine(prompt);
        if (guard.active()) out_ << "\n";
    }
    return s;
}

TimePoint InputReader::readDateTime(const std::string& prompt) {
    for (;;) {
        const std::string s = trim(readLine(prompt + " [YYYY-MM-DD HH:MM, local time]: "));
        if (auto t = parseLocal(s)) return *t;
        out_ << "  Please use the form 2026-11-20 17:00.\n";
    }
}

} // namespace lms
