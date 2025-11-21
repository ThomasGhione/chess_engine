#include "engine.hpp"
#include "../coords/coords.hpp"

#include <omp.h>

#ifdef DEBUG
#include <chrono>
#include <iostream>
#endif

namespace engine {

uint64_t Engine::nodesSearched = 0;

Engine::Engine()
    : board(chess::Board())
    , depth(6)
{
    // this->nodesSearched = 0;
    // per ora non avviamo la search automaticamente nel costruttore
}

int64_t Engine::getMaterialDelta(const chess::Board& b) noexcept {

	static constexpr auto coefficientPiece = [](uint8_t piece) {
	    return 2 * (piece & chess::Board::MASK_COLOR) - 1; 
  	};

	static constexpr auto pieceValue = [](int8_t x) {
    	return static_cast<int64_t>(x * (-1267.0/60.0 +
        	x * (3445.0/72.0 +
        	x * (-881.0/24.0 +
        	x * (937.0/72.0 +
        	x * (-87.0/40.0 +
        	x * (5.0/36.0))))))
        );
	};

  	int64_t delta = 0;
  	static constexpr uint8_t MAX_INDEX = 64;
    // #pragma omp parallel for reduction(+:delta)
    for (uint8_t i = 0; i < MAX_INDEX; i++) {
    	uint8_t piece = b.get(i);
    	delta += coefficientPiece(piece & chess::Board::MASK_PIECE_TYPE) * pieceValue(piece);
    }

    return delta;
}

int64_t Engine::getMaterialDeltaSLOW(const chess::Board& b) noexcept {

    int64_t delta = 0;
    constexpr uint8_t MAX_INDEX = 64;

    for (uint8_t i = 0; i < MAX_INDEX; i++) {
        uint8_t piece = b.get(i);
        uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
        if (pieceType != chess::Board::EMPTY) {
            int64_t value = pieceValues.at(pieceType);
            int8_t colorFactor = piece & chess::Board::MASK_COLOR ? -1 : 1;
            delta += colorFactor * value;
        }
    }

    return delta;
}


void Engine::updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore, 
                 chess::Board::Move& bestMove, const chess::Board::Move& m) {
    if (usIsWhite) {
        // White is the maximizing player
        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }
        if (score > alpha) alpha = score;
        return;
    }
    // Black is the minimizing player
    if (score < bestScore) {
        bestScore = score;
        bestMove = m;
    }
    if (score < beta) beta = score;
}

void Engine::updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& best) {
    if (usIsWhite) {
        if (score > best) best = score;
        if (score > alpha) alpha = score;
        return; 
    }
    if (score < best) best = score;
    if (score < beta) beta = score;
}

void Engine::search(uint64_t depth) {
    if (depth == 0) return;

    std::vector<chess::Board::Move> moves = this->generateLegalMoves(this->board);

    if (moves.empty()) return;
    

    const uint8_t us = this->board.getActiveColor();
    const bool usIsWhite = (us == chess::Board::WHITE);

    this->nodesSearched = 0; // reset the nodes searched counter

    // Alpha-beta is always from the side-to-move point of view:
    // White tries to maximise the score, Black to minimise it.
    int64_t alpha = NEG_INF;
    int64_t beta  = POS_INF;
    int64_t bestScore = (usIsWhite) ? NEG_INF : POS_INF;
    chess::Board::Move bestMove = moves.front(); // temporary initialization

    for (const auto& m : moves) {
        chess::Board copy = this->board;
        if (!copy.moveBB(m.from, m.to)) {
            continue;
        }
        constexpr int currPly = 1;
        int64_t score = this->searchPosition(copy, depth - 1, alpha, beta, currPly);

        this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);
    }

    // Esegui sulla board principale la mossa migliore trovata
    (void)this->board.moveBB(bestMove.from, bestMove.to);

    // TODO spostare in driver
    std::string moveStr = chess::Coords::toAlgebric(bestMove.from) + chess::Coords::toAlgebric(bestMove.to);
    std::cout << "Engine plays: " << moveStr << " (score: " << bestScore << ")\n";
}

int64_t Engine::searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply) {
    this->nodesSearched++;  // una posizione visitata

    const uint8_t us = b.getActiveColor();
    const bool usIsWhite = (us == chess::Board::WHITE);

    if (depth == 0 || b.isCheckmate(us) || b.isStalemate(us)) {
        return this->evaluate(b);
    }

    bool inCheck = b.inCheck(us);
    if (inCheck && depth > 0) depth++; // extend search if in check
    
    
    std::vector<chess::Board::Move> moves = this->generateLegalMoves(b);
    if (moves.empty()) return this->evaluate(b);
    std::vector<ScoredMove> orderedScoredMoves = this->sortLegalMoves(moves, ply, b, usIsWhite);


    int64_t best = usIsWhite ? NEG_INF : POS_INF;

    // #pragma omp parallel for schedule(dynamic) // TODO check whether schedule(dynamic) works or not
    for (const auto& scoredMove : orderedScoredMoves) {
        const auto& m = scoredMove.move;
        chess::Board copy = b;
        if (!copy.moveBB(m.from, m.to)) continue;

        int64_t score = this->searchPosition(copy, depth - 1, alpha, beta, ply + 1);

        this->updateMinMax(usIsWhite, score, alpha, beta, best);
        
        // TODO wrap in a function
        // Beta cutoff: update killer moves and history for non-captures
        if (alpha >= beta) {
            if (ply < MAX_PLY) {
                uint8_t toPiece   = b.get(m.to);
                uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
                const bool isCapture = (toPieceType != chess::Board::EMPTY);

                if (!isCapture) {
                    // Killer moves
                    auto& km1 = killerMoves[0][ply];
                    auto& km2 = killerMoves[1][ply];
                    if (!(m.from.file == km1.from.file && m.from.rank == km1.from.rank &&
                          m.to.file   == km1.to.file   && m.to.rank   == km1.to.rank)) {
                        km2 = km1;
                        km1 = m;
                    }

                    // History heuristic
                    int colorIndex = (us == chess::Board::WHITE) ? 0 : 1;
                    int fromIndex = m.from.rank * 8 + m.from.file;
                    int toIndex   = m.to.rank   * 8 + m.to.file;
                    int bonus = static_cast<int>((depth + 1) * (depth + 1));
                    history[colorIndex][fromIndex][toIndex] += bonus;
                }
            }
            break; // alpha-beta cutoff
        }
        
    }

    return best;
}


// Nuova versione bitboard-based per tipo
// TODO fare un array di uint64_t ownPieces per fare un for sull'array per togliere codice duplicato + parallel for
std::vector<chess::Board::Move> Engine::generateLegalMoves(const chess::Board& b) const {
    std::vector<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const bool isWhite = (color == chess::Board::WHITE);
    // Usa occupancy e bitboard per tipo/colore dalla Board
    const uint64_t occ = b.getPiecesBitMap();

    uint64_t ownPawns   = (color == chess::Board::WHITE) ? b.pawns_bb[0]   : b.pawns_bb[1];
    uint64_t ownKnights = (color == chess::Board::WHITE) ? b.knights_bb[0] : b.knights_bb[1];
    uint64_t ownBishops = (color == chess::Board::WHITE) ? b.bishops_bb[0] : b.bishops_bb[1];
    uint64_t ownRooks   = (color == chess::Board::WHITE) ? b.rooks_bb[0]   : b.rooks_bb[1];
    uint64_t ownQueens  = (color == chess::Board::WHITE) ? b.queens_bb[0]  : b.queens_bb[1];
    uint64_t ownKings   = (color == chess::Board::WHITE) ? b.kings_bb[0]   : b.kings_bb[1];

    auto addMovesFromMask = [&](uint8_t from, uint64_t mask) {
        // Rimuovi le caselle occupate dai nostri pezzi
        uint64_t ownOcc = ownPawns | ownKnights | ownBishops | ownRooks | ownQueens | ownKings;
        mask &= ~ownOcc;

        while (mask) {
            const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(mask));
            mask &= (mask - 1);

            chess::Coords fromC{from};
            chess::Coords toC{to};

            if (!b.canMoveToBB(fromC, toC)) continue;

            moves.push_back(chess::Board::Move{fromC, toC});
        }
    };

    // Pawns
    while (ownPawns) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownPawns));
        ownPawns &= (ownPawns - 1);

        const uint64_t attacks = pieces::getPawnAttacks(static_cast<int16_t>(from), isWhite);
        const uint64_t pushes  = pieces::getPawnForwardPushes(static_cast<int16_t>(from), isWhite, occ);

        addMovesFromMask(from, attacks | pushes);
    }

    // Knights
    while (ownKnights) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownKnights));
        ownKnights &= (ownKnights - 1);

        const uint64_t mask = pieces::getKnightAttacks(static_cast<int16_t>(from));
        addMovesFromMask(from, mask);
    }

    // Bishops
    while (ownBishops) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownBishops));
        ownBishops &= (ownBishops - 1);

        const uint64_t mask = pieces::getBishopAttacks(static_cast<int16_t>(from), occ);
        addMovesFromMask(from, mask);
    }

    // Rooks
    while (ownRooks) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownRooks));
        ownRooks &= (ownRooks - 1);

        const uint64_t mask = pieces::getRookAttacks(static_cast<int16_t>(from), occ);
        addMovesFromMask(from, mask);
    }

    // Queens
    while (ownQueens) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownQueens));
        ownQueens &= (ownQueens - 1);

        const uint64_t mask = pieces::getQueenAttacks(static_cast<int16_t>(from), occ);
        addMovesFromMask(from, mask);
    }

    // Kings
    while (ownKings) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownKings));
        ownKings &= (ownKings - 1);

        const uint64_t mask = pieces::getKingAttacks(static_cast<int16_t>(from));
        addMovesFromMask(from, mask);
    }

#ifdef DEBUG
    // std::cout << "[DEBUG] generateLegalMoves_new found " << moves.size() << " moves.\n";
#endif

    return moves;
}

std::vector<Engine::ScoredMove> Engine::sortLegalMoves(const std::vector<chess::Board::Move>& moves, int ply, const chess::Board& b, bool usIsWhite) {
    std::vector<ScoredMove> orderedScoredMoves;
    orderedScoredMoves.reserve(moves.size());
    for (const auto& m : moves) {
        int64_t score = 0;

        uint8_t fromPiece = b.get(m.from);
        uint8_t toPiece = b.get(m.to);
        uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;
        uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;

        // MVV-LVA (Most Valuable Victim - Least Valuable Attacker)
        if (toPieceType != chess::Board::EMPTY) {
            int64_t victimValue = pieceValues.at(toPieceType);
            int64_t attackerValue = pieceValues.at(fromPieceType);
            score += (victimValue * 10 - attackerValue); // MVV-LVA    
        }

        const bool isCapture = (toPieceType != chess::Board::EMPTY);

        if (fromPieceType == chess::Board::PAWN) {
            // Bonus per le promozioni
            if ((usIsWhite && m.to.rank == 7) || (!usIsWhite && m.to.rank == 0)) {
                score += pieceValues.at(chess::Board::QUEEN);
            }
        }

        // TODO: da rivedere, non sempre dare uno scacco e' la mossa migliore
        chess::Board checkBoard = b;
        if (checkBoard.moveBB(m.from, m.to)) {
            uint8_t opponent = usIsWhite ? chess::Board::BLACK : chess::Board::WHITE;
            if (checkBoard.inCheck(opponent)) {
                score += 50;
            }
        }

        // Killer move bonus for non-captures
        if (!isCapture && ply < MAX_PLY) {
            const auto& km1 = killerMoves[0][ply];
            const auto& km2 = killerMoves[1][ply];
            if (m.from.file == km1.from.file && m.from.rank == km1.from.rank &&
                m.to.file == km1.to.file && m.to.rank == km1.to.rank) {
                score += 100000;
            } else if (m.from.file == km2.from.file && m.from.rank == km2.from.rank &&
                       m.to.file == km2.to.file && m.to.rank == km2.to.rank) {
                score += 90000;
            }

            // History heuristic bonus (only for non-captures)
            int colorIndex = (usIsWhite) ? 0 : 1;
            int fromIndex = m.from.rank * 8 + m.from.file;
            int toIndex   = m.to.rank   * 8 + m.to.file;
            score += history[colorIndex][fromIndex][toIndex];
        }

        orderedScoredMoves.push_back(ScoredMove{m, score});
    }

    std::sort(orderedScoredMoves.begin(), orderedScoredMoves.end(),
              [usIsWhite](const ScoredMove& a, const ScoredMove& b) {
                  return usIsWhite ? (a.score > b.score) : (a.score < b.score);
    });

    return orderedScoredMoves;
}


int64_t Engine::evaluateCheckmate(const chess::Board& board) {
    return (board.getActiveColor() == chess::Board::BLACK) ? POS_INF : NEG_INF;
}

int64_t Engine::evaluate(const chess::Board& board) {

    // 1) EVALUATION CHECKMATE
    if (board.isCheckmate(board.getActiveColor())) {
        return this->evaluateCheckmate(board);
    }

    int64_t eval = 0;

    int64_t materialDelta = getMaterialDeltaSLOW(board);
    eval += materialDelta;

    
    // 2) IS THIS AN ENDGAME?

    // count pieces (not including pawns, kings)
    int nonPawnNonKingPieces = 0;
    // #pragma omp parallel for reduction(+:nonPawnNonKingPieces)
    for (uint8_t i = 0; i < 64; ++i) {
        uint8_t piece = board.get(i);
        uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
        if (pieceType == chess::Board::EMPTY ||
            pieceType == chess::Board::PAWN ||
            pieceType == chess::Board::KING) {
            continue;
        }
        ++nonPawnNonKingPieces;
    }
    bool isEndgame = (nonPawnNonKingPieces <= PHASE_FINAL_THRESHOLD);

    // 3) BONUS POSITION TABLE
    // #pragma omp parallel for reduction(+:eval)
    for (uint8_t i = 0; i < 64; ++i) {
        uint8_t piece = board.get(i);
        uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
        uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

        if (pieceType == chess::Board::EMPTY) continue;

        int64_t posValue = 0;
        
        uint8_t newIdx = i;
        if (pieceColor == chess::Board::BLACK) {
            newIdx = mirrorIndex(i);
        }

        switch (pieceType) {
            case chess::Board::PAWN:
                posValue = isEndgame ? PAWN_END_GAME_VALUES_TABLE[newIdx] 
                                     : PAWN_VALUES_TABLE[newIdx];
                break;
            case chess::Board::KNIGHT:
                posValue = KNIGHT_VALUES_TABLE[newIdx];
                break;
            case chess::Board::BISHOP:
                posValue = BISHOP_VALUES_TABLE[newIdx];
                break;
            case chess::Board::ROOK:
                posValue = ROOK_VALUES_TABLE[newIdx];
                break;
            case chess::Board::QUEEN:
                posValue = QUEEN_VALUES_TABLE[newIdx];
                break;
            case chess::Board::KING:
                posValue = isEndgame ? KING_END_GAME_VALUES_TABLE[newIdx] 
                                     : KING_MIDDLE_GAME_VALUES_TABLE[newIdx];
                break;
        }

        eval += (pieceColor == chess::Board::WHITE) ? posValue : -posValue;
    }



    int64_t whiteEval = 0, blackEval = 0;
    
    eval += (whiteEval - blackEval);
    return eval;
}
// 4365

bool Engine::isMate() {
    uint8_t toMove = this->board.getActiveColor();
    if (this->board.isCheckmate(toMove) || this->board.isStalemate(toMove)) {
        return true;
    }
    return false;
}

} // namespace engine
