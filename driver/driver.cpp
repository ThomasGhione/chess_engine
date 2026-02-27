#include "driver.hpp"

#include "../printer/menu.hpp"
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

    static std::string buildStockfishPositionCommand(const std::string& fen, int moveTimeMs) {
        std::ostringstream cmd;
        cmd << "position fen " << fen << "\n";
        cmd << "go movetime " << moveTimeMs << "\n";
        return cmd.str();
    }

    static std::pair<std::string, std::string> extractBestMoveFromOutput(const std::string& output) {
        const std::string token = "bestmove ";
        const auto pos = output.find(token);
        if (pos == std::string::npos) return {"", output};
        const auto end = output.find('\n', pos);
        const std::string line = output.substr(pos + token.size(), end - (pos + token.size()));
        const auto spacePos = line.find(' ');
        return {line.substr(0, spacePos), output};
    }

    struct StockfishProcess {
#ifdef _WIN32
        HANDLE inputWrite {nullptr};
        HANDLE outputRead {nullptr};
        PROCESS_INFORMATION processInfo{};
#else
        FILE* in {nullptr};
        FILE* out {nullptr};
        pid_t pid {-1};
#endif

        ~StockfishProcess() {
#ifdef _WIN32
            if (inputWrite) CloseHandle(inputWrite);
            if (outputRead) CloseHandle(outputRead);
            if (processInfo.hThread) CloseHandle(processInfo.hThread);
            if (processInfo.hProcess) {
                const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 50);
                if (waitResult == WAIT_TIMEOUT) {
                    TerminateProcess(processInfo.hProcess, 0);
                }
                CloseHandle(processInfo.hProcess);
            }
#else
            if (in) fclose(in);
            if (out) fclose(out);
            if (pid > 0) {
                kill(pid, SIGTERM);
                int status = 0;
                waitpid(pid, &status, 0);
            }
#endif
        }

        bool valid() const noexcept {
#ifdef _WIN32
            return inputWrite && outputRead && processInfo.hProcess;
#else
            return in && out && pid > 0;
#endif
        }
    };

    static bool writeStockfishCommand(StockfishProcess& sf, const std::string& cmd) noexcept {
#ifdef _WIN32
        DWORD written = 0;
        return WriteFile(sf.inputWrite, cmd.c_str(), static_cast<DWORD>(cmd.size()), &written, nullptr) != 0;
#else
        const int rc = fputs(cmd.c_str(), sf.in);
        fflush(sf.in);
        return rc >= 0;
#endif
    }

    static std::string readStockfishOutputUntil(StockfishProcess& sf, const std::string& token, int maxIterations) {
        std::array<char, 512> buffer{};
        std::string output;

#ifdef _WIN32
        for (int i = 0; i < maxIterations; ++i) {
            DWORD available = 0;
            if (!PeekNamedPipe(sf.outputRead, nullptr, 0, nullptr, &available, nullptr)) {
                std::cerr << "[WARNING] PeekNamedPipe failed after " << i << " iterations\n";
                break;
            }

            if (available == 0) {
                Sleep(2);
                continue;
            }

            DWORD bytesRead = 0;
            if (!ReadFile(sf.outputRead, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead, nullptr) || bytesRead == 0) {
                std::cerr << "[WARNING] ReadFile failed after " << i << " iterations\n";
                break;
            }

            buffer[bytesRead] = '\0';
            output.append(buffer.data(), bytesRead);

            if (output.find(token) != std::string::npos) break;
        }
#else
        for (int i = 0; i < maxIterations && fgets(buffer.data(), static_cast<int>(buffer.size()), sf.out); ++i) {
            output += buffer.data();
            if (output.find(token) != std::string::npos) break;
        }
#endif

        if (output.find(token) == std::string::npos) {
            std::cerr << "[WARNING] Token '" << token << "' not found. Output so far: " << output.substr(0, 200) << "\n";
        }

        return output;
    }

    static bool isStockfishRunning(const StockfishProcess& sf) noexcept {
#ifdef _WIN32
        DWORD exitCode = 0;
        if (!GetExitCodeProcess(sf.processInfo.hProcess, &exitCode)) return false;
        return exitCode == STILL_ACTIVE;
#else
        return sf.pid > 0;
#endif
    }

    static std::unique_ptr<StockfishProcess> spawnStockfishProcess(const std::string& stockfishPath) noexcept {
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;

        HANDLE childStdInRead = nullptr;
        HANDLE childStdInWrite = nullptr;
        HANDLE childStdOutRead = nullptr;
        HANDLE childStdOutWrite = nullptr;

        if (!CreatePipe(&childStdOutRead, &childStdOutWrite, &sa, 0)) {
            std::cerr << "CreatePipe (stdout) failed. Error: " << GetLastError() << "\n";
            return nullptr;
        }
        if (!SetHandleInformation(childStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
            std::cerr << "SetHandleInformation (stdout read) failed. Error: " << GetLastError() << "\n";
            CloseHandle(childStdOutRead);
            CloseHandle(childStdOutWrite);
            return nullptr;
        }

        if (!CreatePipe(&childStdInRead, &childStdInWrite, &sa, 0)) {
            std::cerr << "CreatePipe (stdin) failed. Error: " << GetLastError() << "\n";
            CloseHandle(childStdOutRead);
            CloseHandle(childStdOutWrite);
            return nullptr;
        }
        if (!SetHandleInformation(childStdInWrite, HANDLE_FLAG_INHERIT, 0)) {
            std::cerr << "SetHandleInformation (stdin write) failed. Error: " << GetLastError() << "\n";
            CloseHandle(childStdOutRead);
            CloseHandle(childStdOutWrite);
            CloseHandle(childStdInRead);
            CloseHandle(childStdInWrite);
            return nullptr;
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.hStdError = childStdOutWrite;
        si.hStdOutput = childStdOutWrite;
        si.hStdInput = childStdInRead;
        si.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi{};
        std::string cmdLine = stockfishPath;
        if (!CreateProcessA(
                nullptr,
                cmdLine.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &si,
                &pi)) {
            std::cerr << "Failed to launch Stockfish. Error: " << GetLastError() << "\n";
            CloseHandle(childStdOutRead);
            CloseHandle(childStdOutWrite);
            CloseHandle(childStdInRead);
            CloseHandle(childStdInWrite);
            return nullptr;
        }

        CloseHandle(childStdOutWrite);
        CloseHandle(childStdInRead);

        auto proc = std::make_unique<StockfishProcess>();
        proc->inputWrite = childStdInWrite;
        proc->outputRead = childStdOutRead;
        proc->processInfo = pi;
        CloseHandle(proc->processInfo.hThread);
        proc->processInfo.hThread = nullptr;
        return proc;
#else
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
            dup2(toChild[0], STDIN_FILENO);
            dup2(fromChild[1], STDOUT_FILENO);
            close(toChild[0]); close(toChild[1]);
            close(fromChild[0]); close(fromChild[1]);
            execl(stockfishPath.c_str(), stockfishPath.c_str(), (char*)nullptr);
            std::perror("execl");
            _exit(1);
        }

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
        return proc;
#endif
    }

    static std::unique_ptr<StockfishProcess> startStockfishSession(const std::string& stockfishPath) noexcept {
        auto proc = spawnStockfishProcess(stockfishPath);
        if (!proc || !proc->valid()) return nullptr;

        if (!writeStockfishCommand(*proc, "uci\n") || !writeStockfishCommand(*proc, "isready\n")) {
            return nullptr;
        }
        const std::string initOutput = readStockfishOutputUntil(*proc, "readyok", 400);
        if (initOutput.find("readyok") == std::string::npos) {
            std::cerr << "Stockfish did not report readyok.\n";
            return nullptr;
        }
        if (!writeStockfishCommand(*proc, "ucinewgame\n")) {
            return nullptr;
        }
        return proc;
    }

    static std::pair<std::string, std::string> runStockfishRequest(StockfishProcess& sf, const std::string& fen, int moveTimeMs) {
        if (!isStockfishRunning(sf)) {
            return {"", ""};
        }

        const std::string cmd = buildStockfishPositionCommand(fen, moveTimeMs);
        if (!writeStockfishCommand(sf, cmd)) {
            return {"", ""};
        }
        return extractBestMoveFromOutput(readStockfishOutputUntil(sf, "bestmove", 800));
    }

    Driver::Driver(print::Menu& m, engine::Engine& e) 
        : menu(m)
        , engine(e) 
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
            uint8_t mainMenuChoice = menu.mainMenu();

            switch (mainMenuChoice) {
                
                case '1': {
                    uint8_t colorChoice = menu.playWithEngineMenu();
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
                    uint8_t extraMenuChoice = menu.extraMenu();
                    this->quit(std::string(1, extraMenuChoice));

                    switch (extraMenuChoice) {
                        case '1':
                            this->botVsBot();
                            break;

                        case '2': {
                            if (!this->checkAndDownloadStockfish()) break;
                            
                            uint8_t botStockfishChoice = menu.playBotVsStockfishMenu();
                            this->quit(std::string(1, botStockfishChoice));

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
                                    printInvalidOption();
                                    break;
                            }
                            break;
                        }

                        case '3': {
                            uint8_t betaAlphaChoice = menu.playBetaVsAlphaMenu();
                            this->quit(std::string(1, betaAlphaChoice));
                            switch (betaAlphaChoice) {
                                case '1':
                                    this->betaVsAlpha(true);
                                    break;
                                case '2':
                                    this->betaVsAlpha(false);
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
                        
                        case '4': {
                            auto uciInterface = uci::UCI();
                            uciInterface.mainLoop();
                            break;
                        }

                        case '5':
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

        if (mode == "-bvs" || mode == "4") {
            bool isWhite = false;
            if (!parseRequiredColorArg(argc, argv,
                                       "Error: Please specify 'w' for white or 'b' for black to choose engine color.",
                                       isWhite)) {
                exit(EXIT_FAILURE);
            }
            this->botVsStockfish(isWhite);
        }
        else if (mode == "-bvb" || mode == "3") {
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
        else if (mode == "uci" || mode == "-uci" || mode == "--uci") {
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

    void Driver::botVsStockfish(const bool botColor) noexcept {
#ifdef _WIN32
        const std::string stockfishPath = "./stockfish/windows/stockfish-windows-x86-64-avx2.exe";
        constexpr bool verboseApply = true;
#else
        const std::string stockfishPath = "./stockfish/linux/stockfish-ubuntu-x86-64-avx2";
        constexpr bool verboseApply = false;
#endif // _WIN32
        const int stockfishMoveTimeMs = 200;

        engine.reset();
        engine.isPlayerWhite = botColor;

        auto sfProc = startStockfishSession(stockfishPath);
        if (!sfProc) return;

        while (!this->engine.isGameOver()) {
            const bool engineToMove = (engine.board.getActiveColor() == chess::Board::WHITE) == engine.isPlayerWhite;
            if (engineToMove) {
                this->engineTurn();
                std::cout << "Our engine move:\n" << print::Prints::getBasicBoard(engine.board) << "\n";
                std::cout.flush();
            } else {
                std::cout << "Waiting for Stockfish move..." << std::endl;
                const std::string fen = engine.board.getCurrentFen();
                const auto [bestMove, output] = runStockfishRequest(*sfProc, fen, stockfishMoveTimeMs);

                if (bestMove.empty()) {
                    std::cerr << "[ERROR] Stockfish did not return a move. Aborting match.\n";
                    std::cerr << "Current FEN: " << fen << "\n";
                    if (!output.empty()) {
                        std::cerr << "Output: " << output.substr(0, 300) << "\n";
                    }
                    return;
                }

                if (!this->applyUciMoveToBoard(bestMove, verboseApply)) {
                    std::cerr << "[ERROR] Failed to apply Stockfish move: " << bestMove << "\n";
                    return;
                }

                std::cout << "Stockfish plays: " << bestMove << "\n";
                std::cout << print::Prints::getBasicBoard(engine.board) << "\n";
                std::cout.flush();
            }

            if (this->engine.isGameOver()) {
                endGame();
                break;
            }
        }

        writeStockfishCommand(*sfProc, "quit\n");
    }


    void Driver::betaVsAlpha(bool betaIsWhite) noexcept {
        // Note: betaIsWhite parameter is kept for API compatibility but not used
        // The function always runs 10 games (5 as white + 5 as black)
        (void)betaIsWhite; // Suppress unused parameter warning
#ifdef _WIN32
        std::cout << "Beta vs Alpha is only available on Linux." << std::endl;
        return;
#else
        const std::string alphaPath = "./versions/chess_alpha0-0-1";
        
        // Statistics tracking
        struct GameStats {
            int betaWins = 0;
            int alphaWins = 0;
            int draws = 0;
            int betaWinsAsWhite = 0;
            int betaWinsAsBlack = 0;
            int alphaWinsAsWhite = 0;
            int alphaWinsAsBlack = 0;
            int drawsWithBetaWhite = 0;
            int drawsWithBetaBlack = 0;
            int alphaStalls = 0; // Times Alpha got stuck and was killed by watchdog
        } stats;

        struct AlphaProcess {
            FILE* in {nullptr};
            FILE* out {nullptr};
            pid_t pid {-1};

            ~AlphaProcess() {
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

        auto startAlpha = [&]() -> std::unique_ptr<AlphaProcess> {
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
                dup2(toChild[0], STDIN_FILENO);
                dup2(fromChild[1], STDOUT_FILENO);
                close(toChild[0]); close(toChild[1]);
                close(fromChild[0]); close(fromChild[1]);

                // Use stdbuf to force line-buffered output from alpha so we can read moves promptly
                execlp("stdbuf", "stdbuf", "-oL", alphaPath.c_str(), (char*)nullptr);
                std::perror("execlp stdbuf");
                _exit(1);
            }

            close(toChild[0]);
            close(fromChild[1]);

            FILE* childIn = fdopen(toChild[1], "w");
            FILE* childOut = fdopen(fromChild[0], "r");
            if (!childIn || !childOut) {
                std::cerr << "Error: fdopen failed for Alpha pipes.\n";
                if (childIn) fclose(childIn);
                if (childOut) fclose(childOut);
                kill(pid, SIGTERM);
                int status = 0;
                waitpid(pid, &status, 0);
                return nullptr;
            }

            setvbuf(childIn, nullptr, _IONBF, 0);
            setvbuf(childOut, nullptr, _IONBF, 0);

            auto proc = std::make_unique<AlphaProcess>();
            proc->in = childIn;
            proc->out = childOut;
            proc->pid = pid;

            return proc;
        };
        
        auto configureAlpha = [](AlphaProcess& proc, bool alphaIsWhite) {
            // One Player match, then choose Alpha color (opposite of Beta)
            fputs("1\n", proc.in);
            // Empirically, alpha expects '2' when it should play White, '1' when it should play Black.
            const char alphaColorChoice = alphaIsWhite ? '2' : '1';
            fputc(alphaColorChoice, proc.in);
            fputc('\n', proc.in);
            fflush(proc.in);
            
            // Consume any initial output from Alpha (menu text, etc.)
            std::array<char, 512> buffer{};
            for (int i = 0; i < 100; ++i) {
                // Set a short timeout to avoid blocking
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(fileno(proc.out), &readfds);
                struct timeval tv = {0, 10000}; // 10ms timeout
                
                int ret = select(fileno(proc.out) + 1, &readfds, nullptr, nullptr, &tv);
                if (ret <= 0) break; // No more data available
                
                if (!fgets(buffer.data(), static_cast<int>(buffer.size()), proc.out)) break;
            }
        };

        auto sendMoveToAlpha = [](AlphaProcess& proc, const std::string& move) {
            std::cerr << "[DEBUG] Sending to Alpha: " << move << "\n";
            fputs(move.c_str(), proc.in);
            fputc('\n', proc.in);
            fflush(proc.in);
            std::cerr << "[DEBUG] Move sent and flushed\n";
        };

        auto parseAlphaMove = [](const std::string& line) -> std::string {
            const std::string token = "Engine plays:";
            const auto pos = line.find(token);
            if (pos == std::string::npos) return "";

            size_t start = line.find_first_not_of(" \t", pos + token.size());
            if (start == std::string::npos) return "";
            size_t end = start;
            while (end < line.size() && !std::isspace(static_cast<unsigned char>(line[end]))) {
                ++end;
            }
            std::string move = line.substr(start, end - start);
            for (char& c : move) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return move;
        };

        auto readAlphaMove = [&](AlphaProcess& proc, const std::atomic<bool>& keepRunning) -> std::string {
            std::array<char, 512> buffer{};
            int linesRead = 0;
            
            while (keepRunning.load()) {
                // Set 1 second timeout for select
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(fileno(proc.out), &readfds);
                struct timeval tv = {1, 0}; // 1 second
                
                int ret = select(fileno(proc.out) + 1, &readfds, nullptr, nullptr, &tv);
                if (ret < 0) {
                    return std::string{};
                }
                if (ret == 0) {
                    // Timeout, check keepRunning and continue
                    continue;
                }
                
                if (!fgets(buffer.data(), static_cast<int>(buffer.size()), proc.out)) {
                    return std::string{};
                }
                
                const std::string line(buffer.data());
                linesRead++;
                
                const std::string move = parseAlphaMove(line);
                if (!move.empty()) {
                    return move;
                }
            }
            
            return std::string{};
        };

        // Lambda to play a single game and return the result
        // Returns: pair<GameResult, bool wasStalled>
        auto playOneGame = [&](AlphaProcess& proc, bool betaIsWhite) -> std::pair<engine::Engine::GameResult, bool> {
            engine.reset();
            engine.isPlayerWhite = betaIsWhite;
            const bool alphaIsWhite = !betaIsWhite;
            
            configureAlpha(proc, alphaIsWhite);

            // Watchdog timer: 30 seconds per move
            std::atomic<bool> gameRunning{true};
            std::atomic<time_t> lastMoveTime{time(nullptr)};
            const int moveTimeoutSeconds = 30;
            std::atomic<bool> watchdogKilled{false};
            
            // Watchdog thread
            std::thread watchdog([&]() {
                while (gameRunning.load()) {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    
                    const time_t now = time(nullptr);
                    const time_t elapsed = now - lastMoveTime.load();
                    
                    if (elapsed >= moveTimeoutSeconds) {
                        std::cerr << "[Watchdog] Move timeout after " << elapsed << " seconds - KILLING Alpha process\n";
                        gameRunning.store(false);
                        watchdogKilled.store(true);
                        
                        // Force kill Alpha process
                        if (proc.pid > 0) {
                            kill(proc.pid, SIGKILL);
                            std::cerr << "[Watchdog] Sent SIGKILL to Alpha (PID " << proc.pid << ")\n";
                        }
                        return;
                    }
                }
            });

            engine::Engine::GameResult result = engine::Engine::DRAW;
            bool timeoutOccurred = false;

            while (!this->engine.isGameOver() && gameRunning.load()) {
                const bool betaToMove = (engine.board.getActiveColor() == chess::Board::WHITE) == engine.isPlayerWhite;

                // Reset timer for new move
                lastMoveTime.store(time(nullptr));

                if (betaToMove) {
                    const std::size_t previousHistorySize = engine.moveHistory.size();
                    
                    this->engineTurn();
                    
                    if (!gameRunning.load()) {
                        std::cerr << "[Watchdog] Timeout during Beta's turn\n";
                        timeoutOccurred = true;
                        break;
                    }

                    std::string delta = engine.moveHistory.substr(previousHistorySize);
                    while (!delta.empty() && (delta.back() == '\n' || delta.back() == '\r')) {
                        delta.pop_back();
                    }

                    if (delta.size() < 4) {
                        std::cerr << "Beta engine did not produce a move\n";
                        result = engine::Engine::DRAW;
                        break;
                    }

                    sendMoveToAlpha(proc, delta);

                    if (this->engine.isGameOver()) {
                        result = this->engine.getGameResult();
                        break;
                    }
                } else {
                    const std::string alphaMove = readAlphaMove(proc, gameRunning);
                    
                    if (!gameRunning.load()) {
                        std::cerr << "[Watchdog] Timeout during Alpha's turn\n";
                        timeoutOccurred = true;
                        break;
                    }
                    
                    if (alphaMove.empty()) {
                        std::cerr << "[Error] Alpha did not return a move\n";
                        result = engine::Engine::DRAW;
                        break;
                    }

                    if (!this->applyUciMoveToBoard(alphaMove)) {
                        std::cerr << "[Error] Failed to apply Alpha move: " << alphaMove << "\n";
                        result = engine::Engine::DRAW;
                        break;
                    }

                    engine.updateGameResult();
                    if (this->engine.isGameOver()) {
                        result = this->engine.getGameResult();
                        break;
                    }
                }
            }
            
            if (timeoutOccurred || (!gameRunning.load() && result == engine::Engine::ONGOING)) {
                result = engine::Engine::DRAW;
            }
            
            // Stop watchdog
            gameRunning.store(false);
            
            if (watchdog.joinable()) {
                watchdog.join();
            }
            
            return {result, watchdogKilled.load()};
        };

        // Start alpha process
        std::cout << "Starting 50-game match between Beta and Alpha...\n";
        std::cout << "First 25 games: Beta plays White\n";
        std::cout << "Last 25 games: Beta plays Black\n";
        std::cout << "Press any key (except 'S') to start or continue between games...\n\n";

        for (int gameNum = 1; gameNum <= 50; ++gameNum) {
            const bool betaIsWhiteForGame = (gameNum <= 25);
            if (gameNum == 26) {
                std::cout << "\nFirst 25 games completed. Starting second half...\n\n";
            }

            auto alphaProc = startAlpha();
            if (!alphaProc || !alphaProc->valid()) {
                std::cerr << "Failed to start alpha engine for game " << gameNum << ", counting as draw\n";
                stats.draws++;
                if (betaIsWhiteForGame) stats.drawsWithBetaWhite++;
                else stats.drawsWithBetaBlack++;
                std::cout << "Game " << gameNum << "/50 (Beta=" << (betaIsWhiteForGame ? "White" : "Black")
                          << ")... Draw (Alpha failed to start)\n";
                continue;
            }

            std::cout << "Game " << gameNum << "/50 (Beta=" << (betaIsWhiteForGame ? "White" : "Black") << ")... ";
            std::cout.flush();

            const auto [result, wasStalled] = playOneGame(*alphaProc, betaIsWhiteForGame);
            if (wasStalled) stats.alphaStalls++;

            if ((betaIsWhiteForGame && result == engine::Engine::WHITE_WINS) ||
                (!betaIsWhiteForGame && result == engine::Engine::BLACK_WINS)) {
                stats.betaWins++;
                if (betaIsWhiteForGame) stats.betaWinsAsWhite++;
                else stats.betaWinsAsBlack++;
                std::cout << "Beta wins\n";
            } else if (result == engine::Engine::DRAW) {
                stats.draws++;
                if (betaIsWhiteForGame) stats.drawsWithBetaWhite++;
                else stats.drawsWithBetaBlack++;
                std::cout << "Draw\n";
            } else {
                stats.alphaWins++;
                if (betaIsWhiteForGame) stats.alphaWinsAsBlack++;
                else stats.alphaWinsAsWhite++;
                std::cout << "Alpha wins\n";
            }
        }

        // Save results to CSV file
        const std::string resultsFile = "beta_vs_alpha_results.csv";
        std::ofstream outFile(resultsFile);
        if (outFile.is_open()) {
            outFile << "Category,Value\n";
            outFile << "Total Games,50\n";
            outFile << "\n";
            outFile << "Beta Total Wins," << stats.betaWins << "\n";
            outFile << "Alpha Total Wins," << stats.alphaWins << "\n";
            outFile << "Total Draws," << stats.draws << "\n";
            outFile << "\n";
            outFile << "Beta Wins as White," << stats.betaWinsAsWhite << "\n";
            outFile << "Beta Wins as Black," << stats.betaWinsAsBlack << "\n";
            outFile << "Alpha Wins as White," << stats.alphaWinsAsWhite << "\n";
            outFile << "Alpha Wins as Black," << stats.alphaWinsAsBlack << "\n";
            outFile << "Draws (Beta=White)," << stats.drawsWithBetaWhite << "\n";
            outFile << "Draws (Beta=Black)," << stats.drawsWithBetaBlack << "\n";
            outFile << "\n";
            outFile << "Alpha Stalls (Timeouts)," << stats.alphaStalls << "\n";
            outFile << "\n";
            outFile << "Beta Win Rate," << (stats.betaWins * 100.0 / 50.0) << "%\n";
            outFile << "Alpha Win Rate," << (stats.alphaWins * 100.0 / 50.0) << "%\n";
            outFile << "Draw Rate," << (stats.draws * 100.0 / 50.0) << "%\n";
            outFile.close();
            
            std::cout << "\n=== MATCH RESULTS ===\n";
            std::cout << "Results saved to: " << resultsFile << "\n\n";
            std::cout << "Total Games: 50\n";
            std::cout << "\nOverall Results:\n";
            std::cout << "  Beta Wins:  " << stats.betaWins << " (" << (stats.betaWins * 100.0 / 10.0) << "%)\n";
            std::cout << "  Alpha Wins: " << stats.alphaWins << " (" << (stats.alphaWins * 100.0 / 10.0) << "%)\n";
            std::cout << "  Draws:      " << stats.draws << " (" << (stats.draws * 100.0 / 10.0) << "%)\n";
            std::cout << "\nDetailed Breakdown:\n";
            std::cout << "  Beta as White: " << stats.betaWinsAsWhite << " wins, " 
                      << stats.drawsWithBetaWhite << " draws\n";
            std::cout << "  Beta as Black: " << stats.betaWinsAsBlack << " wins, " 
                      << stats.drawsWithBetaBlack << " draws\n";
            std::cout << "  Alpha as White: " << stats.alphaWinsAsWhite << " wins\n";
            std::cout << "  Alpha as Black: " << stats.alphaWinsAsBlack << " wins\n";
            std::cout << "\nAlpha Stalls (Timeouts): " << stats.alphaStalls << "\n";
            std::cout << "=====================\n";
        } else {
            std::cerr << "Failed to open file for writing results." << std::endl;
        }

#endif // _WIN32
    }
}
