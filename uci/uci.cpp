#include "uci.hpp"
#include <sstream>
#include <algorithm>

namespace uci {
    
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
        else if (command == "setoption") {
            setOption();
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
        std::cout << "id name Fenty The Chess Engine Beta 0.1.0" << std::endl;
        std::cout << "id author Thomas Ghione, Daniele Ferretti, Simone Tomasella" << std::endl;
        std::cout << "uciok" << std::endl;
    }

    void UCI::setOption() noexcept {

    }

    void UCI::position(const std::string& command) noexcept {
        engine.reset();
        
        if (command.find("startpos") == 0) {
            auto movesPos = command.find("moves");
            if (movesPos != std::string::npos) {
                parseMoves(command.substr(movesPos));
            }
            return;
        }
        else if (command.find("fen") == 0) {
            auto movesPos = command.find("moves");
            if (movesPos == std::string::npos) {
                parseFEN(command.substr(4));
            }
            else {
                parseFEN(command.substr(4, movesPos - 4));
                parseMoves(command.substr(command.find("moves")));
            }
        }
    }

    void UCI::ucinewgame() noexcept {
        engine.reset();
    }

    void UCI::isready() noexcept {
        // Not final implementation
        std::cout << "readyok" << std::endl;
    }
    
    void UCI::go(const std::string& args) noexcept {
        // Minimal parsing for common UCI 'go' options: depth and movetime.
        // If depth provided, use it. If not, fall back to engine.depth or default.
        uint64_t requestedDepth = engine.depth;

        if (requestedDepth == 0) requestedDepth = engine::Engine::DEFAULTDEPTH;

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

        engine.search(requestedDepth);
        chess::Board::Move bestMove = engine.bestMove;
        std::cout << "bestmove " << bestMove.toUCIString() << std::endl;
    }
    
    void UCI::stop() noexcept {
        
    }

    void UCI::ponderhit() noexcept {

    }


    // Private parsing helpers

    void UCI::parseMoves(const std::string& moves) noexcept {
        if (moves.empty() || (moves.size() < 6 || moves.substr(0, 6) != "moves ")) {
            return;
        }

        std::string movesList = moves.substr(6);

        std::string::size_type delimiterPos = 0;
        while ((delimiterPos = movesList.find(' ')) != std::string::npos) {
            std::string move = movesList.substr(0, delimiterPos);

            if (move.size() >= 4) {
                engine.movePiece(chess::Coords(move.substr(0,2)), chess::Coords(move.substr(2,2)),
                                 move.size() > 4 ? move[4] : '\0');
            }

            movesList.erase(0, delimiterPos + 1);
        }

        // Last move (or only move if no spaces)
        if (!movesList.empty() && movesList.size() >= 4) {
            engine.movePiece(chess::Coords(movesList.substr(0,2)), chess::Coords(movesList.substr(2,2)),
                             movesList.size() > 4 ? movesList[4] : '\0');
        }
    }

    void UCI::parseFEN(const std::string& fen) noexcept {
        this->engine.board = chess::Board(fen);
    }
}
