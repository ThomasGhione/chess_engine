#include "engine.hpp"

namespace engine {

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