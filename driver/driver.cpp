#include "driver.hpp"

#include "../printer/menu.hpp"
#include "../engine/engine.hpp"
#include <array>
#include <cstdio>
#include <memory>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fstream>
#include <filesystem>

namespace driver {

    Driver::Driver(print::Menu& m, engine::Engine& e) 
        : menu(m)
        , engine(e) 
    {}

    void Driver::startGame(int argc, char *argv[]) noexcept {

        parse(argc, argv);

        while (true) {
            uint8_t mainMenuChoice = menu.mainMenu();

            switch (mainMenuChoice) {
                
                case '1': {
                    uint8_t colorChoice = menu.playWithEngineMenu();
                    Driver::quit(std::string(1, colorChoice));
                    switch (colorChoice) {
                        case '1':
                            this->playGameVsEngine(true);
                            break;
                        case '2':
                            this->playGameVsEngine(false);
                            break;
                        case '3':
                            // Back to main menu
                            break;
                        default:
                            std::cout << "Invalid option. Please select a valid option.\n";
                            break;
                    }
                    break;
                }

                case '2': {
                    this->playGameVsHuman();
                    break;

                    /*
                    uint8_t twoPlayerChoice = menu.playWithPlayerMenu();
                    Driver::quit(std::string(1, twoPlayerChoice));

                    switch (twoPlayerChoice) {
                        case '1':
                            this->playGameVsHuman();
                            break;
                        case '2':
                            // Back to main menu
                            break;
                        default:
                            std::cout << "Invalid option. Please select a valid option.\n";
                            break;
                    }
                    break;
                    */
                }

                case '3':
                    this->botVsBot();
                    break;

                case '4': {
                    uint8_t botStockfishChoice = menu.playBotVsStockfishMenu();
                    Driver::quit(std::string(1, botStockfishChoice));

                    switch (botStockfishChoice) {
                        case '1':
                            this->botVsStockfish(true);
                            break;
                        case '2':
                            this->botVsStockfish(false);
                            break;
                        case '3':
                            // Back to main menu
                            break;
                        default:
                            std::cout << "Invalid option. Please select a valid option.\n";
                            break;
                    }
                    break;
                }

                case '5':
                    if (!this->loadGame()) {
                        std::cout << "No saved game found. Returning to main menu.\n";
                    }
                    break;

                case '6':
                    std::cout << "Thank you for playing! See you next time." << std::endl;
                    exit(EXIT_SUCCESS);
                    break;

                default:
                    std::cout << "Invalid option. Please select a valid option.\n";
                    break;
            }
        } 
    }


    void Driver::parse(int argc, char *argv[]) noexcept {
        if (argc == NO_ARGS || argc > MAX_PARAM_LENGTH) {
            return;
        }

        std::string mode = std::string(argv[MODE]);
        std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

        if (mode == "bvs" || mode == "4") {
            if (argc < MAX_PARAM_LENGTH) {
                std::cout << "Error: Please specify 'w' for white or 'b' for black to choose engine color.\n";
                exit(EXIT_FAILURE);
            }

            std::string color = argv[COLOR];
            std::transform(color.begin(), color.end(), color.begin(), ::tolower);
            
            if (color == "w") {
                this->botVsStockfish(true);
            } 
            else if (color == "b") {
                this->botVsStockfish(false);
            } 
            else {
                std::cout << "Error: Invalid color option. Use 'w' for white or 'b' for black. \n";
                exit(EXIT_FAILURE);
            }
        }
        else if (mode == "bvb" || mode == "3") {
            this->botVsBot();
        } 
        else if (mode == "pvp" || mode == "21") {
            this->playGameVsHuman();
        } 
        else if (mode == "pvb" || mode == "11") {
            if (argc < MAX_PARAM_LENGTH) {
                std::cout << "Error: Please specify 'w' for white or 'b' for black when playing against the engine.\n";
                exit(EXIT_FAILURE);
            }

            std::string color = argv[COLOR];
            std::transform(color.begin(), color.end(), color.begin(), ::tolower);

            if (color == "w") {
                this->playGameVsEngine(true);
            } 
            else if (color == "b") {
                this->playGameVsEngine(false);
            } 
            else {
                std::cout << "Error: Invalid color option. Use 'w' for white or 'b' for black. \n";
                exit(EXIT_FAILURE);
            }
        } 
        else {
            std::cout << "Error: Invalid mode. Use 'bvb' for bot vs bot, 'pvp' for player vs player, or 'pvb' for player vs bot. \n";
            exit(EXIT_FAILURE);
        }
    }

    bool Driver::loadGame() noexcept{
        std::ifstream SaveFile("saves/save.txt");
        if (!SaveFile.is_open()) {
            std::cerr << "Error: Unable to open save file.\n";
            return false;
        }

        std::string line;
    
        if (std::getline(SaveFile, line)) {
            this->engine.board = chess::Board(line);
        }

        if (std::getline(SaveFile, line)) {
            vsBot = true;

            if (line == "w") {
                this->engine.isPlayerWhite = false;
            } else if (line == "b") {
                this->engine.isPlayerWhite = true;
            }
        }

        // TODO aggiungere controlli/eccezioni per il fen
    
        SaveFile.close();

        // Not working correctly in playervsbot and player is black
        vsBot ? this->playGameVsEngine(this->engine.isPlayerWhite) : this->playGameVsHuman();

        return true;
    }

    void Driver::saveGame() noexcept {
        if (!std::filesystem::exists("saves")) {
            std::filesystem::create_directory("saves");
        }

        if (std::filesystem::exists("saves/save.txt")) {
            char ans;
            
            std::cout << "An existing save file has been detected, do you want to overwrite it? (Y/N) ";
            std::cin >> ans;
            if (ans == 'Y' || ans == 'y') {
                std::filesystem::remove("saves/save.txt");
            }
            else {
                return;
            }   
        }
    
        std::ofstream SaveFile("saves/save.txt");
        SaveFile << engine.board.getCurrentFen();

        // If playing against bot, then saveGame() is called by the player, so it saves the opposite active color to indicate
        // the color of the bot
        if (vsBot) {
            SaveFile << '\n' << (this->engine.board.getActiveColor() == chess::Board::WHITE ? 'b' : 'w');
        }
        
        SaveFile.close();
    }

    void Driver::endGame() noexcept {
        if (this->engine.isCheckMate) {
            uint8_t nextColor = this->engine.board.getActiveColor();
            if (this->engine.board.isCheckmate(nextColor)) {
                std::cout << "\nCheckmate! "
                            << (nextColor == chess::Board::WHITE ? "Black" : "White")
                            << " wins.\n";
            } else if (this->engine.board.isStalemate(nextColor)) {
                std::cout << "\nStalemate. Game drawn.\n";
            }
            std::cout << "Press s to print the game on a file or any other key to return to the menu: ";
            
            std::string input;
            std::cin >> input;
            if (input == "s" || input == "S") {
                printGameOnFile();
            }

            engine.reset();
        } 
    }

    void Driver::printGameOnFile() noexcept {
        if (!std::filesystem::exists("games")) {
            std::filesystem::create_directory("games");
        }
        
        std::string currentTime = std::to_string(std::time(nullptr));
        std::string fileName = "games/game_" + currentTime + ".txt";

        std::ofstream gameFile(fileName);
        gameFile << this->engine.moveHistory;
        gameFile.close();
    }

    void Driver::quit(std::string input) noexcept{
        if(input=="Q"||input=="q"){
            std::cout << "Thank you for playing! See you next time." << std::endl;
            exit(EXIT_SUCCESS);
        }
    } 

    void Driver::playGameVsHuman() noexcept {
    	vsBot = false;
        
        while(!engine.isCheckMate) {
    	    //! It doesn't check for loaded games, we should fix it later based on the activeColor in board
            this->playerTurn();
            if (engine.isCheckMate) { endGame(); return; }

            this->playerTurn();
            if (engine.isCheckMate) { endGame(); return; }
    	}
    }

    void Driver::playGameVsEngine(bool isHumanWhite) noexcept{
        vsBot = true;
        
        if (isHumanWhite) {
            while (!engine.isCheckMate) {
                this->playerTurn();
                if (engine.isCheckMate) { endGame(); return; }
                
                this->engineTurn();
                if (engine.isCheckMate) { endGame(); return; }
            }
        } 
        else {
            while (!engine.isCheckMate) {
                this->engineTurn();
                if (engine.isCheckMate) { endGame(); return; }

                this->playerTurn();
                if (engine.isCheckMate) { endGame(); return; }
            } 
        }
    }

    void Driver::botVsBot() noexcept {
        std::string currentBoard = print::Prints::getBasicBoard(engine.board);
        std::cout << currentBoard << "\n";

        while (!engine.isCheckMate) {
            this->engineTurn();
            if (engine.isCheckMate) { endGame(); return; }
            currentBoard = print::Prints::getBasicBoard(engine.board);
            std::cout << currentBoard << "\n";

            this->engineTurn();
            if (engine.isCheckMate) { endGame(); return; }
            currentBoard = print::Prints::getBasicBoard(engine.board);
            std::cout << currentBoard << "\n";
        }
    }

    void Driver::botVsStockfish(const bool botColor) noexcept {
        // botColor: true = our engine plays White, false = our engine plays Black
        const std::string stockfishPath = "./stockfish/stockfish-ubuntu-x86-64-avx2";
        const int stockfishMoveTimeMs = 200; // tweak if needed
        
        // Fresh game state for each match
        engine.reset();
        engine.isPlayerWhite = botColor;

        struct StockfishProcess {
            FILE* in {nullptr};
            FILE* out {nullptr};
            pid_t pid {-1};

            ~StockfishProcess() {
                if (in) fclose(in);
                if (out) fclose(out);
                if (pid > 0) {
                    kill(pid, SIGTERM);
                    int status = 0;
                    waitpid(pid, &status, 0);
                }
            }

            bool valid() const noexcept { return in && out && pid > 0; }
        };

        auto startStockfish = [&]() -> std::unique_ptr<StockfishProcess> {
            int toChild[2];
            int fromChild[2];
            if (pipe(toChild) != 0 || pipe(fromChild) != 0) {
                std::perror("pipe");
                return nullptr;
            }

            pid_t pid = fork();
            if (pid < 0) {
                std::perror("fork");
                close(toChild[0]); close(toChild[1]);
                close(fromChild[0]); close(fromChild[1]);
                return nullptr;
            }

            if (pid == 0) {
                // Child: connect pipes and exec stockfish
                dup2(toChild[0], STDIN_FILENO);
                dup2(fromChild[1], STDOUT_FILENO);
                close(toChild[0]); close(toChild[1]);
                close(fromChild[0]); close(fromChild[1]);
                execl(stockfishPath.c_str(), stockfishPath.c_str(), (char*)nullptr);
                std::perror("execl");
                _exit(1);
            }

            // Parent
            close(toChild[0]);
            close(fromChild[1]);

            FILE* childIn = fdopen(toChild[1], "w");
            FILE* childOut = fdopen(fromChild[0], "r");
            if (!childIn || !childOut) {
                std::cerr << "Error: fdopen failed for Stockfish pipes.\n";
                if (childIn) fclose(childIn);
                if (childOut) fclose(childOut);
                kill(pid, SIGTERM);
                int status = 0;
                waitpid(pid, &status, 0);
                return nullptr;
            }

            auto proc = std::make_unique<StockfishProcess>();
            proc->in = childIn;
            proc->out = childOut;
            proc->pid = pid;

            // Init UCI and wait readyok
            fputs("uci\n", proc->in);
            fputs("isready\n", proc->in);
            fflush(proc->in);

            std::array<char, 512> buffer{};
            bool ready = false;
            for (int i = 0; i < 400 && fgets(buffer.data(), static_cast<int>(buffer.size()), proc->out); ++i) {
                std::string line(buffer.data());
                if (line.find("readyok") != std::string::npos) {
                    ready = true;
                    break;
                }
            }
            if (!ready) {
                std::cerr << "Stockfish did not report readyok.\n";
                return nullptr;
            }

            fputs("ucinewgame\n", proc->in);
            fflush(proc->in);
            return proc;
        };

        auto runStockfish = [&](StockfishProcess& sf, const std::string& fen) -> std::pair<std::string, std::string> {
            std::array<char, 512> buffer{};
            std::string output;

            std::ostringstream cmd;
            cmd << "position fen " << fen << "\n";
            cmd << "go movetime " << stockfishMoveTimeMs << "\n";

            fputs(cmd.str().c_str(), sf.in);
            fflush(sf.in);

            for (int i = 0; i < 800 && fgets(buffer.data(), static_cast<int>(buffer.size()), sf.out); ++i) {
                output += buffer.data();
                if (std::string(buffer.data()).rfind("bestmove", 0) == 0) {
                    break;
                }
            }

            const std::string token = "bestmove ";
            const auto pos = output.find(token);
            if (pos == std::string::npos) return {"", output};
            const auto end = output.find('\n', pos);
            const std::string line = output.substr(pos + token.size(), end - (pos + token.size()));
            const auto spacePos = line.find(' ');
            return {line.substr(0, spacePos), output};
        };

        auto applyUciMoveToBoard = [&](const std::string& uciMove) {
            if (uciMove.size() < 4) return false;

            const std::string fromStr = uciMove.substr(0, 2);
            const std::string toStr   = uciMove.substr(2, 2);
            const bool hasPromo       = uciMove.size() >= 5;
            const char promo = hasPromo
                ? static_cast<char>(std::tolower(static_cast<unsigned char>(uciMove[4])))
                : '\0';

            chess::Coords fromCoords(fromStr);
            chess::Coords toCoords(toStr);
            if (!chess::Coords::isInBounds(fromCoords) || !chess::Coords::isInBounds(toCoords)) {
                return false;
            }

            return hasPromo
                ? engine.movePiece(fromCoords, toCoords, promo)
                : engine.movePiece(fromCoords, toCoords);
        };

        auto sfProc = startStockfish();
        if (!sfProc) {
            return;
        }

        while (!engine.isCheckMate) {
            const bool engineToMove = (engine.board.getActiveColor() == chess::Board::WHITE) == engine.isPlayerWhite;

            if (engineToMove) {
                // Our engine move
                this->engineTurn();
                std::cout << "Our engine move:\n" << print::Prints::getBasicBoard(engine.board) << "\n";
            } else {
                // Stockfish move
                const std::string fen = engine.board.getCurrentFen();
                const auto [bestMove, sfOutput] = runStockfish(*sfProc, fen);
                if (bestMove.empty()) {
                    std::cerr << "Stockfish did not return a move. Aborting match.\n";
                    return;
                }

                if (!applyUciMoveToBoard(bestMove)) {
                    std::cerr << "Failed to apply Stockfish move: " << bestMove << "\n";
                    return;
                }

                std::cout << "Stockfish plays: " << bestMove << "\n";
                std::cout << print::Prints::getBasicBoard(engine.board) << "\n";
                engine.setIsCheckMate();
            }

            if (engine.isCheckMate) {
                endGame();
                return;
            }
        }
    }


    void Driver::playerTurn() noexcept{
        engine.board.getActiveColor() == chess::Board::WHITE ? std::cout << "\nWhite's turn.\n\n" : std::cout << "\nBlack's turn.\n\n";

        std::string playerInput;

        bool isWhiteTurn = (engine.board.getActiveColor() == chess::Board::WHITE);

        bool error = true;
        while (error) {
            std::string currentBoard = print::Prints::getBasicBoard(engine.board);
            std::cout << currentBoard << "\n";

            std::cout << "Enter your move (write 's' to save the game or 'q' to quit): ";
            std::cin >> playerInput;

            if (playerInput == "s") {
                this->saveGame();
                continue;
            }

            if (playerInput == "q") {
                std::cout << "Thank you for playing! See you next time." << std::endl;
                exit(EXIT_SUCCESS);
            }

            if (playerInput.length() != 4 && playerInput.length() != 5) {
                std::cout << "Invalid move length. Please enter your move in the format 'e2e4' or 'e7e8q'.\n";
                continue;
            }

            chess::Coords fromCoords(playerInput.substr(0, 2));
            chess::Coords toCoords(playerInput.substr(2, 2));
        
            if (!chess::Coords::isInBounds(fromCoords) || !chess::Coords::isInBounds(toCoords)) {
                std::cout << "Invalid move format. Please enter your move in the format 'e2e4'.\n";
                continue;
            }
        
            uint8_t piece = engine.board.get(fromCoords);

            /*
            std::cout << "[DEBUG] fromCoords: " << fromCoords.toString() << " (index=" << (int)fromCoords.index
                      << ", rank=" << (int)fromCoords.rank() << ", file=" << (int)fromCoords.file() << ")\n";
            std::cout << "[DEBUG] piece at from: " << (int)piece << "\n";
            std::cout << "[DEBUG] activeColor: " << (int)engine.board.getActiveColor() << " (WHITE=0, BLACK=8)\n";
            std::cout << "[DEBUG] isWhiteTurn: " << (isWhiteTurn ? "true" : "false") << "\n";
            std::cout << "[DEBUG] getColor(from): " << (int)engine.board.getColor(fromCoords) << "\n";
            */

            if (piece == chess::Board::EMPTY) {
                std::cout << "There is no piece at the source square. Please enter a valid move.\n";
                continue;
            }

            if (isWhiteTurn != (engine.board.getColor(fromCoords) == chess::Board::WHITE)) {
                std::cout << "It's not your turn to move that piece. Please enter a valid move.\n";
                continue;
            }

            // TODO: check whether it's redundant or not
            if (engine.board.isSameColor(fromCoords, toCoords)) {
                std::cout << "You cannot move to a square occupied by your own piece.\n";
                continue;
            }

    #ifdef DEBUG
            auto chrono_start = std::chrono::high_resolution_clock::now();
    #endif  

            const uint8_t pieceType  = piece & chess::Board::MASK_PIECE_TYPE;
            const uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

            bool moveOk = false;

            // Optional promotion character (5th char): e7e8q, e2e1N, ...
            char promoChar = '\0';
            if (playerInput.length() == 5) {
                promoChar = static_cast<char>(std::tolower(static_cast<unsigned char>(playerInput[4])));
                if (promoChar != 'q' && promoChar != 'r' && promoChar != 'b' && promoChar != 'n') {
                    std::cout << "Invalid promotion piece. Use q, r, b or n.\n";
                    continue;
                }
            }

            const bool isPromotionCandidate =
                (pieceType == chess::Board::PAWN) &&
                ((pieceColor == chess::Board::WHITE && toCoords.rank() == 0) ||
                 (pieceColor == chess::Board::BLACK && toCoords.rank() == 7));

            if (isPromotionCandidate && playerInput.length() == 4) {
                // If user didn't specify, default to queen
                promoChar = 'q';
            }

            moveOk = engine.movePiece(fromCoords, toCoords, promoChar);

            if (!moveOk) {
                std::cout << "Invalid move. Please try again.\n";
                continue;
            }

    #ifdef DEBUG
            auto chrono_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> elapsed = chrono_end - chrono_start;
            std::cout << "[DEBUG] MoveBB executed in " << elapsed.count() << " microseconds.\n";
    #endif

            // Print the updated board after successful move
            std::cout << "[DEBUG] About to print updated board...\n";
            std::cout << "\n" << print::Prints::getBasicBoard(engine.board) << "\n";
            std::cout << "[DEBUG] Board printed successfully.\n";

            // TODO To be eliminated because Engine will handle it in future
            engine.setIsCheckMate();

            error = false;
        }  

        return;
    }

    void Driver::engineTurn() noexcept {
        std::cout << "Engine's thinking... \n";
#ifdef DEBUG
                auto chrono_start = std::chrono::high_resolution_clock::now();
#endif
        this->engine.search(this->engine.depth);
#ifdef DEBUG
                auto chrono_end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = chrono_end - chrono_start;
                std::cout << "[DEBUG] Engine search: " << elapsed.count() << "ms.\n";
                std::cout << "[DEBUG] Nodes visited: " << engine.nodesSearched << "\n";
#endif
    }
}