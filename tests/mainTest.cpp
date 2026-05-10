#include "ut.hpp"
#include "../board/board.hpp"
#include "../board/piece.hpp"
#include "../engine/eval/evaluator.hpp"
#include "../engine/search/searcher.hpp"

#include <cassert>

namespace {

void testQuiescenceScoresThreefoldAsDrawBeforeStaticEval() {
    chess::Board board("k7/8/8/8/8/8/8/K6R w - - 0 1");

    assert(board.move(chess::Coords("h1"), chess::Coords("g1")));
    assert(board.move(chess::Coords("a8"), chess::Coords("b8")));
    assert(board.move(chess::Coords("g1"), chess::Coords("h1")));
    assert(board.move(chess::Coords("b8"), chess::Coords("a8")));
    assert(board.move(chess::Coords("h1"), chess::Coords("g1")));
    assert(board.move(chess::Coords("a8"), chess::Coords("b8")));
    assert(board.move(chess::Coords("g1"), chess::Coords("h1")));
    assert(board.move(chess::Coords("b8"), chess::Coords("a8")));

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

} // namespace

int main(){
    // Initialize magic bitboards before running tests
    pieces::initMagicBitboards();
    testQuiescenceScoresThreefoldAsDrawBeforeStaticEval();
}
