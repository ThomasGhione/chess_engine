#include "uci.hpp"

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
            position(command.substr(9)); // pass the rest of the command
        }
        else if (command == "ucinewgame") {
            ucinewgame();
        }
        else if (command == "isready") {
            isready();
        }
        else if (command.find("go") == 0) { // starts with "go"
            go();
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
        std::cout << "id name Fenty The Chess Engine Beta 0.1.0\n";
        std::cout << "id author Thomas Ghione, Daniele Ferretti, Simone Tomasella\n";
        std::cout << "uciok\n";
    }

    void UCI::setOption() noexcept {

    }

    void UCI::position(const std::string& command) noexcept {
        engine.reset();
        
        if (command.find("startpos") == 0) {
            parseMoves(command.substr(9));
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
        std::cout << "readyok\n";
    }
    
    void UCI::go() noexcept {
        engine.search(engine.depth);
        chess::Board::Move bestMove = engine.bestMove;
        std::cout << "bestmove " << bestMove.toUCIString() << "\n";
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

        int delimiterPos = 0;
        while ((delimiterPos = movesList.find(' ')) != std::string::npos) {
            std::string move = movesList.substr(0, delimiterPos);

            engine.movePiece(chess::Coords(move.substr(0,2)), chess::Coords(move.substr(2,2)),
                             move.size() > 4 ? move[4] : '\0');

            movesList.erase(0, delimiterPos + 1);
        }

        // Last move (or only move if no spaces)
        engine.movePiece(chess::Coords(movesList.substr(0,2)), chess::Coords(movesList.substr(2,2)),
                         movesList.size() > 4 ? movesList[4] : '\0');
    }

    void UCI::parseFEN(const std::string& fen) noexcept {
        this->engine.board = chess::Board(fen);
    }
}