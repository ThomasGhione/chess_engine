#include "engine.hpp"

namespace engine {

    //! TODO Try to add support for multiple saves

    void Engine::saveGame() {
        
        bool fileExists;

        if (filesystem::exists("save.txt")) {
            fileExists = true;

            string ans;
            
            cout << "A save file has been detected, do you want to overwrite it? (Y/N) ";
            cin >> ans;

            if (ans == "N") return;
        }

        if (!fileExists) filesystem::remove("saves/save.txt");

        ofstream SaveFile("saves/save.txt");

        SaveFile << board.getCurrentFen(); 

        SaveFile.close();
    }
    
    bool Engine::loadGame() {
        ifstream SaveFile("saves/save.txt");
        if (!SaveFile.is_open()) {
            return false;
        }

        std::string line;
        
        if (std::getline(SaveFile, line)) {
            board = chess::Board(line);
        } 
        else {
            return false;
        }

        return true;
    }
}