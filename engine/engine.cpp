#include "engine.hpp"

namespace engine {

Engine::Engine(){
  //this->board = chess::Board();
}

void Engine::startGame(){
  std::cout << print::Menu::getMainMenu();

  uint8_t numberOfPlayers = print::Menu::getPlayerInput();

  if(numberOfPlayers == 0){
    exit(0);
  }

  if(numberOfPlayers == 1){
    //std::cout << "Not available in this moment" << std::endl;
    std::cout << "Not available in this moment" << std::endl;
    return;
  }

  this->playGameVsHuman();
}

void Engine::playGameVsHuman() {
    while (!this->isMate()) {
        std::cout << "It's white's turn: ";
        takePlayerTurn();

        std::cout << "It's black's turn: ";
        takePlayerTurn();
    }
}

bool Engine::isMate(){
  return true;
}

}
/*
void Engine::playGameVsEngine(bool isWhite) {
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
}*/
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


bool Engine::loadGame() {
    std::ifstream SaveFile("saves/save.txt");
    if (!SaveFile.is_open()) {
        return false;
    }
    
    std::string line;
    
    if (std::getline(SaveFile, line)) {
        board = chess::Board(line); // TODO aggiungere controlli/eccezioni per il fen
    } 
    else {
        return false;
    }
    
    return true;
}
*/
