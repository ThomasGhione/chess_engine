#include "engine.hpp"
#include "../coords/coords.hpp"

namespace engine {

Engine::Engine(){
  this->board = chess::Board();
}


void Engine::playGameVsHuman() {
  while(!this->isMate()) {
    // std::cout << print::Prints::getPrintableBoard( this->board.getCurrentPositionFen() ) << std::endl;
    std::cout << print::Prints::getBasicBoard( this->board ) << std::endl;

    std::cout << "It's white's turn: ";
    this->takePlayerTurn();

    // std::cout << print::Prints::getPrintableBoard( this->board.getCurrentPositionFen() ) << std::endl;
    std::cout << print::Prints::getBasicBoard( this->board ) << std::endl;

    std::cout << "It's black's turn: ";
    this->takePlayerTurn();

    // sleep(3);
  }
}

bool Engine::isMate(){
  // return this->board.isCurrentPositionMate();
  return false;
}


void Engine::takePlayerTurn() {
  std::cout << "Funzione leggi la mossa non ancora implementata" << std::endl;
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
    char fromFile, fromRank, toFile, toRank;
    std::cin >> fromFile >> fromRank >> toFile >> toRank;
    
    // setting variables up
    // TOBE FIXED: Stiamo convertendo un char in un numero uint8_t
    // Non mi pare converta '0' in 0
    chess::Coords currentCoords = {static_cast<uint8_t>(fromFile - '0'), static_cast<uint8_t>(fromRank - 'a')};
    chess::Coords targetCoords = {static_cast<uint8_t>(toFile - '0'), static_cast<uint8_t>(toRank - 'a')};
    chess::Piece pieceToMove = board[board.fromCoordsToPosition(currentCoords)];

    // moving the piece 
    board.movePiece(pieceToMove, targetCoords);

}


*/


