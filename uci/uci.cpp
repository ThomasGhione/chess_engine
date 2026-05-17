#include "uci.hpp"

#include "../board/board.hpp"
#include "../engine/engine.hpp"
#include "../engine/eval_constants.hpp"

#include <algorithm>
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

    static std::string optionDisplayName(std::string_view rawName) {
        std::string display;
        display.reserve(rawName.size());
        bool upperNext = true;
        for (const char c : rawName) {
            if (c == '_' || c == '-' || c == ' ') {
                upperNext = true;
                continue;
            }
            display.push_back(upperNext ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                                         : static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
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

    static void defaultRangeFor(int32_t value, int32_t& minValue, int32_t& maxValue) noexcept {
        const int64_t absValue = std::llabs(static_cast<int64_t>(value));
        const int64_t delta = std::max<int64_t>(2, absValue / 10);
        const int64_t minCandidate = static_cast<int64_t>(value) - delta;
        const int64_t maxCandidate = static_cast<int64_t>(value) + delta;
        minValue = static_cast<int32_t>(std::clamp(minCandidate, static_cast<int64_t>(INT32_MIN), static_cast<int64_t>(INT32_MAX)));
        maxValue = static_cast<int32_t>(std::clamp(maxCandidate, static_cast<int64_t>(INT32_MIN), static_cast<int64_t>(INT32_MAX)));
    }

    static void refreshPieceTables() noexcept {
        engine::PIECE_VALUES[0] = 0;
        engine::PIECE_VALUES[1] = engine::PAWN_VALUE;
        engine::PIECE_VALUES[2] = engine::KNIGHT_VALUE;
        engine::PIECE_VALUES[3] = engine::BISHOP_VALUE;
        engine::PIECE_VALUES[4] = engine::ROOK_VALUE;
        engine::PIECE_VALUES[5] = engine::QUEEN_VALUE;
        engine::PIECE_VALUES[6] = engine::KING_VALUE;
        engine::PIECE_VALUES[7] = 0;

        engine::MVV_TABLE[0] = 0;
        engine::MVV_TABLE[1] = engine::PAWN_VALUE * 10;
        engine::MVV_TABLE[2] = engine::KNIGHT_VALUE * 10;
        engine::MVV_TABLE[3] = engine::BISHOP_VALUE * 10;
        engine::MVV_TABLE[4] = engine::ROOK_VALUE * 10;
        engine::MVV_TABLE[5] = engine::QUEEN_VALUE * 10;
        engine::MVV_TABLE[6] = engine::KING_VALUE * 10;

        chess::Board::MATERIAL_VALUES[0] = 0;
        chess::Board::MATERIAL_VALUES[1] = engine::PAWN_VALUE;
        chess::Board::MATERIAL_VALUES[2] = engine::KNIGHT_VALUE;
        chess::Board::MATERIAL_VALUES[3] = engine::BISHOP_VALUE;
        chess::Board::MATERIAL_VALUES[4] = engine::ROOK_VALUE;
        chess::Board::MATERIAL_VALUES[5] = engine::QUEEN_VALUE;
        chess::Board::MATERIAL_VALUES[6] = engine::KING_VALUE;
        chess::Board::MATERIAL_VALUES[7] = 0;
    }

    static EvalOption kEvalOptions[] = {
        {"PAWN_VALUE", &engine::PAWN_VALUE, true, false, 0, 0},
        {"KNIGHT_VALUE", &engine::KNIGHT_VALUE, true, false, 0, 0},
        {"BISHOP_VALUE", &engine::BISHOP_VALUE, true, false, 0, 0},
        {"ROOK_VALUE", &engine::ROOK_VALUE, true, false, 0, 0},
        {"QUEEN_VALUE", &engine::QUEEN_VALUE, true, false, 0, 0},
        {"KING_VALUE", &engine::KING_VALUE, true, false, 0, 0},
        {"MATE_SCORE", &engine::MATE_SCORE, false, true, 0, 2'147'483'647},
        {"PHASE_FINAL_THRESHOLD", &engine::PHASE_FINAL_THRESHOLD, false, false, 0, 0},
        {"DOUBLED_PAWN_PENALTY", &engine::DOUBLED_PAWN_PENALTY, false, false, 0, 0},
        {"ISOLATED_PAWN_PENALTY", &engine::ISOLATED_PAWN_PENALTY, false, false, 0, 0},
        {"PASSED_PAWN_BONUS", &engine::PASSED_PAWN_BONUS, false, false, 0, 0},
        {"PAWN_ISLAND_PENALTY", &engine::PAWN_ISLAND_PENALTY, false, false, 0, 0},
        {"PAWN_SUPPORT_BONUS", &engine::PAWN_SUPPORT_BONUS, false, false, 0, 0},
        {"CANDIDATE_PASSER_BONUS", &engine::CANDIDATE_PASSER_BONUS, false, false, 0, 0},
        {"CONNECTED_PASSER_BONUS", &engine::CONNECTED_PASSER_BONUS, false, false, 0, 0},
        {"BACKWARD_PAWN_PENALTY", &engine::BACKWARD_PAWN_PENALTY, false, false, 0, 0},
        {"PASSED_PAWN_BLOCKED_PENALTY", &engine::PASSED_PAWN_BLOCKED_PENALTY, false, false, 0, 0},
        {"CENTER_CONTROL_BONUS", &engine::CENTER_CONTROL_BONUS, false, false, 0, 0},
        {"BISHOP_PAIR_BONUS", &engine::BISHOP_PAIR_BONUS, false, false, 0, 0},
        {"KING_NON_CASTLING_PENALTY", &engine::KING_NON_CASTLING_PENALTY, false, false, 0, 0},
        {"KING_LOST_CASTLING_RIGHTS_PENALTY", &engine::KING_LOST_CASTLING_RIGHTS_PENALTY, false, false, 0, 0},
        {"LOSS_OF_CASTLING_PENALTY", &engine::LOSS_OF_CASTLING_PENALTY, false, false, 0, 0},
        {"INIT_BONUS_MG", &engine::INIT_BONUS_MG, false, false, 0, 0},
        {"INIT_BONUS_EG", &engine::INIT_BONUS_EG, false, false, 0, 0},
        {"DEVELOPMENT_BONUS", &engine::DEVELOPMENT_BONUS, false, false, 0, 0},
        {"LOW_MOBILITY_KNIGHT_PENALTY", &engine::LOW_MOBILITY_KNIGHT_PENALTY, false, false, 0, 0},
        {"PINNED_KNIGHT_PENALTY", &engine::PINNED_KNIGHT_PENALTY, false, false, 0, 0},
        {"LOW_MOBILITY_BISHOP_PENALTY", &engine::LOW_MOBILITY_BISHOP_PENALTY, false, false, 0, 0},
        {"PINNED_BISHOP_PENALTY", &engine::PINNED_BISHOP_PENALTY, false, false, 0, 0},
        {"LOW_MOBILITY_ROOK_PENALTY", &engine::LOW_MOBILITY_ROOK_PENALTY, false, false, 0, 0},
        {"PINNED_ROOK_PENALTY", &engine::PINNED_ROOK_PENALTY, false, false, 0, 0},
        {"LOW_MOBILITY_QUEEN_PENALTY", &engine::LOW_MOBILITY_QUEEN_PENALTY, false, false, 0, 0},
        {"PINNED_QUEEN_PENALTY", &engine::PINNED_QUEEN_PENALTY, false, false, 0, 0},
        {"MOBILITY_CENTER_BONUS", &engine::MOBILITY_CENTER_BONUS, false, false, 0, 0},
        {"MOBILITY_OWN_PAWN_BLOCKER_PENALTY", &engine::MOBILITY_OWN_PAWN_BLOCKER_PENALTY, false, false, 0, 0},
        {"QUEEN_EARLY_MOBILITY_THRESHOLD", &engine::QUEEN_EARLY_MOBILITY_THRESHOLD, false, false, 0, 0},
        {"QUEEN_EARLY_MOBILITY_PENALTY", &engine::QUEEN_EARLY_MOBILITY_PENALTY, false, false, 0, 0},
        {"OUTPOST_CENTER_FILE_BONUS", &engine::OUTPOST_CENTER_FILE_BONUS, false, false, 0, 0},
        {"OUTPOST_NEAR_CENTER_FILE_BONUS", &engine::OUTPOST_NEAR_CENTER_FILE_BONUS, false, false, 0, 0},
        {"OUTPOST_ADVANCED_RANK_BONUS", &engine::OUTPOST_ADVANCED_RANK_BONUS, false, false, 0, 0},
        {"OUTPOST_KING_ZONE_BONUS", &engine::OUTPOST_KING_ZONE_BONUS, false, false, 0, 0},
        {"OUTPOST_KEY_SQUARE_BONUS", &engine::OUTPOST_KEY_SQUARE_BONUS, false, false, 0, 0},
        {"COORDINATION_PENALTY", &engine::COORDINATION_PENALTY, false, false, 0, 0},
        {"OUTPOST_BISHOP_BONUS", &engine::OUTPOST_BISHOP_BONUS, false, false, 0, 0},
        {"OUTPOST_KNIGHT_BONUS", &engine::OUTPOST_KNIGHT_BONUS, false, false, 0, 0},
{"HANGING_PAWN_PENALTY", &engine::HANGING_PAWN_PENALTY, false, false, 0, 0},
        {"HANGING_PAWN_NEAR_KING_PENALTY", &engine::HANGING_PAWN_NEAR_KING_PENALTY, false, false, 0, 0},
        {"HANGING_HOOK_PAWN_PENALTY", &engine::HANGING_HOOK_PAWN_PENALTY, false, false, 0, 0},
        {"HANGING_MINOR_PENALTY", &engine::HANGING_MINOR_PENALTY, false, false, 0, 0},
        {"HANGING_ROOK_PENALTY", &engine::HANGING_ROOK_PENALTY, false, false, 0, 0},
        {"HANGING_QUEEN_PENALTY", &engine::HANGING_QUEEN_PENALTY, false, false, 0, 0},
        {"THREAT_PAWN_ATTACK_MINOR_PENALTY", &engine::THREAT_PAWN_ATTACK_MINOR_PENALTY, false, false, 0, 0},
        {"THREAT_PAWN_ATTACK_ROOK_PENALTY", &engine::THREAT_PAWN_ATTACK_ROOK_PENALTY, false, false, 0, 0},
        {"THREAT_PAWN_ATTACK_QUEEN_PENALTY", &engine::THREAT_PAWN_ATTACK_QUEEN_PENALTY, false, false, 0, 0},
        {"THREAT_MINOR_ATTACK_ROOK_PENALTY", &engine::THREAT_MINOR_ATTACK_ROOK_PENALTY, false, false, 0, 0},
        {"THREAT_MINOR_ATTACK_QUEEN_PENALTY", &engine::THREAT_MINOR_ATTACK_QUEEN_PENALTY, false, false, 0, 0},
        {"THREAT_ROOK_ATTACK_QUEEN_PENALTY", &engine::THREAT_ROOK_ATTACK_QUEEN_PENALTY, false, false, 0, 0},
        {"THREAT_PAWN_PUSH_MINOR_PENALTY", &engine::THREAT_PAWN_PUSH_MINOR_PENALTY, false, false, 0, 0},
        {"THREAT_PAWN_PUSH_ROOK_PENALTY", &engine::THREAT_PAWN_PUSH_ROOK_PENALTY, false, false, 0, 0},
        {"THREAT_PAWN_PUSH_QUEEN_PENALTY", &engine::THREAT_PAWN_PUSH_QUEEN_PENALTY, false, false, 0, 0},
        {"THREAT_LOOSE_MINOR_PENALTY", &engine::THREAT_LOOSE_MINOR_PENALTY, false, false, 0, 0},
        {"THREAT_LOOSE_ROOK_PENALTY", &engine::THREAT_LOOSE_ROOK_PENALTY, false, false, 0, 0},
        {"THREAT_LOOSE_QUEEN_PENALTY", &engine::THREAT_LOOSE_QUEEN_PENALTY, false, false, 0, 0},
        {"PAWN_FORK_BASE_BONUS", &engine::PAWN_FORK_BASE_BONUS, false, false, 0, 0},
        {"PAWN_FORK_MAJOR_BONUS", &engine::PAWN_FORK_MAJOR_BONUS, false, false, 0, 0},
        {"PAWN_FORK_ROYAL_BONUS", &engine::PAWN_FORK_ROYAL_BONUS, false, false, 0, 0},
        {"OPEN_FILE_ROOK_BONUS", &engine::OPEN_FILE_ROOK_BONUS, false, false, 0, 0},
        {"SEMI_OPEN_FILE_ROOK_BONUS", &engine::SEMI_OPEN_FILE_ROOK_BONUS, false, false, 0, 0},
        {"ROOK_ON_SEVENTH_BONUS", &engine::ROOK_ON_SEVENTH_BONUS, false, false, 0, 0},
        {"ROOK_BEHIND_OWN_PASSER_BONUS", &engine::ROOK_BEHIND_OWN_PASSER_BONUS, false, false, 0, 0},
        {"ROOK_BEHIND_ENEMY_PASSER_BONUS", &engine::ROOK_BEHIND_ENEMY_PASSER_BONUS, false, false, 0, 0},
        {"ROOK_EG_EDGE_BONUS", &engine::ROOK_EG_EDGE_BONUS, false, false, 0, 0},
        {"ROOK_EG_PRESSURE_BONUS", &engine::ROOK_EG_PRESSURE_BONUS, false, false, 0, 0},
        {"KING_SAFETY_PENALTY", &engine::KING_SAFETY_PENALTY, false, false, 0, 0},
        {"KING_ACTIVITY_BONUS", &engine::KING_ACTIVITY_BONUS, false, false, 0, 0},
        {"CASTLE_PAWN_SUPPORT_BONUS", &engine::CASTLE_PAWN_SUPPORT_BONUS, false, false, 0, 0},
        {"KING_SHELTER_STRONG_BONUS", &engine::KING_SHELTER_STRONG_BONUS, false, false, 0, 0},
        {"KING_SHELTER_WEAK_BONUS", &engine::KING_SHELTER_WEAK_BONUS, false, false, 0, 0},
        {"KING_SHELTER_MISSING_PENALTY", &engine::KING_SHELTER_MISSING_PENALTY, false, false, 0, 0},
        {"KING_PAWN_STORM_NEAR_PENALTY", &engine::KING_PAWN_STORM_NEAR_PENALTY, false, false, 0, 0},
        {"KING_PAWN_STORM_FAR_PENALTY", &engine::KING_PAWN_STORM_FAR_PENALTY, false, false, 0, 0},
        {"KING_CASTLED_SHIELD_BREAK_PENALTY", &engine::KING_CASTLED_SHIELD_BREAK_PENALTY, false, false, 0, 0},
        {"KING_SHELTER_ADVANCE_ONE_PENALTY", &engine::KING_SHELTER_ADVANCE_ONE_PENALTY, false, false, 0, 0},
        {"KING_SHELTER_ADVANCE_TWO_PENALTY", &engine::KING_SHELTER_ADVANCE_TWO_PENALTY, false, false, 0, 0},
        {"KING_HOOK_PAWN_ATTACKED_PENALTY", &engine::KING_HOOK_PAWN_ATTACKED_PENALTY, false, false, 0, 0},
        {"KING_HOOK_PAWN_HANGING_PENALTY", &engine::KING_HOOK_PAWN_HANGING_PENALTY, false, false, 0, 0},
        {"KING_SAFETY_OPENING_SCALE_PERCENT", &engine::KING_SAFETY_OPENING_SCALE_PERCENT, false, false, 0, 0},
        {"KING_SEMI_OPEN_FILE_PENALTY", &engine::KING_SEMI_OPEN_FILE_PENALTY, false, false, 0, 0},
        {"KING_OPEN_FILE_PENALTY", &engine::KING_OPEN_FILE_PENALTY, false, false, 0, 0},
        {"KING_FILE_PRESSURE_PENALTY", &engine::KING_FILE_PRESSURE_PENALTY, false, false, 0, 0},
        {"KING_OPEN_DIAGONAL_PENALTY", &engine::KING_OPEN_DIAGONAL_PENALTY, false, false, 0, 0},
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
        {"STALEMATE_DRAW_PENALTY_MAJOR", &engine::STALEMATE_DRAW_PENALTY_MAJOR, false, false, 0, 0},
        {"STALEMATE_DRAW_PENALTY_MINOR", &engine::STALEMATE_DRAW_PENALTY_MINOR, false, false, 0, 0},
        {"STALEMATE_MATERIAL_THRESHOLD", &engine::STALEMATE_MATERIAL_THRESHOLD, false, false, 0, 0},
        {"CHECK_BONUS", &engine::CHECK_BONUS, false, false, 0, 0},
        {"KILLER1_BONUS", &engine::KILLER1_BONUS, false, false, 0, 0},
        {"KILLER2_BONUS", &engine::KILLER2_BONUS, false, false, 0, 0}
    };

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

    static bool parseI64(string_view token, int64_t& out) noexcept {
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

    static std::string ponderSuffix(const engine::Engine& engine, const chess::Board::Move& bestMove) noexcept {
        chess::Board board = engine.board;
        if (!board.move(bestMove.from, bestMove.to, bestMove.promotionPiece)) return {};

        uint16_t encodedMove = 0;
        if (!engine.tt.probeMove(board.getHash(), encodedMove)) return {};

        const auto move = TranspositionTable::Entry::decodeMove(encodedMove);
        const chess::Board::Move ponderMove{chess::Coords{move.from}, chess::Coords{move.to}, move.promo};
        return board.move(ponderMove.from, ponderMove.to, ponderMove.promotionPiece)
            ? (" ponder " + ponderMove.toUCIString())
            : std::string{};
    }

}

namespace uci {
    UCI::UCI(engine::Engine& e) : engine(e) {}

    UCI::~UCI() noexcept {
        finishSearch(true, false);
    }

    void UCI::emitBestMove(std::string_view move) noexcept {
        std::cout << "bestmove " << move << '\n';
        std::cout.flush();
        searchPrinted = true;
    }

    void UCI::finishSearch(bool requestStop, bool printBestMove) noexcept {
        if (requestStop) engine.stopThinking();
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
            finishSearch(true, true);
            return quit();
        }
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
            << "id name HydraY 1.1.0\n"
            << "id author Thomas Ghione, Daniele Ferretti, Simone Tomasella\n"
            << "option name PonderDebug type check default false\n"
            << "option name SearchApiMutexGuard type check default true\n";
        for (const auto& option : kEvalOptions) {
            int32_t minValue = option.minValue;
            int32_t maxValue = option.maxValue;
            if (!option.hasRange) {
                defaultRangeFor(*option.value, minValue, maxValue);
            }
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

        string_view rest = args;
        if (nextToken(rest) != "name") return;

        const std::size_t valuePos = findWord(rest, "value");
        const string_view optionName =
            (valuePos == string_view::npos) ? trimRight(rest) : trimRight(rest.substr(0, valuePos));
        const string_view optionValue =
            (valuePos == string_view::npos) ? string_view{} : trimLeft(rest.substr(valuePos + 5));

        if (optionName.empty()) return;
        const std::string normalizedName = normalizedOptionName(optionName);
        if (normalizedName != "ponderdebug" && normalizedName != "searchapimutexguard") {
            for (auto& option : kEvalOptions) {
                if (normalizedName != normalizedOptionName(option.key)) continue;
                int parsedValue = 0;
                string_view valueRest = optionValue;
                string_view valueToken = nextToken(valueRest);
                if (!parseInt(valueToken, parsedValue)) {
                    std::cout << "info string invalid value for " << optionName << "\n";
                    return;
                }
                int32_t minValue = option.minValue;
                int32_t maxValue = option.maxValue;
                if (!option.hasRange) {
                    defaultRangeFor(*option.value, minValue, maxValue);
                }
                if (parsedValue < minValue || parsedValue > maxValue) {
                    std::cout << "info string value out of range for " << optionName
                              << " (" << minValue << ".." << maxValue << ")\n";
                    return;
                }
                *option.value = parsedValue;
                if (option.refreshPieceTables) {
                    refreshPieceTables();
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
        finishSearch(true, false);
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
        finishSearch(true, false);
        engine.reset();
    }

    void UCI::isready() noexcept {
        std::cout << "readyok\n";
        std::cout.flush();
    }
    
    void UCI::go(std::string_view args) noexcept {
        finishSearch(true, false);
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
                if (parseI64(nextToken(args), v) && v > 0) {
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
            if (token == "wtime"    && parseI64(nextToken(args), v)) { limits.wtime = v; limits.hasClock = true; continue; }
            if (token == "btime"    && parseI64(nextToken(args), v)) { limits.btime = v; limits.hasClock = true; continue; }
            if (token == "winc"     && parseI64(nextToken(args), v)) { limits.winc = v;  continue; }
            if (token == "binc"     && parseI64(nextToken(args), v)) { limits.binc = v;  continue; }
            if (token == "movetime" && parseI64(nextToken(args), v)) { limits.movetime = v; continue; }
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
            searchThread = std::thread([this, limits, ponder] {
                const chess::Board::Move move = engine.searchUCI(limits);
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
    
    void UCI::stop() noexcept {
        finishSearch(true, true);
    }

    void UCI::ponderhit() noexcept {
        std::lock_guard<std::mutex> lock(searchMutex);
        searchPonder = false;
        if (searchDone && !searchPrinted) emitBestMove(searchBestMove);
    }

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
