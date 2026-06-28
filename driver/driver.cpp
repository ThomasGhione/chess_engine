#include "driver.hpp"

//FIXME Spostare gli include dentro .hpp non qui
#include "../engine/engine.hpp"
#include "../uci/uci.hpp"
#include "../debug.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace driver {

//FIXME Evitare namespace anonimi
namespace {

using chess::Board;
//FIXME Mettere questi parametri dentro classe
constexpr int32_t MAX_PARAM_LENGTH = 3;
constexpr int32_t MODE = 1;
constexpr int32_t COLOR = 2;
constexpr int32_t NO_ARGS = 1;

[[noreturn]] void exitGame() noexcept {
    std::cout << "Thank you for playing! See you next time." << std::endl;
    std::exit(EXIT_SUCCESS);
}

//FIXME Mettere graffe, evitare tutto inline
void ensureDirectory(const char* path) noexcept {
    if (!std::filesystem::exists(path)) std::filesystem::create_directories(path);
}

//FIXME Tramutare da switch a:
//Array dentro la classe.
//Tramite una funzione convertiamo la lettera in indice
//Da indice risaliamo a lettera.
//Evitiamo tutti i brach prediction e possiamo farlo diventare constexpr
char pieceToSymbol(uint8_t piece) noexcept {
    if (piece == Board::EMPTY) return '.';
    const bool isBlack = (piece & Board::MASK_COLOR) == Board::BLACK;

    switch (piece & Board::MASK_PIECE_TYPE) {
        case Board::PAWN:   return isBlack ? 'p' : 'P';
        case Board::KNIGHT: return isBlack ? 'n' : 'N';
        case Board::BISHOP: return isBlack ? 'b' : 'B';
        case Board::ROOK:   return isBlack ? 'r' : 'R';
        case Board::QUEEN:  return isBlack ? 'q' : 'Q';
        case Board::KING:   return isBlack ? 'k' : 'K';
        default:            return '?';
    }
}

//FIXME Nome traviante, se legge non dovrebbe stampare un prompt.
uint8_t readMenuChoice(const char* prompt, uint8_t minChoice, uint8_t maxChoice) noexcept {
    std::cout << prompt;

    uint8_t choice = 0;
    std::cin >> choice;
    while (choice < minChoice || choice > maxChoice) {
        std::cout << "Invalid option. Please select a valid option (" << minChoice << "-" << maxChoice << "): ";
        std::cin >> choice;
    }
    return choice;
}

//FIXME Invece che cambiare il parametro, ritorniamo un valore.
bool parseColorOption(const char* colorArg, bool& outIsWhite) noexcept {
    if (colorArg == nullptr || colorArg[0] == '\0' || colorArg[1] != '\0') return false;

    switch (std::tolower(static_cast<unsigned char>(colorArg[0]))) {
        case 'w': outIsWhite = true;  return true;
        case 'b': outIsWhite = false; return true;
        default:  return false;
    }
}

bool parseRequiredColorArg(int argc, char* argv[], const char* missingArgMessage, bool& outIsWhite) noexcept {
    if (argc < MAX_PARAM_LENGTH) {
        std::cout << missingArgMessage << "\n";
        return false;
    }

    if (!parseColorOption(argv[COLOR], outIsWhite)) {
        std::cout << "Error: Invalid color option. Use 'w' for white or 'b' for black. \n";
        return false;
    }
    return true;
}

} // namespace

Driver::Driver(engine::Engine& e) : engine(e), uciInterface(e) {}

void Driver::printInvalidOption() noexcept { std::cout << "Invalid option. Please select a valid option.\n"; }

[[noreturn]] void Driver::startGame(int argc, char* argv[]) noexcept {
    parse(argc, argv);

    //FIXME Evitare while true
    //FIXME Fare una funzione helper per il corpo del ciclo
    while (true) {
        engine.reset();
        switch (mainMenu()) {
            case '1': {
                switch (playWithEngineMenu()) {
                    case '1': playGameVsEngine(true); break;
                    case '2': playGameVsEngine(false); break;
                    default:  break; // Back to main menu
                }
                break;
            }
            case '2': playGameVsHuman(); break;
            case '3':
                if (!loadGame()) std::cout << "No saved game found. Returning to main menu.\n";
                break;
            case '4': {
                switch (extraMenu()) {
                    case '1': botVsBot(); break;
                    case '2': {
                        uciInterface.mainLoop();
                        break;
                    }
                    default: break; // Back to main menu
                }
                break;
            }
            case '5': exitGame();
            default:  printInvalidOption(); break;
        }
    }
}

// TODO: update for new modes
void Driver::parse(int argc, char* argv[]) noexcept {
    if (argc == NO_ARGS || argc > MAX_PARAM_LENGTH) return;

    std::string mode = argv[MODE];
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    //FIXME Evitare numeri magici
    if (mode == "-bvb" || mode == "41") { botVsBot(); return; }
    if (mode == "-pvp" || mode == "21") { playGameVsHuman(); return; }

    if (mode == "-pvb" || mode == "11") {
        bool isWhite = false;
        //FIXME Condizione non leggibile
        if (!parseRequiredColorArg(
                argc,
                argv,
                "Error: Please specify 'w' for white or 'b' for black when playing against the engine.",
                isWhite)) {
            std::exit(EXIT_FAILURE);
        }
        playGameVsEngine(isWhite);
        return;
    }

    if (mode == "uci" || mode == "-uci" || mode == "--uci" || mode == "42") {
        uciInterface.mainLoop();
        return;
    }

    std::cout << "Error: Invalid mode. Use '-bvb' for bot vs bot, '-pvp' for player vs player, or '-pvb' for player vs bot. \n";
    std::exit(EXIT_FAILURE);
}

bool Driver::loadGame() noexcept {
    ensureDirectory("saves");

    std::ifstream saveFile("saves/save.txt");
    if (!saveFile.is_open()) {
        std::cerr << "Error: Unable to open save file.\n";
        return false;
    }

    std::string line;
    if (std::getline(saveFile, line)) engine.board = chess::Board(line);

    if (std::getline(saveFile, line)) {
        vsBot = true;
        if (line == "w") engine.isPlayerWhite = false;
        else if (line == "b") engine.isPlayerWhite = true;
    }

    // TODO: add checks/exceptions for FEN parsing
    if (vsBot) {
        const bool playerMovesNext =
            (engine.board.getActiveColor() == chess::Board::WHITE) == engine.isPlayerWhite;
        playGameVsEngine(playerMovesNext);
    } else {
        playGameVsHuman();
    }
    return true;
}

void Driver::saveGame() noexcept {
    ensureDirectory("saves");

    if (std::filesystem::exists("saves/save.txt")) {
        std::cout << "An existing save file has been detected, do you want to overwrite it? (Y/N) ";
        char ans = '\0';
        std::cin >> ans;
        if (ans != 'Y' && ans != 'y') return;
        std::filesystem::remove("saves/save.txt");
    }

    std::ofstream saveFile("saves/save.txt");
    if (!saveFile.is_open()) {
        std::cerr << "Error: cannot open 'saves/save.txt' for writing.\n";
        return;
    }
    saveFile << engine.board.fromBoardToFen();

    // If playing against bot, then saveGame() is called by the player, so it saves the opposite active color to indicate
    // the color of the bot
    if (vsBot) saveFile << '\n' << (engine.board.getActiveColor() == chess::Board::WHITE ? 'b' : 'w');

    saveFile.flush();
    if (!saveFile) {
        std::cerr << "Error: failed to write 'saves/save.txt' (state corrupted, partial write).\n";
    }
}

void Driver::endGame() noexcept {
    if (!engine.isGameOver()) return;

    if (engine.isMate()) {
        const uint8_t nextColor = engine.board.getActiveColor();
        std::cout << "\nCheckmate! " << (nextColor == chess::Board::WHITE ? "Black" : "White") << " wins.\n";
    } else if (engine.isStalemate()) {
        std::cout << "\nStalemate. Game drawn.\n";
    } else if (engine.isDraw()) {
        std::cout << "\nDraw.\n";
    }

    std::cout << "Press s to print the game on a file or any other key to return to the menu: ";

    // Clear any pending input, then block for a full line to ensure Windows/Linux parity
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::string line;
    std::getline(std::cin, line);
    if (!line.empty() && (line[0] == 's' || line[0] == 'S')) printGameOnFile();

    engine.reset();
}

void Driver::printGameOnFile() noexcept {
    ensureDirectory("games");
    const std::string fileName = "games/game_" + std::to_string(std::time(nullptr)) + ".txt";
    std::ofstream gameFile(fileName);
    gameFile << engine.moveHistory;
}

bool Driver::playOneTurn(bool playerTurnFlag) noexcept {
    if (playerTurnFlag) playerTurn(); else engineTurn();
    if (!engine.isGameOver()) return false;
    endGame();
    return true;
}

void Driver::playAlternatingTurns(bool firstPlayerTurn, bool secondPlayerTurn, bool printBoard) noexcept {
    const bool turns[2] = {firstPlayerTurn, secondPlayerTurn};
    if (printBoard) std::cout << Driver::getBasicBoard(engine.board) << "\n";

    while (!engine.isGameOver()) {
        for (bool turn : turns) {
            if (playOneTurn(turn)) return;
            if (printBoard) std::cout << Driver::getBasicBoard(engine.board) << "\n";
        }
    }
}

void Driver::playGameVsHuman() noexcept { vsBot = false; playAlternatingTurns(true, true, false); }

void Driver::playGameVsEngine(bool isFirstTurnOfPlayer) noexcept { vsBot = true; playAlternatingTurns(isFirstTurnOfPlayer, !isFirstTurnOfPlayer, false); }

void Driver::botVsBot() noexcept { playAlternatingTurns(false, false, true); }

void Driver::playerTurn() noexcept {
    //FIXME Usare if invece di costrutto ternario inline.
    std::cout << (engine.board.getActiveColor() == chess::Board::WHITE ? "\nWhite's turn.\n\n" : "\nBlack's turn.\n\n");

    std::string playerInput;

    //FIXME Evitare while true
    //FIXME Usare funzioni helper per il copro della funzione, troppo alto
    while (true) {
        std::cout << Driver::getBasicBoard(engine.board) << "\n";
        std::cout << "Enter your move (type 's' to save or 'q' to quit): ";
        std::cin >> playerInput;

        if (playerInput == "s") [[unlikely]] { saveGame(); continue; }
        if (playerInput == "q") [[unlikely]] exitGame();

        // Optional promotion character (5th char): e7e8q, e2e1N, ...
        if (playerInput.length() == 5) {
            const char promo = static_cast<char>(std::tolower(static_cast<unsigned char>(playerInput[4])));
            if (promo != 'q' && promo != 'r' && promo != 'b' && promo != 'n') [[unlikely]] {
                std::cout << "Invalid promotion piece. Use q, r, b or n.\n";
                continue;
            }
        }

        if (playerInput.length() != 4 && playerInput.length() != 5) [[unlikely]] {
            std::cout << "Invalid move length. Please enter your move in the format 'e2e4' or 'e7e8q'.\n";
            continue;
        }

        const chess::Coords fromCoords(playerInput.substr(0, 2));
        const chess::Coords toCoords(playerInput.substr(2, 2));
        if (!chess::Coords::isInBounds(fromCoords) || !chess::Coords::isInBounds(toCoords)) [[unlikely]] {
            std::cout << "Invalid move format. Please enter your move in the format 'e2e4'.\n";
            continue;
        }

        DBG_TIMER_DECLARE(moveTimer);
        DBG_TIMER_START(moveTimer);

        const uint8_t piece = engine.board.get(fromCoords);
        const uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
        const uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

        const bool isPromotionCandidate =
            (pieceType == chess::Board::PAWN) &&
            ((pieceColor == chess::Board::WHITE && toCoords.rank() == 0) ||
             (pieceColor == chess::Board::BLACK && toCoords.rank() == 7));

        if (isPromotionCandidate && playerInput.length() == 4) {
            // If user didn't specify, default to queen
            playerInput += 'q';
        }

        const char movePromotion = (playerInput.length() == 5) ? playerInput[4] : '\0';
        if (!engine.movePiece(fromCoords, toCoords, movePromotion)) {
            std::cout << "Illegal move.\n";
            continue;
        }
        
        DBG_TIMER_US(moveTimer, "move executed");

        std::cout << "\n" << Driver::getBasicBoard(engine.board) << "\n";
        return;
    }
}

void Driver::engineTurn() noexcept {
    std::cout << "Engine's thinking... \n";
    DBG_TIMER_DECLARE(engineSearchTimer);
    DBG_TIMER_START(engineSearchTimer);
    
    engine.search(engine.DEFAULTDEPTH);

    DBG_TIMER_MS(engineSearchTimer, "Engine search");
    DBG_LOG_STREAM("[DEBUG] Nodes visited: " << engine.nodesSearched << "\n");
}

std::string Driver::getBasicBoard(const Board& board) {
    //FIXME Spostare in variabili dentro la classe
    static constexpr char FILES_ROW[] = "  a b c d e f g h\n";
    static constexpr std::size_t FILES_ROW_LEN = sizeof(FILES_ROW) - 1;
    static constexpr std::size_t RANK_ROW_LEN = 21;
    static constexpr std::size_t BOARD_STR_LEN = FILES_ROW_LEN + (8 * RANK_ROW_LEN) + FILES_ROW_LEN;

    //FIXME Creare funzione helper per astrarre queste operazioni con nomi più chiari
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
    const uint8_t choice = readMenuChoice(prompt, minChoice, maxChoice);
    clearScreen();
    return choice;
}

//FIXME Servono questi commenti?
// It can return:
// 1 -> One Player
// 2 -> Two Players
// 3 -> Load Game
// 4 -> Extra modes
// 5 -> Quit Game
uint32_t Driver::mainMenu() noexcept {
    //FIXME Spostare variabile dentro classe
    static constexpr const char* PROMPT =
        "\n\n==================== MAIN MENU ====================\n\n"
        "1. One Player\n"
        "2. Two Players\n"
        "3. Load Game\n"
        "4. Extra Modes\n"
        "5. Quit Game\n\n"
        "Select an option (1-5): ";
    return showMenu(PROMPT, '1', '5');
}

// It can return:
// 1 -> Bot vs Bot (Two instances of this engine)
// 2 -> UCI Mode
// 3 -> Back to Main Menu
uint32_t Driver::extraMenu() noexcept {
    //FIXME Spostare variabile dentro classe
    static constexpr const char* PROMPT =
        "\n\n==================== EXTRA MODES MENU ====================\n\n"
        "1. Bot Vs Bot\n"
        "2. UCI Mode\n"
        "3. Go back\n\n"
        "Select an option (1-3): ";
    return showMenu(PROMPT, '1', '3');
}

// It can return:
// 1 -> Play as White
// 2 -> Play as Black
// 3 -> Back to Main Menu
uint32_t Driver::playWithEngineMenu() noexcept {
    //FIXME Spostare variabile dentro classe
    static constexpr const char* PROMPT =
        "\n\n==================== ONE PLAYER MENU ====================\n\n"
        "1. Play as White\n"
        "2. Play as Black\n"
        "3. Back to Main Menu\n\n"
        "Select an option (1-3): ";
    return showMenu(PROMPT, '1', '3', false);
}

void Driver::clearScreen() noexcept { //! MIGHT NOT BE NOEXCEPT
//FIXME Da aggiungere il controllo sul valore di ritorno
#ifdef _WIN32
    [[maybe_unused]] const int result = std::system("cls");
#else
    [[maybe_unused]] const int result = std::system("clear");
#endif
}

} // namespace driver
