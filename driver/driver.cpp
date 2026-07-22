#include "driver.hpp"

#include "../engine/engine.hpp"
#include "../debug.hpp"

#include <iostream>
#include <limits>

namespace driver {

namespace {

using chess::Board;
constexpr int32_t MAX_PARAM_LENGTH = 3;
constexpr int32_t MODE = 1;
constexpr int32_t COLOR = 2;
constexpr int32_t NO_ARGS = 1;

[[noreturn]] void exitGame() noexcept {
    std::cout << "Thank you for playing! See you next time." << std::endl;
    std::exit(EXIT_SUCCESS);
}

char pieceToSymbol(uint8_t piece) noexcept {
    if (piece == Board::EMPTY) return '.';
    constexpr std::string_view syms = "PpNnBbRrQqKk";
    const int idx = ((piece & Board::MASK_PIECE_TYPE) - 1) * 2 + ((piece & Board::MASK_COLOR) == Board::BLACK);
    return idx < 12 ? syms[idx] : '?';
}

bool parseColorArg(int argc, char* argv[], bool& outIsWhite) noexcept {
    if (argc < MAX_PARAM_LENGTH) {
        std::cout << "Error: Please specify 'w' for white or 'b' for black when playing against the engine.\n";
        return false;
    }
    const char* arg = argv[COLOR];
    if (arg != nullptr && arg[0] != '\0' && arg[1] == '\0') {
        const char c = char(std::tolower(static_cast<unsigned char>(arg[0])));
        if (c == 'w' || c == 'b') { outIsWhite = (c == 'w'); return true; }
    }
    std::cout << "Error: Invalid color option. Use 'w' for white or 'b' for black.\n";
    return false;
}

} // namespace

Driver::Driver(engine::Engine& e) : engine(e), uciInterface(e) {}

[[noreturn]] void Driver::startGame(int argc, char* argv[]) noexcept {
    if (argc != NO_ARGS && argc <= MAX_PARAM_LENGTH) {
        std::string mode = argv[MODE];
        for (char& c : mode) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (mode == "-bvb" || mode == "41") startSession(GameMode::BvB);
        else if (mode == "-pvp" || mode == "21") startSession(GameMode::PvP);
        else if (mode == "-pvb" || mode == "11") {
            bool isWhite = false;
            if (!parseColorArg(argc, argv, isWhite)) std::exit(EXIT_FAILURE);
            startSession(GameMode::PvE, isWhite);
        } else if (mode == "uci" || mode == "-uci" || mode == "--uci" || mode == "42") {
            uciInterface.mainLoop();
        } else {
            std::cout << "Error: Invalid mode. Use '-bvb' for bot vs bot, '-pvp' for player vs player, or '-pvb' for player vs bot.\n";
            std::exit(EXIT_FAILURE);
        }
    }

    while (true) {
        engine.reset();
        static constexpr const char* MAIN_PROMPT =
            "\n\n==================== MAIN MENU ====================\n\n"
            "1. One Player\n"
            "2. Two Players\n"
            "3. Extra Modes\n"
            "4. Quit Game\n\n"
            "Select an option (1-4): ";
        switch (showMenu(MAIN_PROMPT, '1', '4')) {
            case '1': {
                static constexpr const char* PVE_PROMPT =
                    "\n\n==================== ONE PLAYER MENU ====================\n\n"
                    "1. Play as White\n"
                    "2. Play as Black\n"
                    "3. Back to Main Menu\n\n"
                    "Select an option (1-3): ";
                switch (showMenu(PVE_PROMPT, '1', '3', false)) {
                    case '1': startSession(GameMode::PvE, true); break;
                    case '2': startSession(GameMode::PvE, false); break;
                    default:  break;
                }
                break;
            }
            case '2': startSession(GameMode::PvP); break;
            case '3': {
                static constexpr const char* EXTRA_PROMPT =
                    "\n\n==================== EXTRA MODES MENU ====================\n\n"
                    "1. Bot Vs Bot\n"
                    "2. UCI Mode\n"
                    "3. Go back\n\n"
                    "Select an option (1-3): ";
                switch (showMenu(EXTRA_PROMPT, '1', '3')) {
                    case '1': startSession(GameMode::BvB); break;
                    case '2': uciInterface.mainLoop(); break;
                    default:  break;
                }
                break;
            }
            case '4': exitGame();
            default:  std::cout << "Invalid option. Please select a valid option.\n"; break;
        }
    }
}

void Driver::startSession(GameMode mode, bool playerIsWhite) noexcept {
    switch (mode) {
        case GameMode::PvP: playAlternatingTurns(true, true, false); break;
        case GameMode::PvE: playAlternatingTurns(playerIsWhite, !playerIsWhite, false); break;
        case GameMode::BvB: playAlternatingTurns(false, false, true); break;
    }
}

void Driver::playAlternatingTurns(bool firstPlayerTurn, bool secondPlayerTurn, bool printBoard) noexcept {
    if (printBoard) std::cout << getBasicBoard(engine.board) << "\n";

    while (!engine.isGameOver()) {
        for (const bool isPlayerTurn : {firstPlayerTurn, secondPlayerTurn}) {
            if (isPlayerTurn) {
                std::cout << (engine.board.getActiveColor() == chess::Board::WHITE ? "\nWhite's turn.\n\n" : "\nBlack's turn.\n\n");
                std::string playerInput;
                while (true) {
                    std::cout << getBasicBoard(engine.board) << "\n";
                    std::cout << "Enter your move (type 'q' to quit): " << std::flush;
                    std::cin >> playerInput;

                    if (playerInput == "q") [[unlikely]] exitGame();

                    // Optional promotion character (5th char): e7e8q, e2e1N, ...
                    // Normalise to lowercase in place so the move uses the validated form
                    // (the engine's promotion convention is lowercase q/r/b/n).
                    if (playerInput.size() == 5) {
                        playerInput[4] = char(playerInput[4] | 0x20);
                        if (!std::string_view("qrbn").contains(playerInput[4])) [[unlikely]] {
                            std::cout << "Invalid promotion piece. Use q, r, b or n.\n";
                            continue;
                        }
                    }

                    if (playerInput.length() != 4 && playerInput.length() != 5) [[unlikely]] {
                        std::cout << "Invalid move length. Please enter your move in the format 'e2e4' or 'e7e8q'.\n";
                        continue;
                    }

                    const chess::Square fromSquare = chess::parseSquare(playerInput.substr(0, 2));
                    const chess::Square toSquare = chess::parseSquare(playerInput.substr(2, 2));
                    if (!chess::isValidSquare(fromSquare) || !chess::isValidSquare(toSquare)) [[unlikely]] {
                        std::cout << "Invalid move format. Please enter your move in the format 'e2e4'.\n";
                        continue;
                    }

                    DBG_TIMER_DECLARE(moveTimer);
                    DBG_TIMER_START(moveTimer);

                    const uint8_t piece = engine.board.get(fromSquare);
                    const uint8_t toRank = chess::rank(toSquare);
                    const bool isPromo = (piece & chess::Board::MASK_PIECE_TYPE) == chess::Board::PAWN &&
                        ((piece & chess::Board::MASK_COLOR) == chess::Board::WHITE ? toRank == 0 : toRank == 7);
                    if (isPromo && playerInput.size() == 4) playerInput += 'q';

                    const char movePromotion = (playerInput.length() == 5) ? playerInput[4] : '\0';
                    if (!engine.movePiece(fromSquare, toSquare, movePromotion)) {
                        std::cout << "Illegal move.\n";
                        continue;
                    }

                    DBG_TIMER_US(moveTimer, "move executed");
                    std::cout << "\n" << getBasicBoard(engine.board) << "\n";
                    break;
                }
            } else {
                std::cout << "Engine's thinking... \n" << std::flush;
                DBG_TIMER_DECLARE(engineSearchTimer);
                DBG_TIMER_START(engineSearchTimer);
                engine.search(engine.DEFAULTDEPTH);
                DBG_TIMER_MS(engineSearchTimer, "Engine search");
                DBG_LOG_STREAM("[DEBUG] Nodes visited: " << engine.searchRuntime.nodesSearched << "\n");
            }

            if (engine.isGameOver()) {
                if (engine.isMate())
                    std::cout << "\nCheckmate! " << (engine.board.getActiveColor() == chess::Board::WHITE ? "Black" : "White") << " wins.\n";
                else if (engine.isStalemate())
                    std::cout << "\nStalemate. Game drawn.\n";
                else if (engine.isDraw())
                    std::cout << "\nDraw.\n";

                std::cout << "Press any key to return to the menu: " << std::flush;

                // Clear any pending input, then block for a full line to ensure Windows/Linux parity
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::string line;
                std::getline(std::cin, line);

                engine.reset();
                return;
            }
            if (printBoard) std::cout << getBasicBoard(engine.board) << "\n";
        }
    }
}

std::string Driver::getBasicBoard(const Board& board) {
    static constexpr char FILES_ROW[] = "  a b c d e f g h\n";
    static constexpr std::size_t FILES_ROW_LEN = sizeof(FILES_ROW) - 1;
    static constexpr std::size_t RANK_ROW_LEN = 21;
    static constexpr std::size_t BOARD_STR_LEN = FILES_ROW_LEN + (8 * RANK_ROW_LEN) + FILES_ROW_LEN;

    std::string result(BOARD_STR_LEN, '\0');
    char* out = result.data();

    std::memcpy(out, FILES_ROW, FILES_ROW_LEN);
    out += FILES_ROW_LEN;

    for (int row = 7; row >= 0; --row) {
        const char rankChar = static_cast<char>('1' + row);
        *out++ = rankChar;
        *out++ = ' ';
        for (int col = 0; col < 8; ++col) {
            *out++ = pieceToSymbol(board.get(row, col));
            *out++ = ' ';
        }
        *out++ = ' ';
        *out++ = rankChar;
        *out++ = '\n';
    }

    std::memcpy(out, FILES_ROW, FILES_ROW_LEN);
    return result;
}

uint32_t Driver::showMenu(const char* prompt, uint8_t minChoice, uint8_t maxChoice, bool clearBefore) noexcept {
    if (clearBefore) clearScreen();
    std::cout << prompt << std::flush;
    uint8_t choice = 0;
    std::cin >> choice;
    while (choice < minChoice || choice > maxChoice) {
        std::cout << "Invalid option. Please select a valid option (" << minChoice << "-" << maxChoice << "): ";
        std::cin >> choice;
    }
    clearScreen();
    return choice;
}

void Driver::clearScreen() {
#ifdef _WIN32
    [[maybe_unused]] const int result = std::system("cls");
#else
    [[maybe_unused]] const int result = std::system("clear");
#endif
}

} // namespace driver
