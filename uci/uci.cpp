#include "uci.hpp"

#include "../engine/engine.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string_view>

namespace uci {

    static bool iequalsAscii(std::string_view lhs, std::string_view rhs) noexcept {
        if (lhs.size() != rhs.size()) return false;
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(lhs[i])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(rhs[i])));
            if (a != b) return false;
        }
        return true;
    }

    static std::string normalizedOptionName(std::string_view optionName) {
        std::string out;
        out.reserve(optionName.size());
        for (char c : optionName) {
            if (c == ' ' || c == '_' || c == '-') continue;
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    }

    static std::string_view trimLeft(std::string_view s) {
        std::size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        return s.substr(i);
    }

    static bool parseCheckValue(std::string_view rawValue, bool& outValue) noexcept {
        const std::string_view value = trimLeft(rawValue);
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
    
    UCI::UCI() 
        : engine(*(new engine::Engine())) // Default engine instance
    {}

    UCI::UCI(engine::Engine& e) 
        : engine(e) 
    {}

    [[noreturn]] void UCI::mainLoop() noexcept {
        while(true) {
            std::string command;
            std::getline(std::cin, command);
            // Trim possible Windows CRLF trailing \r from piped input
            if (!command.empty() && command.back() == '\r') command.pop_back();
            if (command.empty()) continue;
            this->parseCommand(command);
        }
    }

    void UCI::parseCommand(const std::string& command) noexcept {
        if (command == "quit") {
            quit();
        }
        else if (command == "uci") {
            uci();
        }
        else if (command.find("setoption") == 0) {
            std::string args;
            if (command.size() > 10 && command[9] == ' ') args = command.substr(10);
            setOption(args);
        }
        else if (command.find("position") == 0) { // starts with "position"
            std::string rest;
            if (command.size() > 9 && command[8] == ' ') rest = command.substr(9);
            position(rest); // pass the rest of the command (safe)
        }
        else if (command == "ucinewgame") {
            ucinewgame();
        }
        else if (command == "isready") {
            isready();
        }
        else if (command.find("go") == 0) { // starts with "go"
            std::string args;
            if (command.size() > 3 && command[2] == ' ') args = command.substr(3);
            go(args);
        }
        else if (command == "stop") {
            stop();
        }
        else if (command == "ponderhit") {
            ponderhit();
        }
    }


    // Standard UCI commands
    void UCI::quit() noexcept {
        exit(EXIT_SUCCESS);
    }

    void UCI::uci() noexcept {
        std::cout << "id name Fenty The Chess Engine 1.0.0" << std::endl;
        std::cout << "id author Thomas Ghione, Daniele Ferretti, Simone Tomasella" << std::endl;
        std::cout << "option name PonderDebug type check default false" << std::endl;
        std::cout << "uciok" << std::endl;
    }

    void UCI::setOption(const std::string& args) noexcept {
        if (args.empty()) return;

        std::istringstream iss(args);
        std::string token;
        std::string optionName;
        std::string optionValueStorage;
        std::string_view optionValue;

        while (iss >> token) {
            if (token != "name") continue;

            while (iss >> token) {
                if (token == "value") break;
                if (!optionName.empty()) optionName += ' ';
                optionName += token;
            }

            if (token == "value") {
                std::getline(iss, optionValueStorage);
                optionValue = trimLeft(optionValueStorage);
            }
            break;
        }

        if (optionName.empty()) return;

        const std::string normalized = normalizedOptionName(optionName);
        if (normalized == "ponderdebug") {
            bool enabled = false;
            if (optionValue.empty() || !parseCheckValue(optionValue, enabled)) {
                std::cout << "info string invalid value for PonderDebug: '" << optionValue
                          << "' (use true/false)" << std::endl;
                return;
            }

            engine.setPonderDebugEnabled(enabled);
            std::cout << "info string PonderDebug "
                      << (engine.isPonderDebugEnabled() ? "enabled" : "disabled")
                      << std::endl;
        }

    }

    void UCI::position(const std::string& command) noexcept {
        engine.stopThinking();

        // UCI "position" must not reset the whole engine state:
        // reset TT/history only on "ucinewgame".
        if (command.find("startpos") == 0) {
            engine.board = chess::Board();
            engine.bestMove = chess::Board::Move{};
            engine.moveHistory.clear();

            auto movesPos = command.find("moves");
            if (movesPos != std::string::npos) {
                parseMoves(command.substr(movesPos));
            }

            engine.updateGameResult();
            return;
        }
        else if (command.find("fen") == 0) {
            auto movesPos = command.find("moves");
            engine.bestMove = chess::Board::Move{};
            engine.moveHistory.clear();

            if (movesPos == std::string::npos) {
                parseFEN(command.substr(4));
            }
            else {
                parseFEN(command.substr(4, movesPos - 4));
                parseMoves(command.substr(command.find("moves")));
            }

            engine.updateGameResult();
        }
    }

    void UCI::ucinewgame() noexcept {
        engine.stopThinking();
        engine.reset();
    }

    void UCI::isready() noexcept {
        // Not final implementation
        std::cout << "readyok" << std::endl;
    }
    
    void UCI::go(const std::string& args) noexcept {
        // Minimal parsing for common UCI 'go' options: depth and movetime.
        // If depth provided, use it. If not, fall back to engine.depth or default.
        uint64_t requestedDepth = engine::Engine::DEFAULTDEPTH;

        if (!args.empty()) {
            std::istringstream iss(args);
            std::string token;
            while (iss >> token) {
                if (token == "depth") {
                    int d = 0;
                    if (iss >> d) requestedDepth = static_cast<uint64_t>(std::max(0, d));
                }
                else if (token == "movetime") {
                    // We don't have a time-based search API; fall back to default depth.
                    // Consume value so subsequent tokens (if any) are parsed correctly.
                    int mt = 0;
                    if (iss >> mt) {
                        // No-op conversion: keep requestedDepth as-is (could be improved).
                    }
                }
                else {
                    // Skip token's argument if present (e.g. wtime 3000 btime 3000)
                    if (token == "wtime" || token == "btime" || token == "winc" || token == "binc") {
                        int skipv = 0;
                        iss >> skipv;
                    }
                }
            }
        }

        engine.stopThinking(); // Stop any ongoing pondering before starting a new search
        const chess::Board::Move bestMove = engine.searchUCI(requestedDepth);
        std::cout << "bestmove " << bestMove.toUCIString() << std::endl;
    }
    
    void UCI::stop() noexcept {
        engine.stopThinking();
    }

    void UCI::ponderhit() noexcept {

    }


    // Private parsing helpers

    void UCI::parseMoves(const std::string& moves) noexcept {
        std::string_view movesView(moves);
        if (movesView.size() < 6 || !movesView.starts_with("moves ")) {
            return;
        }
        movesView.remove_prefix(6);

        const auto parseSquare = [](std::string_view sq) noexcept -> chess::Coords {
            if (sq.size() != 2) return chess::Coords{};
            const char file = static_cast<char>(std::tolower(static_cast<unsigned char>(sq[0])));
            const char rank = sq[1];
            if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return chess::Coords{};

            const uint8_t fileIdx = static_cast<uint8_t>(file - 'a');
            const uint8_t rankIdx = static_cast<uint8_t>('8' - rank);
            return chess::Coords(fileIdx, rankIdx);
        };

        std::size_t pos = 0;
        while (pos < movesView.size()) {
            while (pos < movesView.size() && movesView[pos] == ' ') ++pos;
            if (pos >= movesView.size()) break;

            std::size_t end = pos;
            while (end < movesView.size() && movesView[end] != ' ') ++end;

            const std::string_view move = movesView.substr(pos, end - pos);
            if (move.size() >= 4) {
                const chess::Coords from = parseSquare(move.substr(0, 2));
                const chess::Coords to = parseSquare(move.substr(2, 2));
                if (chess::Coords::isInBounds(from) && chess::Coords::isInBounds(to)) {
                    const char promo = (move.size() > 4) ? move[4] : '\0';
                    engine.movePiece(from, to, promo);
                }
            }

            pos = end + 1;
        }
    }

    void UCI::parseFEN(const std::string& fen) noexcept {
        this->engine.board = chess::Board(fen);
    }
}
