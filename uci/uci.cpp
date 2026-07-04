#include "uci.hpp"

#include "../engine/engine.hpp"

#include <omp.h>

#include <charconv>
#include <iostream>

namespace {
    using std::string_view;

    static string_view trimLeft(string_view s) noexcept {
        size_t i = 0;
        while (i < s.size() && std::isspace(s[i])) ++i;
        return s.substr(i);
    }

    static string_view trimRight(string_view s) noexcept {
        while (!s.empty() && std::isspace(s.back())) s.remove_suffix(1);
        return s;
    }

    static std::string normalizedOptionName(string_view optionName) {
        std::string normalized;
        normalized.reserve(optionName.size());
        for (const char c : optionName) {
            if (c == ' ' || c == '_' || c == '-') continue;
            normalized.push_back(static_cast<char>(std::tolower(c)));
        }
        return normalized;
    }

    static std::string optionDisplayName(std::string_view rawName) {
        std::string display;
        display.reserve(rawName.size());
        bool upperNext = true;
        for (const char c : rawName) {
            if (c == '_' || c == '-' || c == ' ') {
                upperNext = true;
                continue;
            }
            display.push_back(upperNext ? static_cast<char>(std::toupper(c))
                                        : static_cast<char>(std::tolower(c)));
            upperNext = false;
        }
        return display;
    }

    struct EvalOption final {
        const char* key;
        int32_t* value;
        bool refreshPieceTables;
        bool hasRange;
        int32_t minValue;
        int32_t maxValue;
    };

    // Wide auto-range so the tuner's sampled values land inside the engine's
    // accepted bounds without each option needing an explicit hasRange entry.
    // ±200% of magnitude with a floor of 10 covers the typical
    // chess-tuning-tools parameter ranges used by the JSON groups.
    static std::pair<int32_t, int32_t> defaultRangeFor(int32_t value) noexcept {
        const int64_t absValue = std::llabs(static_cast<int64_t>(value));
        const int64_t delta = std::max<int64_t>(10, absValue * 2);
        const auto toI32 = [](int64_t x) {
            return static_cast<int32_t>(std::clamp<int64_t>(x, INT32_MIN, INT32_MAX));
        };
        return {toI32(value - delta), toI32(value + delta)};
    }

    // Resolved [min, max] range advertised and accepted for an eval option:
    // its explicit bounds when given, otherwise the auto-range.
    static std::pair<int32_t, int32_t> optionRange(const EvalOption& option) noexcept {
        return option.hasRange ? std::pair{option.minValue, option.maxValue}
                               : defaultRangeFor(*option.value);
    }

    static void refreshPieceTables() noexcept {
        // One row per piece slot (0=empty, 1=pawn … 6=king, 7=empty).
        // King has no phase-split value, so mg/eg both use the base.
        struct Slot { int32_t base, mg, eg; };
        const Slot slots[] = {
            {0,                    0,                       0                      },
            {engine::PAWN_VALUE,   engine::PAWN_VALUE_MG,   engine::PAWN_VALUE_EG  },
            {engine::KNIGHT_VALUE, engine::KNIGHT_VALUE_MG, engine::KNIGHT_VALUE_EG},
            {engine::BISHOP_VALUE, engine::BISHOP_VALUE_MG, engine::BISHOP_VALUE_EG},
            {engine::ROOK_VALUE,   engine::ROOK_VALUE_MG,   engine::ROOK_VALUE_EG  },
            {engine::QUEEN_VALUE,  engine::QUEEN_VALUE_MG,  engine::QUEEN_VALUE_EG },
            {engine::KING_VALUE,   engine::KING_VALUE,      engine::KING_VALUE     },
            {0,                    0,                       0                      },
        };
        for (int i = 0; i < 8; ++i) {
            engine::PIECE_VALUES[i] = slots[i].base;
            chess::Board::MATERIAL_VALUES[i]    = slots[i].base;
            chess::Board::MATERIAL_VALUES_MG[i] = slots[i].mg;
            chess::Board::MATERIAL_VALUES_EG[i] = slots[i].eg;
        }
        for (int i = 0; i < 7; ++i)
            engine::MVV_TABLE[i] = slots[i].base * 10;
    }

    // Macro to expand a PhaseValue constant into Mg + Eg UCI options. Tuners
    // can drive each side independently to fit the smooth phase curve.
    #define PV_OPTS(NAME, REF) \
        {NAME "_Mg", &(REF).mg, false, false, 0, 0}, \
        {NAME "_Eg", &(REF).eg, false, false, 0, 0}

    static EvalOption kEvalOptions[] = {
        {"PAWN_VALUE_Mg",   &engine::PAWN_VALUE_MG,   true, false, 0, 0},
        {"PAWN_VALUE_Eg",   &engine::PAWN_VALUE_EG,   true, false, 0, 0},
        {"KNIGHT_VALUE_Mg", &engine::KNIGHT_VALUE_MG, true, false, 0, 0},
        {"KNIGHT_VALUE_Eg", &engine::KNIGHT_VALUE_EG, true, false, 0, 0},
        {"BISHOP_VALUE_Mg", &engine::BISHOP_VALUE_MG, true, false, 0, 0},
        {"BISHOP_VALUE_Eg", &engine::BISHOP_VALUE_EG, true, false, 0, 0},
        {"ROOK_VALUE_Mg",   &engine::ROOK_VALUE_MG,   true, false, 0, 0},
        {"ROOK_VALUE_Eg",   &engine::ROOK_VALUE_EG,   true, false, 0, 0},
        {"QUEEN_VALUE_Mg",  &engine::QUEEN_VALUE_MG,  true, false, 0, 0},
        {"QUEEN_VALUE_Eg",  &engine::QUEEN_VALUE_EG,  true, false, 0, 0},
        {"KING_VALUE", &engine::KING_VALUE, true, false, 0, 0},
        {"MATE_SCORE", &engine::MATE_SCORE, false, true, 0, 2'147'483'647},
        PV_OPTS("DOUBLED_PAWN_PENALTY", engine::DOUBLED_PAWN_PENALTY),
        PV_OPTS("ISOLATED_PAWN_PENALTY", engine::ISOLATED_PAWN_PENALTY),
        PV_OPTS("PASSED_PAWN_BONUS", engine::PASSED_PAWN_BONUS),
        PV_OPTS("PAWN_ISLAND_PENALTY", engine::PAWN_ISLAND_PENALTY),
        PV_OPTS("PAWN_SUPPORT_BONUS", engine::PAWN_SUPPORT_BONUS),
        PV_OPTS("CANDIDATE_PASSER_BONUS", engine::CANDIDATE_PASSER_BONUS),
        PV_OPTS("CONNECTED_PASSER_BONUS", engine::CONNECTED_PASSER_BONUS),
        PV_OPTS("BACKWARD_PAWN_PENALTY", engine::BACKWARD_PAWN_PENALTY),
        PV_OPTS("PASSED_PAWN_BLOCKED_PENALTY", engine::PASSED_PAWN_BLOCKED_PENALTY),
        PV_OPTS("CENTER_CONTROL_BONUS", engine::CENTER_CONTROL_BONUS),
        {"COLOR_COMPLEX_PENALTY", &engine::COLOR_COMPLEX_PENALTY, false, false, 0, 0},
        PV_OPTS("PASSED_ADVANCEMENT_SCALE", engine::PASSED_ADVANCEMENT_SCALE),
        PV_OPTS("PASSED_NEAR_PROMOTION_BONUS", engine::PASSED_NEAR_PROMOTION_BONUS),
        PV_OPTS("BISHOP_PAIR_BONUS", engine::BISHOP_PAIR_BONUS),
        PV_OPTS("KING_NON_CASTLING_PENALTY", engine::KING_NON_CASTLING_PENALTY),
        PV_OPTS("KING_LOST_CASTLING_RIGHTS_PENALTY", engine::KING_LOST_CASTLING_RIGHTS_PENALTY),
        PV_OPTS("LOSS_OF_CASTLING_PENALTY", engine::LOSS_OF_CASTLING_PENALTY),
        PV_OPTS("INIT_BONUS", engine::INIT_BONUS),
        PV_OPTS("DEVELOPMENT_BONUS", engine::DEVELOPMENT_BONUS),
        PV_OPTS("LOW_MOBILITY_KNIGHT_PENALTY", engine::LOW_MOBILITY_KNIGHT_PENALTY),
        PV_OPTS("PINNED_KNIGHT_PENALTY", engine::PINNED_KNIGHT_PENALTY),
        PV_OPTS("LOW_MOBILITY_BISHOP_PENALTY", engine::LOW_MOBILITY_BISHOP_PENALTY),
        PV_OPTS("PINNED_BISHOP_PENALTY", engine::PINNED_BISHOP_PENALTY),
        PV_OPTS("LOW_MOBILITY_ROOK_PENALTY", engine::LOW_MOBILITY_ROOK_PENALTY),
        PV_OPTS("PINNED_ROOK_PENALTY", engine::PINNED_ROOK_PENALTY),
        PV_OPTS("LOW_MOBILITY_QUEEN_PENALTY", engine::LOW_MOBILITY_QUEEN_PENALTY),
        PV_OPTS("PINNED_QUEEN_PENALTY", engine::PINNED_QUEEN_PENALTY),
        // Per-piece safe-mobility refs (scalar) and weights (PhaseValue). Explicit
        // ranges so the tuner has room (defaultRangeFor would clamp small defaults).
        {"MOBILITY_KNIGHT_REF", &engine::MOBILITY_KNIGHT_REF, false, true, 0, 16},
        {"MOBILITY_BISHOP_REF", &engine::MOBILITY_BISHOP_REF, false, true, 0, 16},
        {"MOBILITY_ROOK_REF",   &engine::MOBILITY_ROOK_REF,   false, true, 0, 16},
        {"MOBILITY_QUEEN_REF",  &engine::MOBILITY_QUEEN_REF,  false, true, 0, 20},
        {"MOBILITY_KNIGHT_WEIGHT_Mg", &engine::MOBILITY_KNIGHT_WEIGHT.mg, false, true, -4, 20},
        {"MOBILITY_KNIGHT_WEIGHT_Eg", &engine::MOBILITY_KNIGHT_WEIGHT.eg, false, true, -4, 20},
        {"MOBILITY_BISHOP_WEIGHT_Mg", &engine::MOBILITY_BISHOP_WEIGHT.mg, false, true, -4, 20},
        {"MOBILITY_BISHOP_WEIGHT_Eg", &engine::MOBILITY_BISHOP_WEIGHT.eg, false, true, -4, 20},
        {"MOBILITY_ROOK_WEIGHT_Mg",   &engine::MOBILITY_ROOK_WEIGHT.mg,   false, true, -4, 20},
        {"MOBILITY_ROOK_WEIGHT_Eg",   &engine::MOBILITY_ROOK_WEIGHT.eg,   false, true, -4, 20},
        {"MOBILITY_QUEEN_WEIGHT_Mg",  &engine::MOBILITY_QUEEN_WEIGHT.mg,  false, true, -4, 20},
        {"MOBILITY_QUEEN_WEIGHT_Eg",  &engine::MOBILITY_QUEEN_WEIGHT.eg,  false, true, -4, 20},
        PV_OPTS("COORDINATION_PENALTY", engine::COORDINATION_PENALTY),
        PV_OPTS("OUTPOST_BISHOP_BONUS", engine::OUTPOST_BISHOP_BONUS),
        PV_OPTS("OUTPOST_KNIGHT_BONUS", engine::OUTPOST_KNIGHT_BONUS),
        PV_OPTS("HANGING_PAWN_PENALTY", engine::HANGING_PAWN_PENALTY),
        PV_OPTS("HANGING_PAWN_NEAR_KING_PENALTY", engine::HANGING_PAWN_NEAR_KING_PENALTY),
        PV_OPTS("HANGING_HOOK_PAWN_PENALTY", engine::HANGING_HOOK_PAWN_PENALTY),
        PV_OPTS("HANGING_MINOR_PENALTY", engine::HANGING_MINOR_PENALTY),
        PV_OPTS("HANGING_ROOK_PENALTY", engine::HANGING_ROOK_PENALTY),
        PV_OPTS("HANGING_QUEEN_PENALTY", engine::HANGING_QUEEN_PENALTY),
        PV_OPTS("THREAT_PAWN_ATTACK_MINOR_PENALTY", engine::THREAT_PAWN_ATTACK_MINOR_PENALTY),
        PV_OPTS("THREAT_PAWN_ATTACK_ROOK_PENALTY", engine::THREAT_PAWN_ATTACK_ROOK_PENALTY),
        PV_OPTS("THREAT_PAWN_ATTACK_QUEEN_PENALTY", engine::THREAT_PAWN_ATTACK_QUEEN_PENALTY),
        PV_OPTS("THREAT_MINOR_ATTACK_ROOK_PENALTY", engine::THREAT_MINOR_ATTACK_ROOK_PENALTY),
        PV_OPTS("THREAT_MINOR_ATTACK_QUEEN_PENALTY", engine::THREAT_MINOR_ATTACK_QUEEN_PENALTY),
        PV_OPTS("THREAT_ROOK_ATTACK_QUEEN_PENALTY", engine::THREAT_ROOK_ATTACK_QUEEN_PENALTY),
        PV_OPTS("THREAT_PAWN_PUSH_MINOR_PENALTY", engine::THREAT_PAWN_PUSH_MINOR_PENALTY),
        PV_OPTS("THREAT_PAWN_PUSH_ROOK_PENALTY", engine::THREAT_PAWN_PUSH_ROOK_PENALTY),
        PV_OPTS("THREAT_PAWN_PUSH_QUEEN_PENALTY", engine::THREAT_PAWN_PUSH_QUEEN_PENALTY),
        PV_OPTS("THREAT_LOOSE_MINOR_PENALTY", engine::THREAT_LOOSE_MINOR_PENALTY),
        PV_OPTS("THREAT_LOOSE_ROOK_PENALTY", engine::THREAT_LOOSE_ROOK_PENALTY),
        PV_OPTS("THREAT_LOOSE_QUEEN_PENALTY", engine::THREAT_LOOSE_QUEEN_PENALTY),
        PV_OPTS("PAWN_FORK_BASE_BONUS", engine::PAWN_FORK_BASE_BONUS),
        PV_OPTS("PAWN_FORK_MAJOR_BONUS", engine::PAWN_FORK_MAJOR_BONUS),
        PV_OPTS("PAWN_FORK_ROYAL_BONUS", engine::PAWN_FORK_ROYAL_BONUS),
        PV_OPTS("OPEN_FILE_ROOK_BONUS", engine::OPEN_FILE_ROOK_BONUS),
        PV_OPTS("SEMI_OPEN_FILE_ROOK_BONUS", engine::SEMI_OPEN_FILE_ROOK_BONUS),
        PV_OPTS("ROOK_ON_SEVENTH_BONUS", engine::ROOK_ON_SEVENTH_BONUS),
        PV_OPTS("ROOK_BEHIND_OWN_PASSER_BONUS", engine::ROOK_BEHIND_OWN_PASSER_BONUS),
        PV_OPTS("ROOK_BEHIND_ENEMY_PASSER_BONUS", engine::ROOK_BEHIND_ENEMY_PASSER_BONUS),
        PV_OPTS("ROOK_EG_EDGE_BONUS", engine::ROOK_EG_EDGE_BONUS),
        PV_OPTS("ROOK_EG_PRESSURE_BONUS", engine::ROOK_EG_PRESSURE_BONUS),
        PV_OPTS("KING_SAFETY_PENALTY", engine::KING_SAFETY_PENALTY),
        PV_OPTS("KING_ACTIVITY_BONUS", engine::KING_ACTIVITY_BONUS),
        PV_OPTS("CASTLE_PAWN_SUPPORT_BONUS", engine::CASTLE_PAWN_SUPPORT_BONUS),
        PV_OPTS("KING_SHELTER_STRONG_BONUS", engine::KING_SHELTER_STRONG_BONUS),
        PV_OPTS("KING_SHELTER_WEAK_BONUS", engine::KING_SHELTER_WEAK_BONUS),
        PV_OPTS("KING_SHELTER_MISSING_PENALTY", engine::KING_SHELTER_MISSING_PENALTY),
        PV_OPTS("KING_PAWN_STORM_NEAR_PENALTY", engine::KING_PAWN_STORM_NEAR_PENALTY),
        PV_OPTS("KING_PAWN_STORM_FAR_PENALTY", engine::KING_PAWN_STORM_FAR_PENALTY),
        PV_OPTS("KING_SHELTER_ADVANCE_ONE_PENALTY", engine::KING_SHELTER_ADVANCE_ONE_PENALTY),
        PV_OPTS("KING_SHELTER_ADVANCE_TWO_PENALTY", engine::KING_SHELTER_ADVANCE_TWO_PENALTY),
        PV_OPTS("KING_HOOK_PAWN_ATTACKED_PENALTY", engine::KING_HOOK_PAWN_ATTACKED_PENALTY),
        PV_OPTS("KING_HOOK_PAWN_HANGING_PENALTY", engine::KING_HOOK_PAWN_HANGING_PENALTY),
        PV_OPTS("KING_SEMI_OPEN_FILE_PENALTY", engine::KING_SEMI_OPEN_FILE_PENALTY),
        PV_OPTS("KING_OPEN_FILE_PENALTY", engine::KING_OPEN_FILE_PENALTY),
        PV_OPTS("KING_FILE_PRESSURE_PENALTY", engine::KING_FILE_PRESSURE_PENALTY),
        PV_OPTS("KING_OPEN_DIAGONAL_PENALTY", engine::KING_OPEN_DIAGONAL_PENALTY),
        {"KING_SAFETY_SIDE_CAP", &engine::KING_SAFETY_SIDE_CAP, false, false, 0, 0},
        {"KING_ATTACK_MATERIAL_MIN_SCALE", &engine::KING_ATTACK_MATERIAL_MIN_SCALE, false, false, 0, 0},
        {"KING_ATTACK_MATERIAL_MAX_SCALE", &engine::KING_ATTACK_MATERIAL_MAX_SCALE, false, false, 0, 0},
        {"KING_ATTACK_WEIGHT_KNIGHT", &engine::KING_ATTACK_WEIGHT_KNIGHT, false, false, 0, 0},
        {"KING_ATTACK_WEIGHT_BISHOP", &engine::KING_ATTACK_WEIGHT_BISHOP, false, false, 0, 0},
        {"KING_ATTACK_WEIGHT_ROOK", &engine::KING_ATTACK_WEIGHT_ROOK, false, false, 0, 0},
        {"KING_ATTACK_WEIGHT_QUEEN", &engine::KING_ATTACK_WEIGHT_QUEEN, false, false, 0, 0},
        {"KING_SAFE_CONTACT_BONUS", &engine::KING_SAFE_CONTACT_BONUS, false, false, 0, 0},
        {"KING_FORCING_CONTACT_BONUS", &engine::KING_FORCING_CONTACT_BONUS, false, false, 0, 0},
        {"KING_SAFE_CHECK_BONUS", &engine::KING_SAFE_CHECK_BONUS, false, false, 0, 0},
        {"KING_FORCING_CHECK_BONUS", &engine::KING_FORCING_CHECK_BONUS, false, false, 0, 0},
        {"KING_ATTACK_DANGER_CAP", &engine::KING_ATTACK_DANGER_CAP, false, false, 0, 0},
        PV_OPTS("SPACE_BONUS", engine::SPACE_BONUS),
        {"STALEMATE_DRAW_PENALTY_MAJOR", &engine::STALEMATE_DRAW_PENALTY_MAJOR, false, false, 0, 0},
        {"STALEMATE_DRAW_PENALTY_MINOR", &engine::STALEMATE_DRAW_PENALTY_MINOR, false, false, 0, 0},
        {"STALEMATE_MATERIAL_THRESHOLD", &engine::STALEMATE_MATERIAL_THRESHOLD, false, false, 0, 0},
    };
    #undef PV_OPTS

    static bool parseCheckValue(string_view rawValue, bool& outValue) noexcept {
        const string_view value = trimLeft(rawValue);
        if (ascii::iequals(value, "true") || value == "1" || ascii::iequals(value, "on")) {
            outValue = true;
            return true;
        }
        if (ascii::iequals(value, "false") || value == "0" || ascii::iequals(value, "off")) {
            outValue = false;
            return true;
        }
        return false;
    }

    static string_view nextToken(string_view& text) noexcept {
        text = trimLeft(text);
        if (text.empty()) return {};
        size_t end = 0;
        while (end < text.size() && !std::isspace(text[end])) ++end;
        const string_view token = text.substr(0, end);
        text.remove_prefix(end);
        return token;
    }

    // Parses a whole signed-integer token (optional leading '+'); the entire
    // token must be consumed. Works for any integer type via std::from_chars.
    template<typename T>
    static bool parseInt(string_view token, T& out) noexcept {
        if (token.starts_with('+')) token.remove_prefix(1);
        if (token.empty()) return false;
        const auto [ptr, err] = std::from_chars(token.data(), token.data() + token.size(), out);
        return err == std::errc{} && ptr == token.data() + token.size();
    }

    // Parses the first whitespace-delimited token of `text` as an integer.
    template<typename T>
    static bool parseFirstInt(string_view text, T& out) noexcept {
        return parseInt(nextToken(text), out);
    }

    static bool splitCommand(string_view command, string_view name, string_view& args) noexcept {
        if (!command.starts_with(name)) return false;
        if (command.size() == name.size()) {
            args = {};
            return true;
        }
        if (!std::isspace(command[name.size()])) return false;
        args = trimLeft(command.substr(name.size()));
        return true;
    }

    static size_t findWord(string_view text, string_view word) noexcept {
        size_t pos = text.find(word);
        while (pos != string_view::npos) {
            const size_t end = pos + word.size();
            const bool leftOk = (pos == 0) || std::isspace(text[pos - 1]);
            const bool rightOk = (end == text.size()) || std::isspace(text[end]);
            if (leftOk && rightOk) return pos;
            pos = text.find(word, pos + 1);
        }
        return string_view::npos;
    }

    static std::string ponderSuffix(const engine::Engine& engine, const chess::Move& bestMove) noexcept {
        chess::Board board = engine.board;
        if (!board.move(bestMove)) return {};

        const chess::Move ponderMove = engine.tt.probeDecodedMove(board.getHash());
        if (!chess::isValidSquare(ponderMove.from)) return {};

        return board.move(ponderMove)
            ? (" ponder " + ponderMove.toUCIString())
            : std::string{};
    }

}

namespace uci {
    UCI::UCI(engine::Engine& e) : engine(e) {}

    UCI::~UCI() noexcept {
        stopSearch(false);
    }

    void UCI::emitBestMove(std::string_view move) noexcept {
        std::cout << "bestmove " << move << '\n';
        std::cout.flush();
        searchPrinted = true;
    }

    void UCI::stopSearch(bool printBestMove) noexcept {
        engine.stopThinking();
        if (searchThread.joinable()) searchThread.join();

        std::lock_guard<std::mutex> lock(searchMutex);
        if (printBestMove && !searchPrinted) emitBestMove(searchBestMove);
    }

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
        if (command == "quit") {
            stopSearch(true);
            std::exit(EXIT_SUCCESS);
        }
        if (command == "uci") return uci();
        if (command == "ucinewgame") return ucinewgame();
        if (command == "isready") return isready();
        if (command == "stop") return stopSearch(true);
        if (command == "ponderhit") return ponderhit();
        if (splitCommand(command, "setoption", args)) return setOption(args);
        if (splitCommand(command, "position", args)) return position(args);
        if (splitCommand(command, "go", args)) return go(args);
    }


    void UCI::uci() noexcept {
        std::cout
            << "id name HydraY 1.2.1\n"
            << "id author Thomas Ghione, Daniele Ferretti, Simone Tomasella\n"
            << "option name BookFile type string default engine/komodo.bin\n"
            << "option name Opening type check default true\n"
            << "option name SyzygyPath type string default <empty>\n"
            << "option name SyzygyProbeDepth type spin default 1 min 1 max 100\n"
            << "option name SearchApiMutexGuard type check default true\n";
        const int hwThreads = omp_get_max_threads();
        std::cout << "option name Threads type spin default " << hwThreads
                  << " min 1 max " << hwThreads << "\n";
        std::cout << "option name Hash type spin default " << TT::DEFAULT_HASH_MB
                  << " min " << TT::MIN_HASH_MB
                  << " max " << TT::MAX_HASH_MB << "\n";
        for (const auto& option : kEvalOptions) {
            const auto [minValue, maxValue] = optionRange(option);
            std::cout << "option name " << optionDisplayName(option.key)
                      << " type spin default " << *option.value
                      << " min " << minValue
                      << " max " << maxValue << "\n";
        }
        std::cout << "uciok\n";
        std::cout.flush();
    }

    void UCI::setOption(std::string_view args) noexcept {
        if (args.empty()) return;

        // Stop any in-flight search before mutating shared engine state. The
        // option backing storage is plain `int32_t` (engine::* globals,
        // PIECE_VALUES, MVV_TABLE, Board::MATERIAL_VALUES); writing them while
        // a worker reads is a data race and `refreshPieceTables` would also
        // leave the incremental material/PSQT deltas out of sync with the new
        // table until the next FEN re-parse.
        stopSearch(false);

        string_view rest = args;
        if (nextToken(rest) != "name") return;
        rest = trimLeft(rest); // nextToken leaves the leading gap before the name

        const size_t valuePos = findWord(rest, "value");
        const string_view optionName =
            (valuePos == string_view::npos) ? trimRight(rest) : trimRight(rest.substr(0, valuePos));
        const string_view optionValue =
            (valuePos == string_view::npos) ? string_view{} : trimLeft(rest.substr(valuePos + 5));

        if (optionName.empty()) return;
        const std::string normalizedName = normalizedOptionName(optionName);
        if (normalizedName == "bookfile") {
            const std::string path(optionValue);
            if (engine.openingBook.load(path)) {
                std::cout << "info string BookFile loaded: " << path << "\n";
            } else {
                std::cout << "info string BookFile error: could not load '" << path << "'\n";
            }
            return;
        }

        if (normalizedName == "syzygypath") {
            const std::string path(optionValue);
            if (engine.syzygyProber.load(path)) {
                std::cout << "info string SyzygyPath loaded: " << path
                          << " (max pieces: " << engine.syzygyProber.maxPieces() << ")\n";
            } else {
                std::cout << "info string SyzygyPath error: could not load '" << path << "'\n";
            }
            return;
        }

        if (normalizedName == "syzygyprobedepth") {
            int v = 0;
            if (parseFirstInt(optionValue, v) && v >= 1 && v <= 100) {
                engine.syzygyProber.probeDepth = v;
                std::cout << "info string SyzygyProbeDepth set to " << v << "\n";
            } else {
                std::cout << "info string invalid value for SyzygyProbeDepth\n";
            }
            return;
        }

        if (normalizedName == "threads") {
            int v = 0;
            const int maxT = omp_get_max_threads();
            if (parseFirstInt(optionValue, v) && v >= 1) {
                const int clamped = std::clamp(v, 1, maxT);
                engine.requestedThreads = clamped;
                engine.searchRuntime.maxThreads = clamped;
                std::cout << "info string Threads set to " << clamped << "\n";
            } else {
                std::cout << "info string invalid value for Threads\n";
            }
            return;
        }

        if (normalizedName == "hash") {
            int v = 0;
            if (parseFirstInt(optionValue, v) && v >= 1) {
                const int clamped = std::clamp(v,
                    static_cast<int>(TT::MIN_HASH_MB),
                    static_cast<int>(TT::MAX_HASH_MB));
                if (engine.tt.resize(static_cast<size_t>(clamped))) {
                    std::cout << "info string Hash set to " << engine.tt.sizeMB() << " MB\n";
                } else {
                    std::cout << "info string Hash resize to " << clamped << " MB failed (out of memory)\n";
                }
            } else {
                std::cout << "info string invalid value for Hash\n";
            }
            return;
        }

        if (normalizedName != "opening" && normalizedName != "searchapimutexguard") {
            for (auto& option : kEvalOptions) {
                if (normalizedName != normalizedOptionName(option.key)) continue;
                int parsedValue = 0;
                if (!parseFirstInt(optionValue, parsedValue)) {
                    std::cout << "info string invalid value for " << optionName << "\n";
                    return;
                }
                const auto [minValue, maxValue] = optionRange(option);
                if (parsedValue < minValue || parsedValue > maxValue) {
                    std::cout << "info string value out of range for " << optionName
                              << " (" << minValue << ".." << maxValue << ")\n";
                    return;
                }
                *option.value = parsedValue;
                if (option.refreshPieceTables) {
                    refreshPieceTables();
                    // Rebuild the board's incremental material / PSQT deltas
                    // against the freshly-written MATERIAL_VALUES table. Without
                    // this, deltas accumulated under the old values keep being
                    // used by Evaluator::evaluate() until the next `position`
                    // command re-parses a FEN.
                    engine.board.rebuildBitboardsFromSquares();
                    // rebuildBitboardsFromSquares() repopulates the incremental fields but
                    // leaves any cached eval terms valid against the *old* table;
                    // drop them so the next evaluate() recomputes from scratch.
                    engine.board.clearEvalCache();
                }
                std::cout << "info string " << optionDisplayName(option.key)
                          << " set to " << *option.value << "\n";
                return;
            }
            return;
        }

        bool enabled = false;
        if (optionValue.empty() || !parseCheckValue(optionValue, enabled)) {
            std::cout << "info string invalid value for " << optionName << ": '" << optionValue
                      << "' (use true/false)\n";
            return;
        }

        if (normalizedName == "opening") {
            engine.openingEnabled.store(enabled, std::memory_order_relaxed);
            std::cout << "info string Opening "
                      << (enabled ? "enabled" : "disabled") << '\n';
            return;
        }

        engine.setSearchApiMutexEnabled(enabled);
        std::cout << "info string SearchApiMutexGuard "
                  << (engine.isSearchApiMutexEnabled() ? "enabled" : "disabled")
                  << '\n';
    }

    void UCI::position(std::string_view command) noexcept {
        stopSearch(false);
        string_view moves;

        if (command.starts_with("startpos") && (command.size() == 8 || std::isspace(command[8]))) {
            engine.board = chess::Board{};
            const size_t movesPos = findWord(command, "moves");
            if (movesPos != string_view::npos) moves = command.substr(movesPos);
        } else if (command.starts_with("fen") && (command.size() == 3 || std::isspace(command[3]))) {
            string_view fenPayload = trimLeft(command.substr(3));
            const size_t movesPos = findWord(fenPayload, "moves");
            if (movesPos == string_view::npos) {
                parseFEN(fenPayload);
            } else {
                parseFEN(trimRight(fenPayload.substr(0, movesPos)));
                moves = fenPayload.substr(movesPos);
            }
        } else {
            return;
        }

        engine.bestMove = chess::Move{};
        engine.moveHistory.clear();
        
        if (!moves.empty()) parseMoves(moves);
        
        engine.updateGameResult();
    }

    void UCI::ucinewgame() noexcept {
        stopSearch(false);
        engine.reset();
    }

    void UCI::isready() noexcept {
        std::cout << "readyok\n";
        std::cout.flush();
    }
    
    void UCI::go(std::string_view args) noexcept {
        stopSearch(false);
        engine::time::Limits limits;

        while (!args.empty()) {
            const string_view token = nextToken(args);
            if (token.empty()) break;

            if (token == "ponder")   { limits.ponder = true;   continue; }
            if (token == "infinite") { limits.infinite = true; continue; }

            if (token == "depth") {
                int v = 0;
                if (parseInt(nextToken(args), v) && v > 0) limits.maxDepth = v;
                continue;
            }
            if (token == "nodes") {
                int64_t v = 0;
                if (parseInt(nextToken(args), v) && v > 0) {
                    limits.maxNodes = static_cast<uint64_t>(v);
                }
                continue;
            }
            if (token == "movestogo") {
                int v = 0;
                if (parseInt(nextToken(args), v) && v > 0) limits.movestogo = v;
                continue;
            }

            int64_t v = 0;
            if (token == "wtime"    && parseInt(nextToken(args), v)) { limits.wtime = v; limits.hasClock = true; continue; }
            if (token == "btime"    && parseInt(nextToken(args), v)) { limits.btime = v; limits.hasClock = true; continue; }
            if (token == "winc"     && parseInt(nextToken(args), v)) { limits.winc = v;  continue; }
            if (token == "binc"     && parseInt(nextToken(args), v)) { limits.binc = v;  continue; }
            if (token == "movetime" && parseInt(nextToken(args), v)) { limits.movetime = v; continue; }
            if (token == "mate")    { (void)nextToken(args); continue; }
        }

        const bool ponder = limits.ponder;
        {
            std::lock_guard<std::mutex> lock(searchMutex);
            searchBestMove = "0000";
            searchPonder = ponder;
            searchDone = false;
            searchPrinted = false;
        }
        try {
            engine.searchRuntime.emitUciInfo = true; // UCI mode streams "info" lines
            searchThread = std::thread([this, limits, ponder] {
                const chess::Move move = engine.searchUCI(limits);
                std::string bestMove = move.toUCIString();
                if (!ponder) bestMove += ponderSuffix(engine, move);
                std::lock_guard<std::mutex> lock(searchMutex);
                searchBestMove = bestMove;
                searchDone = true;
                if (!searchPonder && !searchPrinted) emitBestMove(searchBestMove);
            });
        } catch (...) {
            std::lock_guard<std::mutex> lock(searchMutex);
            searchDone = true;
            if (!ponder) emitBestMove("0000");
        }
    }
    
    void UCI::ponderhit() noexcept {
        std::lock_guard<std::mutex> lock(searchMutex);
        searchPonder = false;
        if (searchDone && !searchPrinted) emitBestMove(searchBestMove);
    }

    void UCI::parseMoves(std::string_view moves) noexcept {
        // Precondition: caller passes the substring beginning at the "moves"
        // keyword (position() locates it via findWord), so just drop it.
        moves = trimLeft(moves);
        moves.remove_prefix(5);

        for (string_view move = nextToken(moves); !move.empty(); move = nextToken(moves)) {
            if (move.size() < 4) continue;

            const chess::Square from = chess::parseSquare(move.substr(0, 2));
            const chess::Square to = chess::parseSquare(move.substr(2, 2));
            if (!chess::isValidSquare(from) || !chess::isValidSquare(to)) continue;

            const char promo = (move.size() > 4) ? static_cast<char>(std::tolower(move[4])) : '\0';
            engine.movePiece(from, to, promo);
        }
    }

    void UCI::parseFEN(std::string_view fen) noexcept {
        engine.board = chess::Board(std::string(trimRight(trimLeft(fen))));
    }
}
