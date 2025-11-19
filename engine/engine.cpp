#include "engine.hpp"
#include "../coords/coords.hpp"

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

	constexpr auto coefficientPiece = [](uint8_t piece) {
	    return 2 * (piece >> 4) - 1; 
  	};

	constexpr auto pieceValue = [](int8_t x) {
    	return static_cast<int64_t>(x * (-1267.0/60.0 +
        	x * (3445.0/72.0 +
        	x * (-881.0/24.0 +
        	x * (937.0/72.0 +
        	x * (-87.0/40.0 +
        	x * (5.0/36.0))))))
        );
	};

  	int64_t delta = 0;
  	const uint8_t MAX_INDEX = 64;
  	for(uint8_t i = 0; i < MAX_INDEX; i++) {
    	uint8_t piece = b.get( i % 8, i/8);
    	delta += coefficientPiece(piece) * pieceValue(piece);
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


void Engine::search(uint64_t depth) {
    if (depth == 0) return;
    

    std::vector<chess::Board::Move> moves;
    generateLegalMoves(this->board, moves);

    if (moves.empty()) {
        return; // nessuna mossa legale: posizione terminale
    }

    const uint8_t us = this->board.getActiveColor();

    // Alpha-beta is always from the side-to-move point of view:
    // White tries to maximise the score, Black to minimise it.
    int64_t alpha = NEG_INF;
    int64_t beta  = POS_INF;
    int64_t bestScore = (us == chess::Board::WHITE) ? NEG_INF : POS_INF;
    chess::Board::Move bestMove = moves.front(); // temporary initialization

    for (const auto& m : moves) {
        chess::Board copy = this->board;
        if (!copy.moveBB(m.from, m.to)) {
            continue;
        }
        int64_t score = searchPosition(copy, depth - 1, alpha, beta);

        if (us == chess::Board::WHITE) {
            // White is the maximizing player
            if (score > bestScore) {
                bestScore = score;
                bestMove = m;
            }
            if (score > alpha) alpha = score;
        } else {
            // Black is the minimizing player
            if (score < bestScore) {
                bestScore = score;
                bestMove = m;
            }
            if (score < beta) beta = score;
        }
    }

    // Stampa la mossa scelta dall'engine in notazione semplice (es: e2e4)
    auto toAlgebraic = [](const chess::Coords& c) {
        char fileChar = static_cast<char>('a' + c.file);
        char rankChar = static_cast<char>('1' + c.rank);
        std::string s;
        s.push_back(fileChar);
        s.push_back(rankChar);
        return s;
    };

    std::string moveStr = toAlgebraic(bestMove.from) + toAlgebraic(bestMove.to);
    std::cout << "Engine plays: " << moveStr << " (score: " << bestScore << ")\n";

    // Esegui sulla board principale la mossa migliore trovata
    (void)this->board.moveBB(bestMove.from, bestMove.to);
}

int64_t Engine::searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta) {
    this->nodesSearched++;  // una posizione visitata


    const uint8_t us = b.getActiveColor();
    const bool usIsWhite = (us == chess::Board::WHITE);

    if (depth == 0 || b.isCheckmate(us) || b.isStalemate(us)) {
        return evaluate(b);
    }

    bool inCheck = b.inCheck(us);
    if (inCheck && depth > 0) depth++; // extend search if in check
    
    
    std::vector<chess::Board::Move> moves;
    generateLegalMoves(b, moves);
    if (moves.empty()) {
        return evaluate(b);
    }


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

        orderedScoredMoves.push_back(ScoredMove{m, score});
    }

    std::sort(orderedScoredMoves.begin(), orderedScoredMoves.end(),
              [usIsWhite](const ScoredMove& a, const ScoredMove& b) {
                  return usIsWhite ? (a.score > b.score) : (a.score < b.score);
    });


    int64_t best = usIsWhite ? NEG_INF : POS_INF;

    for (const auto& scoredMove : orderedScoredMoves) {
        const auto& m = scoredMove.move;
        chess::Board copy = b;
        if (!copy.moveBB(m.from, m.to)) continue;

        int64_t score = searchPosition(copy, depth - 1, alpha, beta);

        if (usIsWhite) {
            if (score > best) best = score;
            if (score > alpha) alpha = score;
        } else {
            if (score < best) best = score;
            if (score < beta) beta = score;
        }

        if (alpha >= beta) break; // alpha-beta cutoff
    }

    return best;
}

// Vecchia versione basata su scan 0..63
void Engine::generateLegalMoves_old(const chess::Board& b,
                                    std::vector<chess::Board::Move>& moves) const {
    moves.clear();

    const uint8_t color = b.getActiveColor();
    const bool isWhite = (color == chess::Board::WHITE);

    const uint64_t occ = b.getPiecesBitMap();

    for (uint8_t from = 0; from < 64; ++from) {
        const uint8_t piece = b.get(from);
        if ((piece & chess::Board::MASK_PIECE_TYPE) == chess::Board::EMPTY) continue;
        if ((piece & chess::Board::MASK_COLOR) != color) continue;

        const uint8_t piece_type = (piece & chess::Board::MASK_PIECE_TYPE);
        uint64_t mask = 0ULL;

        switch (piece_type) {
            case chess::Board::PAWN: {
                const uint64_t attacks = pieces::getPawnAttacks(static_cast<int16_t>(from), isWhite);
                const uint64_t pushes  = pieces::getPawnForwardPushes(static_cast<int16_t>(from), isWhite, occ);
                mask = attacks | pushes;
                break;
            }
            case chess::Board::KNIGHT:
                mask = pieces::getKnightAttacks(static_cast<int16_t>(from));
                break;
            case chess::Board::BISHOP:
                mask = pieces::getBishopAttacks(static_cast<int16_t>(from), occ);
                break;
            case chess::Board::ROOK:
                mask = pieces::getRookAttacks(static_cast<int16_t>(from), occ);
                break;
            case chess::Board::QUEEN:
                mask = pieces::getQueenAttacks(static_cast<int16_t>(from), occ);
                break;
            case chess::Board::KING:
                mask = pieces::getKingAttacks(static_cast<int16_t>(from));
                break;
            default:
                continue;
        }

        while (mask) {
            const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(mask));
            mask &= (mask - 1);

            const uint8_t dst = b.get(to);
            if (dst != chess::Board::EMPTY && (dst & chess::Board::MASK_COLOR) == color) continue;

            chess::Coords fromC{from};
            chess::Coords toC{to};

            if (!b.canMoveToBB(fromC, toC)) continue;

            moves.push_back(chess::Board::Move{fromC, toC});
        }
    }

#ifdef DEBUG
    // std::cout << "[DEBUG] generateLegalMoves_old found " << moves.size() << " moves.\n";
#endif
}

// Nuova versione bitboard-based per tipo
void Engine::generateLegalMoves(const chess::Board& b,
                                std::vector<chess::Board::Move>& moves) const {
    moves.clear();

    const uint8_t color = b.getActiveColor();
    const bool isWhite = (color == chess::Board::WHITE);

    const uint64_t occ = b.getPiecesBitMap();

    // Bitboard dei nostri pezzi (solo caselle occupate dal nostro colore)
    uint64_t ownOcc = 0ULL;
    for (uint8_t idx = 0; idx < 64; ++idx) {
        uint8_t piece = b.get(idx);
        if ((piece & chess::Board::MASK_PIECE_TYPE) == chess::Board::EMPTY) continue;
        if ((piece & chess::Board::MASK_COLOR) != color) continue;
        ownOcc |= (1ULL << idx);
    }

    auto addMovesFromMask = [&](uint8_t from, uint64_t mask) {
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
    for (uint8_t idx = 0; idx < 64; ++idx) {
        uint8_t piece = b.get(idx);
        if ((piece & chess::Board::MASK_PIECE_TYPE) != chess::Board::PAWN) continue;
        if ((piece & chess::Board::MASK_COLOR) != color) continue;

        const uint8_t from = idx;

        const uint64_t attacks = pieces::getPawnAttacks(static_cast<int16_t>(from), isWhite);
        const uint64_t pushes  = pieces::getPawnForwardPushes(static_cast<int16_t>(from), isWhite, occ);

        addMovesFromMask(from, attacks | pushes);
    }

    // Knights
    for (uint8_t idx = 0; idx < 64; ++idx) {
        uint8_t piece = b.get(idx);
        if ((piece & chess::Board::MASK_PIECE_TYPE) != chess::Board::KNIGHT) continue;
        if ((piece & chess::Board::MASK_COLOR) != color) continue;

        const uint8_t from = idx;
        const uint64_t mask = pieces::getKnightAttacks(static_cast<int16_t>(from));

        addMovesFromMask(from, mask);
    }

    // Bishops
    for (uint8_t idx = 0; idx < 64; ++idx) {
        uint8_t piece = b.get(idx);
        if ((piece & chess::Board::MASK_PIECE_TYPE) != chess::Board::BISHOP) continue;
        if ((piece & chess::Board::MASK_COLOR) != color) continue;

        const uint8_t from = idx;
        const uint64_t mask = pieces::getBishopAttacks(static_cast<int16_t>(from), occ);

        addMovesFromMask(from, mask);
    }

    // Rooks
    for (uint8_t idx = 0; idx < 64; ++idx) {
        uint8_t piece = b.get(idx);
        if ((piece & chess::Board::MASK_PIECE_TYPE) != chess::Board::ROOK) continue;
        if ((piece & chess::Board::MASK_COLOR) != color) continue;

        const uint8_t from = idx;
        const uint64_t mask = pieces::getRookAttacks(static_cast<int16_t>(from), occ);

        addMovesFromMask(from, mask);
    }

    // Queens
    for (uint8_t idx = 0; idx < 64; ++idx) {
        uint8_t piece = b.get(idx);
        if ((piece & chess::Board::MASK_PIECE_TYPE) != chess::Board::QUEEN) continue;
        if ((piece & chess::Board::MASK_COLOR) != color) continue;

        const uint8_t from = idx;
        const uint64_t mask = pieces::getQueenAttacks(static_cast<int16_t>(from), occ);

        addMovesFromMask(from, mask);
    }

    // Kings
    for (uint8_t idx = 0; idx < 64; ++idx) {
        uint8_t piece = b.get(idx);
        if ((piece & chess::Board::MASK_PIECE_TYPE) != chess::Board::KING) continue;
        if ((piece & chess::Board::MASK_COLOR) != color) continue;

        const uint8_t from = idx;
        const uint64_t mask = pieces::getKingAttacks(static_cast<int16_t>(from));

        addMovesFromMask(from, mask);
    }

#ifdef DEBUG
    // std::cout << "[DEBUG] generateLegalMoves_new found " << moves.size() << " moves.\n";
#endif
}

int64_t Engine::evaluate(const chess::Board& board) {

    // 1) EVALUATION CHECKMATE
    const uint8_t sideToMove = board.getActiveColor();
    if (board.isCheckmate(sideToMove)) {
        return (sideToMove == chess::Board::BLACK) ? POS_INF : NEG_INF;
    }
    const uint8_t otherSide = (sideToMove == chess::Board::WHITE)
                                ? chess::Board::BLACK
                                : chess::Board::WHITE;
    if (board.isCheckmate(otherSide)) {
        return (otherSide == chess::Board::BLACK) ? POS_INF : NEG_INF;
    }


    int64_t eval = 0;

    int64_t materialDelta = getMaterialDeltaSLOW(board);
    eval += materialDelta;

    
    // 2) IS THIS AN ENDGAME?

    // count pieces (not including pawns, kings)
    int nonPawnNonKingPieces = 0;
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
    for (uint8_t i = 0; i < 64; ++i) {
        uint8_t piece = board.get(i);
        uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
        uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

        if (pieceType == chess::Board::EMPTY) continue;

        int64_t posValue = 0;

        switch (pieceType) {
            case chess::Board::PAWN:
                posValue = isEndgame ? PAWN_END_GAME_VALUES_TABLE[i] 
                                     : PAWN_VALUES_TABLE[i];
                break;
            case chess::Board::KNIGHT:
                posValue = KNIGHT_VALUES_TABLE[i];
                break;
            case chess::Board::BISHOP:
                posValue = BISHOP_VALUES_TABLE[i];
                break;
            case chess::Board::ROOK:
                posValue = ROOK_VALUES_TABLE[i];
                break;
            case chess::Board::QUEEN:
                posValue = QUEEN_VALUES_TABLE[i];
                break;
            case chess::Board::KING:
                posValue = isEndgame ? KING_END_GAME_VALUES_TABLE[i] 
                                     : KING_MIDDLE_GAME_VALUES_TABLE[i];
                break;
        }

        eval += (pieceColor == chess::Board::WHITE) ? posValue : -posValue;
    }



    int64_t whiteEval = 0, blackEval = 0;
    
    eval += (whiteEval - blackEval);
    return eval;
}
// 4365















void Engine::playGameVsHuman() {
	while(!this->isMate()) {
	    //! It doesn't check for loaded games, we should fix it later based on the activeColor in board

	    // std::cout << print::Prints::getPrintableBoard( this->board.getCurrentPositionFen() ) << std::endl;

	std::cout << "It's white's turn: \n\n";
	this->takePlayerTurn();
	if (this->isMate()) break;

	    // std::cout << print::Prints::getPrintableBoard( this->board.getCurrentPositionFen() ) << std::endl;
	std::cout << "It's black's turn: \n\n";
	this->takePlayerTurn();
	if (this->isMate()) break;

	    // sleep(3);
	}
}

bool Engine::isMate() {
    uint8_t toMove = this->board.getActiveColor();
    if (this->board.isCheckmate(toMove) || this->board.isStalemate(toMove)) {
        return true;
    }
    return false;
}


void Engine::takePlayerTurn() {
    std::string playerInput;

    bool isWhiteTurn = (this->board.getActiveColor() == chess::Board::WHITE);
    std::string currentBoard = print::Prints::getBasicBoard(this->board);

    bool error = true;
    while (error) {
        std::cout << currentBoard << "\n";

        std::cout << "Enter your move (write 's' to save the game or 'q' to quit): ";
        std::cin >> playerInput;

        //! TODO Check if player wants to save or quit

        /*
        if (playerInput == "s") {
            this->saveGame();
            return;
        }

        if (playerInput == "q") {
            this->quitGame();
            return;
        }
        */

        if (playerInput.length() != 4) {
            std::cout << "Invalid move length. Please enter your move in the format 'e2e4'.\n";
            continue;
        }

        chess::Coords fromCoords(playerInput.substr(0, 2));
        chess::Coords toCoords(playerInput.substr(2, 2));
  
        if (!chess::Coords::isInBounds(fromCoords) || !chess::Coords::isInBounds(toCoords)) {
            std::cout << "Invalid move format. Please enter your move in the format 'e2e4'.\n";
            continue;
        }
  
        uint8_t piece = this->board.get(fromCoords);
  

        if (piece == chess::Board::EMPTY) {
            std::cout << "There is no piece at the source square. Please enter a valid move.\n";
            continue;
        }

        if (isWhiteTurn != (this->board.getColor(fromCoords) == chess::Board::WHITE)) {
            std::cout << "It's not your turn to move that piece. Please enter a valid move.\n";
            continue;
        }

        // TODO: check whether it's redundant or not
        if (this->board.isSameColor(fromCoords, toCoords)) {
            std::cout << "You cannot move to a square occupied by your own piece.\n";
            continue;
        }
#ifdef DEBUG
        auto chrono_start = std::chrono::high_resolution_clock::now();
#endif  
        if (!this->board.moveBB(fromCoords, toCoords)) {
            std::cout << "Invalid move. Please try again.";
            continue;
        }
#ifdef DEBUG
        auto chrono_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed = chrono_end - chrono_start;
        std::cout << "[DEBUG] MoveBB executed in " << elapsed.count() << " microseconds.\n";
#endif
      
        // After successful move, detect terminal state for next side to move
        uint8_t nextColor = this->board.getActiveColor();
        if (this->board.isCheckmate(nextColor)) {
            std::cout << "\nCheckmate! " << (nextColor == chess::Board::WHITE ? "Black" : "White") << " wins.\n";
            return; // exit turn early
        }
        if (this->board.isStalemate(nextColor)) {
            std::cout << "\nStalemate. Game drawn.\n";
            return;
        }
      
        error = false;
  }
  
    return;
}

/*
void Engine::saveGame() {
    if (std::filesystem::exists("save.txt")) {
        char ans;
        
        std::cout << "A save file has been detected, do you want to overwrite it? (Y/N) ";
        std::cin >> ans;
        if (ans == 'Y' || ans == 'y') {
          std::filesystem::remove("saves/save.txt");
        }
        else {
            return;
        }   
    }
    
    std::ofstream SaveFile("saves/save.txt");
    SaveFile << board.getCurrentFen(); 
    SaveFile.close();
} */


void Engine::playGameVsEngine(bool isWhite) {
    // Gioco engine vs umano.
    // Se isWhite == true, il giocatore umano gioca il bianco, altrimenti il nero.

    while (!this->isMate()) {
        const uint8_t toMove = this->board.getActiveColor();
        const bool whiteToMove = (toMove == chess::Board::WHITE);

        const bool humanToMove = (isWhite && whiteToMove) || (!isWhite && !whiteToMove);

        if (humanToMove) {
            // Turno dell'umano
            std::cout << (whiteToMove ? "It's your (White) turn:\n\n"
                                      : "It's your (Black) turn:\n\n");
            this->takePlayerTurn();
        } else {
            // Turno dell'engine
            std::cout << (whiteToMove ? "Engine (White) is thinking...\n"
                                      : "Engine (Black) is thinking...\n");
#ifdef DEBUG
            auto chrono_start = std::chrono::high_resolution_clock::now();
#endif  
            // Per semplicità usiamo la profondità di default del motore
            this->search(this->depth);
#ifdef DEBUG
            auto chrono_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = chrono_end - chrono_start;
            std::cout << "[DEBUG] Engine search took " << elapsed.count() << " ms.\n";
            std::cout << "[DEBUG] Nodes searched so far: " << this->nodesSearched << "\n";
#endif
            // Stampa la board aggiornata dopo la mossa dell'engine
            std::string currentBoard = print::Prints::getBasicBoard(this->board);
            std::cout << currentBoard << "\n";
        }

        // Dopo ogni mossa controlla stato terminale
        if (this->isMate()) {
            uint8_t nextColor = this->board.getActiveColor();
            if (this->board.isCheckmate(nextColor)) {
                std::cout << "\nCheckmate! "
                          << (nextColor == chess::Board::WHITE ? "Black" : "White")
                          << " wins.\n";
            } else if (this->board.isStalemate(nextColor)) {
                std::cout << "\nStalemate. Game drawn.\n";
            }
            break;
        }
    }
}

/*
void Engine::playGameVsEngine(bool isWhite) {
    while (!isMate()) {
    if(isWhite) {
      std::cout << "It's your turn: ";
      takePlayerTurn();
      std::cout << "Engine's thinking... ";
      takeEngineTurn();
    }else{
      std::cout << "Engine's thinking... ";
      takeEngineTurn();
      std::cout << "It's your turn: ";
      takePlayerTurn();
    }
  }

}
*/

} // namespace engine
