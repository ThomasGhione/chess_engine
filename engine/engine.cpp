#include "engine.hpp"
#include "../coords/coords.hpp"

#ifdef DEBUG
#include <chrono>
#include <iostream>
#endif

namespace engine {

Engine::Engine()
    : board(chess::Board())
    , depth(4)
{
	search(this->depth);
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

    static const std::unordered_map<uint8_t, int64_t> pieceValues = {
        {chess::Board::PAWN, PAWN_VALUE},
        {chess::Board::KNIGHT, KNIGHT_VALUE},
        {chess::Board::BISHOP, BISHOP_VALUE},
        {chess::Board::ROOK, ROOK_VALUE},
        {chess::Board::QUEEN, QUEEN_VALUE},
        {chess::Board::KING, KING_VALUE}
    };

    int64_t delta = 0;
    constexpr uint8_t MAX_INDEX = 64;

    for (uint8_t i = 0; i < MAX_INDEX; i++) {
        uint8_t piece = b.get(i);
        uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
        if (pieceType != chess::Board::EMPTY) {
            int64_t value = pieceValues.at(pieceType);
            int8_t colorFactor = 2 * (piece >> 4) - 1; // +1 for white, -1 for black
            delta += colorFactor * value;
        }
    }

    return delta;
}


void Engine::search(uint64_t depth) {
	// Placeholder for search algorithm
	// This function would implement the search logic (e.g., Minimax, Alpha-Beta pruning)
	// to determine the best move for the engine at the given depth.
	// For now, it does nothing.

    if (depth == 0) {
        return;
    }

    
	
	search(depth - 1);
}

int64_t Engine::evaluate(const chess::Board& board) {

    int64_t whiteEval = 0, blackEval = 0;
    int64_t eval = whiteEval - blackEval;
    uint8_t perspective = board.getActiveColor() == chess::Board::WHITE ? 1 : -1;


    int64_t materialDelta = getMaterialDeltaSLOW(board);

	return eval * perspective;
}
















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
    std::string output = "In questo momento non è possibile giocare come ";
    if (isWhite) {
        output += "bianco";
    } else {
        output += "nero";
    }
    output += " sarà una futura possibilità.\n";
  
    std::cout << output;
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
