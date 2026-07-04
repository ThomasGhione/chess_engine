#include "ut.hpp"
#include "../board/board.hpp"
#include "../board/piece.hpp"
#include "../engine/eval/evaluator.hpp"
#include "../engine/search/searcher.hpp"

#include <cassert>

namespace {

void testQuiescenceScoresThreefoldAsDrawBeforeStaticEval() {
    chess::Board board("k7/8/8/8/8/8/8/K6R w - - 0 1");

    assert(board.move({chess::parseSquare("h1"), chess::parseSquare("g1")}));
    assert(board.move({chess::parseSquare("a8"), chess::parseSquare("b8")}));
    assert(board.move({chess::parseSquare("g1"), chess::parseSquare("h1")}));
    assert(board.move({chess::parseSquare("b8"), chess::parseSquare("a8")}));
    assert(board.move({chess::parseSquare("h1"), chess::parseSquare("g1")}));
    assert(board.move({chess::parseSquare("a8"), chess::parseSquare("b8")}));
    assert(board.move({chess::parseSquare("g1"), chess::parseSquare("h1")}));
    assert(board.move({chess::parseSquare("b8"), chess::parseSquare("a8")}));

    assert(board.isThreefoldRepetition());
    assert(engine::Evaluator::evaluate(board) > 0);

    engine::Searcher::SearchRuntime runtime{};
    const int32_t qScore = engine::Searcher::quiescenceSearch(
        board,
        runtime,
        engine::Searcher::NEG_INF,
        engine::Searcher::POS_INF,
        0,
        false);

    assert(qScore < 0);
}

void testRootDrawTerminalDoesNotSearchFallbackMove() {
    chess::Board board("k7/8/8/8/8/8/8/KB6 w - - 0 1");
    engine::Searcher::SearchRuntime runtime{};

    assert(board.isDraw(board.getActiveColor()));
    const chess::Move bestMove = engine::Searcher::searchBestMove(board, runtime, 4);

    assert(!chess::isValidSquare(bestMove.from));
    assert(!chess::isValidSquare(bestMove.to));
    assert(runtime.eval == 0);
    assert(runtime.nodesSearched == 0);
}

void testBoardMoveRejectsWrongSideToMove() {
    chess::Board board;
    assert(!board.move({chess::parseSquare("e7"), chess::parseSquare("e5")}));
    assert(board.getActiveColor() == chess::Board::WHITE);
    assert(board.move({chess::parseSquare("e2"), chess::parseSquare("e4")}));
    assert(!board.move({chess::parseSquare("d2"), chess::parseSquare("d4")}));
}

} // namespace

int main(){
    // Initialize magic bitboards before running tests
    pieces::initMagicBitboards();
    testQuiescenceScoresThreefoldAsDrawBeforeStaticEval();
    testRootDrawTerminalDoesNotSearchFallbackMove();
    testBoardMoveRejectsWrongSideToMove();
}
