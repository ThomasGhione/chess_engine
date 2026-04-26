#include "uci.hpp"

#include "../engine/engine.hpp"

#include <charconv>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
    using std::string_view;

    static bool isSpace(const char c) noexcept {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    }

    static char asciiLower(const char c) noexcept {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    static string_view trimLeft(string_view s) noexcept {
        std::size_t i = 0;
        while (i < s.size() && isSpace(s[i])) ++i;
        return s.substr(i);
    }

    static string_view trimRight(string_view s) noexcept {
        while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
        return s;
    }

    static bool iequalsAscii(string_view lhs, string_view rhs) noexcept {
        if (lhs.size() != rhs.size()) return false;
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            if (asciiLower(lhs[i]) != asciiLower(rhs[i])) return false;
        }
        return true;
    }

    static std::string normalizedOptionName(string_view optionName) {
        std::string normalized;
        normalized.reserve(optionName.size());
        for (const char c : optionName) {
            if (c == ' ' || c == '_' || c == '-') continue;
            normalized.push_back(asciiLower(c));
        }
        return normalized;
    }

    static bool parseCheckValue(string_view rawValue, bool& outValue) noexcept {
        const string_view value = trimLeft(rawValue);
        if (iequalsAscii(value, "true") || value == "1" || iequalsAscii(value, "on")) {
            outValue = true;
            return true;
        }
        if (iequalsAscii(value, "false") || value == "0" || iequalsAscii(value, "off")) {
            outValue = false;
            return true;
        }
        return false;
    }

    static string_view nextToken(string_view& text) noexcept {
        text = trimLeft(text);
        if (text.empty()) return {};
        std::size_t end = 0;
        while (end < text.size() && !isSpace(text[end])) ++end;
        const string_view token = text.substr(0, end);
        text.remove_prefix(end);
        return token;
    }

    static bool parseInt(string_view token, int& out) noexcept {
        if (token.starts_with('+')) token.remove_prefix(1);
        if (token.empty()) return false;
        const auto [ptr, err] = std::from_chars(token.data(), token.data() + token.size(), out);
        return err == std::errc{} && ptr == token.data() + token.size();
    }

    static bool splitCommand(string_view command, string_view name, string_view& args) noexcept {
        if (!command.starts_with(name)) return false;
        if (command.size() == name.size()) {
            args = {};
            return true;
        }
        if (!isSpace(command[name.size()])) return false;
        args = trimLeft(command.substr(name.size()));
        return true;
    }

    static std::size_t findWord(string_view text, string_view word) noexcept {
        std::size_t pos = text.find(word);
        while (pos != string_view::npos) {
            const std::size_t end = pos + word.size();
            const bool leftOk = (pos == 0) || isSpace(text[pos - 1]);
            const bool rightOk = (end == text.size()) || isSpace(text[end]);
            if (leftOk && rightOk) return pos;
            pos = text.find(word, pos + 1);
        }
        return string_view::npos;
    }

    static chess::Coords parseSquare(string_view sq) noexcept {
        if (sq.size() != 2) return {};
        const char file = asciiLower(sq[0]);
        const char rank = sq[1];
        if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return {};
        return chess::Coords(
            static_cast<uint8_t>(file - 'a'),
            static_cast<uint8_t>('8' - rank));
    }

    static engine::Engine& defaultEngineInstance() noexcept {
        static engine::Engine engine;
        return engine;
    }
}

namespace uci {
    UCI::UCI() : engine(defaultEngineInstance()) {}

    UCI::UCI(engine::Engine& e) : engine(e) {}

    [[noreturn]] void UCI::mainLoop() noexcept {
        std::string command;
        while (std::getline(std::cin, command)) {
            if (!command.empty() && command.back() == '\r') command.pop_back();
            if (command.empty()) continue;
            parseCommand(command);
        }
        std::exit(EXIT_SUCCESS);
    }

    void UCI::parseCommand(std::string_view command) noexcept {
        string_view args;
        if (command == "quit") return quit();
        if (command == "uci") return uci();
        if (command == "ucinewgame") return ucinewgame();
        if (command == "isready") return isready();
        if (command == "stop") return stop();
        if (command == "ponderhit") return ponderhit();
        if (splitCommand(command, "setoption", args)) return setOption(args);
        if (splitCommand(command, "position", args)) return position(args);
        if (splitCommand(command, "go", args)) return go(args);
    }


    void UCI::quit() noexcept {
        std::exit(EXIT_SUCCESS);
    }

    void UCI::uci() noexcept {
        std::cout
            << "id name Fenty The Chess Engine 1.0.0\n"
            << "id author Thomas Ghione, Daniele Ferretti, Simone Tomasella\n"
            << "option name PonderDebug type check default false\n"
            << "option name SearchApiMutexGuard type check default true\n"
            << "uciok\n";
    }

    void UCI::setOption(std::string_view args) noexcept {
        if (args.empty()) return;

        string_view rest = args;
        if (nextToken(rest) != "name") return;

        const std::size_t valuePos = findWord(rest, "value");
        const string_view optionName =
            (valuePos == string_view::npos) ? trimRight(rest) : trimRight(rest.substr(0, valuePos));
        const string_view optionValue =
            (valuePos == string_view::npos) ? string_view{} : trimLeft(rest.substr(valuePos + 5));

        if (optionName.empty()) return;
        const std::string normalizedName = normalizedOptionName(optionName);
        if (normalizedName != "ponderdebug" && normalizedName != "searchapimutexguard") return;

        bool enabled = false;
        if (optionValue.empty() || !parseCheckValue(optionValue, enabled)) {
            std::cout << "info string invalid value for " << optionName << ": '" << optionValue
                      << "' (use true/false)\n";
            return;
        }

        if (normalizedName == "ponderdebug") {
            engine.setPonderDebugEnabled(enabled);
            std::cout << "info string PonderDebug "
                      << (engine.isPonderDebugEnabled() ? "enabled" : "disabled")
                      << '\n';
            return;
        }

        engine.setSearchApiMutexEnabled(enabled);
        std::cout << "info string SearchApiMutexGuard "
                  << (engine.isSearchApiMutexEnabled() ? "enabled" : "disabled")
                  << '\n';
    }

    void UCI::position(std::string_view command) noexcept {
        engine.stopThinking();
        string_view moves;

        if (command.starts_with("startpos") && (command.size() == 8 || isSpace(command[8]))) {
            engine.board = chess::Board{};
            const std::size_t movesPos = findWord(command, "moves");
            if (movesPos != string_view::npos) moves = command.substr(movesPos);
        } else if (command.starts_with("fen") && (command.size() == 3 || isSpace(command[3]))) {
            string_view fenPayload = trimLeft(command.substr(3));
            const std::size_t movesPos = findWord(fenPayload, "moves");
            if (movesPos == string_view::npos) {
                parseFEN(fenPayload);
            } else {
                parseFEN(trimRight(fenPayload.substr(0, movesPos)));
                moves = fenPayload.substr(movesPos);
            }
        } else {
            return;
        }

        engine.bestMove = chess::Board::Move{};
        engine.moveHistory.clear();
        
        if (!moves.empty()) parseMoves(moves);
        
        engine.updateGameResult();
    }

    void UCI::ucinewgame() noexcept {
        engine.stopThinking();
        engine.reset();
    }

    void UCI::isready() noexcept {
        std::cout << "readyok\n";
    }
    
    void UCI::go(std::string_view args) noexcept {
        uint64_t requestedDepth = engine::Engine::DEFAULTDEPTH;

        while (!args.empty()) {
            const string_view token = nextToken(args);
            if (token.empty()) break;

            if (token == "depth") {
                int depth = 0;
                if (parseInt(nextToken(args), depth) && depth >= 0) {
                    requestedDepth = static_cast<uint64_t>(depth);
                }
                continue;
            }

            if (token == "wtime" || token == "btime" || token == "winc" || token == "binc" || 
                token == "movetime" || token == "movestogo" || token == "mate" || token == "nodes") {
                (void)nextToken(args);
            }
        }

        engine.stopThinking();
        const chess::Board::Move bestMove = engine.searchUCI(requestedDepth);
        std::cout << "bestmove " << bestMove.toUCIString() << '\n';
    }
    
    void UCI::stop() noexcept {
        engine.stopThinking();
    }

    void UCI::ponderhit() noexcept {}

    void UCI::parseMoves(std::string_view moves) noexcept {
        moves = trimLeft(moves);
        if (!moves.starts_with("moves")) return;
        moves.remove_prefix(5);

        while (true) {
            const string_view move = nextToken(moves);
            if (move.empty()) break;
            if (move.size() < 4) continue;

            const chess::Coords from = parseSquare(move.substr(0, 2));
            const chess::Coords to = parseSquare(move.substr(2, 2));
            if (!chess::Coords::isInBounds(from) || !chess::Coords::isInBounds(to)) continue;

            const char promo = (move.size() > 4) ? asciiLower(move[4]) : '\0';
            engine.movePiece(from, to, promo);
        }
    }

    void UCI::parseFEN(std::string_view fen) noexcept {
        engine.board = chess::Board(std::string(trimRight(trimLeft(fen))));
    }
}
