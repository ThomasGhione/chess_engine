#include "driver.hpp"

#include "../engine/engine.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <limits>
#include <memory>
#include <sstream>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include <fstream>
#include <filesystem>

namespace driver {

    Driver::Driver(engine::Engine& e) 
        : engine(e) 
    {}

    bool Driver::parseColorOption(const char* colorArg, bool& outIsWhite) noexcept {
        if (colorArg == nullptr) return false;
        std::string color = colorArg;
        std::transform(color.begin(), color.end(), color.begin(), ::tolower);
        if (color == "w") {
            outIsWhite = true;
            return true;
        }
        if (color == "b") {
            outIsWhite = false;
            return true;
        }
        return false;
    }

    bool Driver::parseRequiredColorArg(int argc, char* argv[], const char* missingArgMessage, bool& outIsWhite) noexcept {
        if (argc < Driver::MAX_PARAM_LENGTH) {
            std::cout << missingArgMessage << "\n";
            return false;
        }

        if (!Driver::parseColorOption(argv[Driver::COLOR], outIsWhite)) {
            std::cout << "Error: Invalid color option. Use 'w' for white or 'b' for black. \n";
            return false;
        }
        return true;
    }

    void Driver::printInvalidOption() noexcept {
        std::cout << "Invalid option. Please select a valid option.\n";
    }

    bool Driver::applyUciMoveToBoard(const std::string& uciMove, bool verboseDebug) noexcept {
        if (verboseDebug) {
            std::cout << "[DEBUG] Applying UCI move: '" << uciMove << "' (length: " << uciMove.size() << ")\n";
            std::cout.flush();
        }

        if (uciMove.size() < 4) {
            if (verboseDebug) {
                std::cerr << "[ERROR] UCI move too short\n";
            }
            return false;
        }

        const std::string fromStr = uciMove.substr(0, 2);
        const std::string toStr   = uciMove.substr(2, 2);
        const bool hasPromo       = uciMove.size() >= 5;
        const char promo = hasPromo
            ? static_cast<char>(std::tolower(static_cast<unsigned char>(uciMove[4])))
            : '\0';

        if (verboseDebug) {
            std::cout << "[DEBUG] From: " << fromStr << ", To: " << toStr
                      << ", HasPromo: " << hasPromo << ", Promo: " << (promo ? promo : '?') << "\n";
            std::cout.flush();
        }

        chess::Coords fromCoords(fromStr);
        chess::Coords toCoords(toStr);
        if (!chess::Coords::isInBounds(fromCoords) || !chess::Coords::isInBounds(toCoords)) {
            if (verboseDebug) {
                std::cerr << "[ERROR] Coordinates out of bounds\n";
            }
            return false;
        }

        if (verboseDebug) {
            std::cout << "[DEBUG] Calling movePiece...\n";
            std::cout.flush();
        }

        const bool result = hasPromo
            ? engine.movePiece(fromCoords, toCoords, promo)
            : engine.movePiece(fromCoords, toCoords);

        if (verboseDebug) {
            std::cout << "[DEBUG] movePiece returned: " << result << "\n";
            std::cout.flush();
        }

        return result;
    }

    void Driver::startGame(int argc, char *argv[]) noexcept {

        parse(argc, argv);

        while (true) {
            this->engine.reset();
            uint8_t mainMenuChoice = mainMenu();

            switch (mainMenuChoice) {
                
                case '1': {
                    uint8_t colorChoice = playWithEngineMenu();
                    this->quit(std::string(1, colorChoice));
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
                            printInvalidOption();
                            break;
                    }
                    break;
                }

                case '2': {
                    this->playGameVsHuman();
                    break;
                }

                case '3':
                    if (!this->loadGame()) {
                        std::cout << "No saved game found. Returning to main menu.\n";
                    }
                    break;

                case '4': {
                    uint8_t extraMenuChoice = extraMenu();
                    this->quit(std::string(1, extraMenuChoice));

                    switch (extraMenuChoice) {
                        case '1':
                            this->botVsBot();
                            break;
                        
                        case '2': {
                            auto uciInterface = uci::UCI();
                            uciInterface.mainLoop();
                            break;
                        }

                        case '3':
                            // Back to main menu
                            break;

                        default:
                            printInvalidOption();
                            break;
                    }
                    
                    break;
                }

                case '5':
                    std::cout << "Thank you for playing! See you next time." << std::endl;
                    exit(EXIT_SUCCESS);
                    break;

                default:
                    printInvalidOption();
                    break;
            }
        } 
    }

    // TODO: update for new modes
    void Driver::parse(int argc, char *argv[]) noexcept {
        if (argc == NO_ARGS || argc > MAX_PARAM_LENGTH) {
            return;
        }

        std::string mode = std::string(argv[MODE]);
        std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

        if (mode == "-bvb" || mode == "41") {
            this->botVsBot();
        } 
        else if (mode == "-pvp" || mode == "21") {
            this->playGameVsHuman();
        } 
        else if (mode == "-pvb" || mode == "11") {
            bool isWhite = false;
            if (!parseRequiredColorArg(argc, argv,
                                       "Error: Please specify 'w' for white or 'b' for black when playing against the engine.",
                                       isWhite)) {
                exit(EXIT_FAILURE);
            }
            this->playGameVsEngine(isWhite);
        } 
        else if (mode == "uci" || mode == "-uci" || mode == "--uci" || mode == "42") {
            auto uciInterface = uci::UCI(this->engine);
            uciInterface.mainLoop();
        }
        else {
            std::cout << "Error: Invalid mode. Use '-bvb' for bot vs bot, '-pvp' for player vs player, or '-pvb' for player vs bot. \n";
            exit(EXIT_FAILURE);
        }
    }

    bool Driver::loadGame() noexcept{
        if (!std::filesystem::exists("saves")) {
            std::filesystem::create_directories("saves");
        }

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

        // TODO: add checks/exceptions for FEN parsing
    
        SaveFile.close();

        // Not working correctly in playervsbot and player is black
        vsBot ? this->playGameVsEngine(true) : this->playGameVsHuman();

        return true;
    }

    void Driver::saveGame() noexcept {
        if (!std::filesystem::exists("saves")) {
            std::filesystem::create_directories("saves");
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
        if (this->engine.isGameOver()) {
            if (this->engine.isMate()) {
                uint8_t nextColor = this->engine.getActiveColor();
                std::cout << "\nCheckmate! "
                            << (nextColor == chess::Board::WHITE ? "Black" : "White")
                            << " wins.\n";
            } else if (this->engine.isStalemate()) {
                std::cout << "\nStalemate. Game drawn.\n";
            }
            std::cout << "Press s to print the game on a file or any other key to return to the menu: ";

            // Clear any pending input, then block for a full line to ensure Windows/Linux parity
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::string line;
            std::getline(std::cin, line);
            if (!line.empty() && (line[0] == 's' || line[0] == 'S')) {
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

    void Driver::quit(const std::string& input) noexcept{
        if(input=="Q"||input=="q"){
            std::cout << "Thank you for playing! See you next time." << std::endl;
            exit(EXIT_SUCCESS);
        }
    } 
}

