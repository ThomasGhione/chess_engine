#include "engine.hpp"
#include "../coords/coords.hpp"

namespace engine {

Engine::Engine(){
  this->board = chess::Board();
}


void Engine::playGameVsHuman() {
  while(!this->isMate()) {
    //! It doesn't check for loaded games, we should fix it later based on the activeColor in board

    // std::cout << print::Prints::getPrintableBoard( this->board.getCurrentPositionFen() ) << std::endl;

    std::cout << "It's white's turn: \n\n";
    this->takePlayerTurn();

    // std::cout << print::Prints::getPrintableBoard( this->board.getCurrentPositionFen() ) << std::endl;
    std::cout << "It's black's turn: \n\n";
    this->takePlayerTurn();

    // sleep(3);
  }
}

bool Engine::isMate(){
  // return this->board.isCurrentPositionMate();
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
  
        if (!this->board.move(fromCoords, toCoords)) {
            std::cout << "Invalid move. Please try again.";
            continue;
        }
  
        error = false;
   }
  
    return;
}


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
}


void Engine::playGameVsEngine(bool isWhite) {
  /*
	while (!isMate()) {
	if(isWhite){
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
*/
}

}

/*

void Engine::loadGame(bool isWithPlayer) {
    std::ifstream SaveFile("saves/save.txt");
    if (!SaveFile.is_open()) {
        // TODO Aggiungere messaggio di errore
        return;
    }

    std::string line;
    
    if (std::getline(SaveFile, line)) {
        board = chess::Board(line); 
    } 

	// TODO aggiungere controlli/eccezioni per il fen
	
	SaveFile.close();

	if (isWithPlayer) {
		this->playGameVsHuman();
	} else {
		std::cout << "Select your color:\n1. White\n2. Black\n";
		int choice;
		std::cin >> choice;
		if (choice == 1) {
			this->isPlayerWhite = true;
		} else {
			this->isPlayerWhite = false;
		}

		this->playGameVsEngine(this->isPlayerWhite);
	}

}



void Engine::takeEngineTurn() {
    //! EVERYTHING'S A PLACEHOLDER
    
    //evaluate();
    //chess::Coords currentCoords;
    //chess::Coords targetCoords;
    //chess::Piece pieceToMove;
    //board.movePiece(pieceToMove, targetCoords);
}*/
/*
void Engine::takePlayerTurn() {
    // reading input
    std::cout << "Insert the coords: ";
    char fromCoordsFile, fromCoordsRank, toFile, toRank;
    std::cin >> fromCoordsFile >> fromCoordsRank >> toFile >> toRank;
    
    // setting variables up
    // TOBE FIXED: Stiamo convertendo un char in un numero uint8_t
    // Non mi pare converta '0' in 0
    chess::Coords currentCoords = {static_cast<uint8_t>(fromCoordsFile - '0'), static_cast<uint8_t>(fromCoordsRank - 'a')};
    chess::Coords targetCoords = {static_cast<uint8_t>(toFile - '0'), static_cast<uint8_t>(toRank - 'a')};
    chess::Piece pieceToMove = board[board.fromCoordsCoordsToPosition(currentCoords)];

    // moving the piece 
    board.movePiece(pieceToMove, targetCoords);

}


*/


