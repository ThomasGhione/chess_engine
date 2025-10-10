#include "engine.hpp"

namespace engine {

void Engine::playGameVsEngine(bool isWhite) {
    while (!isMate()) {
        if (isHumanWhite) {
            std::cout << "It's your turn: ";
            takePlayerTurn();
            std::cout << "Engine's thinking... ";
            takeEngineTurn();
        }
    }
}

void Engine::takeEngineTurn() {
    //! EVERYTHING'S A PLACEHOLDER
    
    evaluate();

    chess::Coords currentCoords;
    chess::Coords targetCoords;
    chess::Piece pieceToMove;

    board.movePiece(pieceToMove, targetCoords);
}

void Engine::playGameVsHuman() {
    while (!isMate()) {
        std::cout << "It's white's turn: ";
        takePlayerTurn();
        std::cout << "It's black's turn: ";
        takePlayerTurn();
    }
    // TODO: stampare il vincitore e ristampare il menu
}

void Engine::takePlayerTurn() {
    // reading input
    std::cout << "Insert the coords: ";
    char fromFile, fromRank, toFile, toRank;
    std::cin >> fromFile >> fromRank >> toFile >> toRank;
    
    // setting variables up
    chess::Coords currentCoords = {fromFile, fromRank};
    chess::Coords targetCoords = {toFile, toRank};
    chess::Piece pieceToMove = board[board.fromCoordsToPosition(currentCoords)];

    // moving the piece 
    board.movePiece(pieceToMove, targetCoords);
}


//! TODO Try to add support for multiple saves
void Engine::saveGame() {

    if (filesystem::exists("save.txt")) {
        char ans;
        
        cout << "A save file has been detected, do you want to overwrite it? (Y/N) ";
        cin >> ans;
        if (ans == 'Y' || ans == 'y') {
            filesystem::remove("saves/save.txt");
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
}