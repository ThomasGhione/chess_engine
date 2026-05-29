#pragma once

#ifdef DEBUG

#include <chrono>
#include <cstdint>
#include <iostream>

#include "engine/eval/evaluator.hpp"



class DebugTimer {
public:
    void start() noexcept { t0_ = std::chrono::steady_clock::now(); }

    void us(const char* label) const noexcept {
        const auto t = std::chrono::steady_clock::now();
        const std::chrono::duration<double, std::micro> d = t - t0_;
        std::cout << "[DEBUG] " << label << ": " << d.count() << " us\n";
    }

    void ms(const char* label) const noexcept {
        const auto t = std::chrono::steady_clock::now();
        const std::chrono::duration<double, std::milli> d = t - t0_;
        std::cout << "[DEBUG] " << label << ": " << d.count() << " ms\n";
    }

private:
    std::chrono::steady_clock::time_point t0_{};
};

#define DBG_TIMER_DECLARE(name) DebugTimer name
#define DBG_TIMER_START(name)   (name).start()
#define DBG_TIMER_US(name, msg) (name).us(msg)
#define DBG_TIMER_MS(name, msg) (name).ms(msg)
#define DBG_LOG_STREAM(expr)    do { std::cout << expr; } while (0)
#define DBG_ONLY(...)           do { __VA_ARGS__ } while (0)



namespace engine {

inline void Evaluator::traceTerm(int32_t& eval, int32_t delta, const char* label) noexcept {
    eval += delta;
    std::cout << "  [TRACE] +" << label << ": " << eval << " (delta=" << delta << ")\n";
}

inline void traceTermPV(int32_t& eval, engine::PhaseValue delta, const char* label, bool eg) noexcept {
    const int32_t v = eg ? delta.eg : delta.mg;
    eval += v;
    std::cout << "  [TRACE] +" << label << ": " << eval << " (delta=" << v << " from PhaseValue{mg=" << delta.mg << ",eg=" << delta.eg << "})\n";
}

inline int32_t Evaluator::evaluateTrace(const chess::Board& board) noexcept {
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0) [[unlikely]] {
        return -POS_INF; // terminal position (negamax/side-to-move relative)
    }

    const int32_t material = board.getIncrementalMaterialDelta();
    std::cout << "  [TRACE] material: " << material << '\n';

    const uint64_t occ = board.getPiecesBitMap();
    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];
    const PhaseInfo phase = classifyPhase(board);

    std::cout << "  [TRACE] phase: w1024=" << phase.w1024
              << " (phaseWeight=" << phase.phaseWeight
              << ", totalPawns=" << phase.totalPawns
              << ", pawnOnlyEG=" << phase.pawnOnlyEndgame << ")\n";

    int32_t psqtMg = 0;
    int32_t psqtEg = 0;
    board.getIncrementalPsqtMgEg(psqtMg, psqtEg);
    std::cout << "  [TRACE] PSQT mg=" << psqtMg << " eg=" << psqtEg << '\n';

    if (phase.pawnOnlyEndgame) {
        int32_t eval = material + psqtEg;
        AttackData pawnAttacks[2]{};
        pawnAttacks[0].allAttacks = collectPawnAttacks(whitePawns, 0);
        pawnAttacks[1].allAttacks = collectPawnAttacks(blackPawns, 1);
        traceTermPV(eval, evalHangingPieces(board, pawnAttacks), "hangingPieces", true);
        traceTermPV(eval, evalPawnStructureCached(board, whitePawns, blackPawns, true), "pawnStructure(eg)", true);
        traceTermPV(eval, evalKingActivity(board, true), "kingActivity(eg)", true);
        traceTermPV(eval, evalEndgameKingActivity(board), "endgameKingActivity", true);
        traceTermPV(eval, evalMopUp(board), "mopUp", true);
        traceTermPV(eval, evalPassedPawnKeySquares(board, whitePawns, blackPawns), "passedKeySq", true);
        traceTermPV(eval, evalRuleOfSquare(board, whitePawns, blackPawns), "ruleOfSquare", true);
        traceTermPV(eval, evalInitiative(board, true), "initiative(eg)", true);
        std::cout << "  [TRACE] TOTAL (pawnOnlyEG): " << eval << '\n';
        return eval;
    }

    AttackData attackData[2]{};
    computeAttackData(attackData, board, occ);

    int32_t mgAcc = material + psqtMg;
    int32_t egAcc = material + psqtEg;

    auto bothTerm = [&](PhaseValue v, const char* label) {
        mgAcc += v.mg; egAcc += v.eg;
        std::cout << "  [TRACE] +" << label << " (both): mg=" << mgAcc << " eg=" << egAcc << " (delta=" << v.mg << "/" << v.eg << ")\n";
    };
    auto mgTerm = [&](PhaseValue v, const char* label) {
        mgAcc += v.mg;
        std::cout << "  [TRACE] +" << label << " (mg): mg=" << mgAcc << " (delta=" << v.mg << ")\n";
    };
    auto egTerm = [&](PhaseValue v, const char* label) {
        egAcc += v.eg;
        std::cout << "  [TRACE] +" << label << " (eg): eg=" << egAcc << " (delta=" << v.eg << ")\n";
    };
    auto pairTerm = [&](PhaseValue pv, const char* label) {
        mgAcc += pv.mg; egAcc += pv.eg;
        std::cout << "  [TRACE] +" << label << " (pair): mg+=" << pv.mg << " eg+=" << pv.eg << '\n';
    };

    bothTerm(evalBishopPairBonusCached(board), "bishopPair");
    bothTerm(evalHangingPieces(board, attackData), "hangingPieces");
    bothTerm(evalMobility(attackData), "mobility");
    bothTerm(evalSpaceAdvantage(board, whitePawns, blackPawns), "space");

    pairTerm(evalThreatsPair(board, attackData, occ), "threats");
    pairTerm(evalPawnStructureCachedPair(board, whitePawns, blackPawns), "pawnStructure");
    pairTerm(evalKingActivityPair(board), "kingActivity");
    pairTerm(evalInitiativePair(board), "initiative");

    mgTerm(evalMinorPieceDevelopmentCached(board), "minorDev");
    mgTerm(evalEarlyQueenCached(board), "earlyQueen");
    mgTerm(evalCastlingBonusCached(board), "castling");
    mgTerm(evalCentralControlCached(board, whitePawns, blackPawns), "centralControl");
    mgTerm(evalPieceCoordinationCached(board), "coordination");
    mgTerm(evalOutpostsCached(board), "outposts");
    mgTerm(evalKingSafetyWithAttackData(board, whitePawns, blackPawns, attackData), "kingSafety");
    mgTerm(evalBlockedPawnByBishopsCached(board), "blockedPawnBishops");
    mgTerm(evalPawnForks(board), "pawnForks");
    mgTerm(evalBlockedCenterWithPieces(board, occ), "blockedCenter");
    mgTerm(evalKingMiddlegame(board, whitePawns, blackPawns, attackData), "kingMiddlegame");

    bothTerm(evalTrappedPieces(board, occ), "trappedPieces");
    bothTerm(evalBadBishopCached(board, whitePawns, blackPawns), "badBishop");
    bothTerm(evalRooksCached(board, whitePawns, blackPawns), "rooks");
    bothTerm(evalWeakSquaresCached(board, whitePawns, blackPawns), "weakSquares");
    bothTerm(evalBishopVsKnightCached(board, whitePawns, blackPawns), "bishopVsKnight");

    egTerm(evalEndgameKingActivity(board), "endgameKingActivity");
    egTerm(evalMopUp(board), "mopUp");
    egTerm(evalPassedPawnKeySquares(board, whitePawns, blackPawns), "passedKeySq");
    egTerm(evalRuleOfSquare(board, whitePawns, blackPawns), "ruleOfSquare");
    egTerm(evalRookEndgamePressure(board), "rookEgPressure");
    egTerm(evalQueenEndgamePressure(board), "queenEgPressure");
    egTerm(evalDoubleRookEndgame(board), "doubleRookEg");

    const int32_t blended = PhaseValue{mgAcc, egAcc}.blend(phase.w1024);
    std::cout << "  [TRACE] TOTAL: mg=" << mgAcc << " eg=" << egAcc
              << " w1024=" << phase.w1024 << " blended=" << blended << '\n';
    return blended;
}

} // namespace engine

#else

#define DBG_TIMER_DECLARE(name)
#define DBG_TIMER_START(name)   ((void)0)
#define DBG_TIMER_US(name, msg) ((void)0)
#define DBG_TIMER_MS(name, msg) ((void)0)
#define DBG_LOG_STREAM(expr)    ((void)0)
#define DBG_ONLY(...)           ((void)0)

#endif
