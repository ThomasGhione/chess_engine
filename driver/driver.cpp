#include "driver.hpp"

#include "../printer/menu.hpp"
#include "../engine/engine.hpp"

namespace driver {

    Driver::Driver(print::Menu m, engine::Engine e) 
        : menu(m)
        , engine(e) 
    {}

    void Driver::startGame() {
        while (true) {
            uint8_t mainMenuChoice = menu.mainMenu();

            switch (mainMenuChoice) {
                
                case '1': {
                    uint8_t colorChoice = menu.playWithEngineMenu();
                    switch (colorChoice) {
                        case '1':
                            this->playGameVsEngine(true); // TODO stampare il risultato finale
                            break;
                        case '2':
                            this->playGameVsEngine(false); // TODO stampare il risultato finale
                            break;
                        case '3':
                            loadGame(false);
                            break;
                        case '4':
                            // Back to main menu
                            break;
                        default:
                            std::cout << "Invalid option. Please select a valid option.\n";
                            break;
                    }
                    break;
                }

                case '2': {
                    uint8_t twoPlayerChoice = menu.playWithPlayerMenu();
                    switch (twoPlayerChoice) {
                        case '1':
                            this->playGameVsHuman();
                            break;
                        case '2':
                            loadGame(true);
                            break;
                        case '3':
                            // Back to main menu
                            break;
                        default:
                            std::cout << "Invalid option. Please select a valid option.\n";
                            break;
                    }
                    break;
                }
                
                case '3':
                    std::cout << "\nThank you for playing! Goodbye!\n";
                    exit(EXIT_SUCCESS);
                    break;

                default:
                    std::cout << "Invalid option. Please select a valid option.\n";
                    break;
            }
        } 
    }

    bool Driver::loadGame(bool isWithPlayer) {
        std::ifstream SaveFile("saves/save.txt");
        if (!SaveFile.is_open()) {
            std::cerr << "Error: Unable to open save file.\n";
            return false;
        }

        std::string line;
    
        if (std::getline(SaveFile, line)) {
            this->engine.board = chess::Board(line);
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
                this->engine.isPlayerWhite = true;
            } else {
                this->engine.isPlayerWhite = false;
            }

            this->playGameVsEngine(this->engine.isPlayerWhite);
        }

        return true;
    }

    void Driver::saveGame() {
    /*    
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
        SaveFile << engine.board.getCurrentFen();
        SaveFile.close();
    */
    }


    void Driver::printFinalResult() {
        if (this->engine.isMate()) {
            uint8_t nextColor = this->engine.board.getActiveColor();
            if (this->engine.board.isCheckmate(nextColor)) {
                std::cout << "\nCheckmate! "
                            << (nextColor == chess::Board::WHITE ? "Black" : "White")
                            << " wins.\n";
            } else if (this->engine.board.isStalemate(nextColor)) {
                std::cout << "\nStalemate. Game drawn.\n";
            }
        } 
    }


    void Driver::playGameVsHuman() {
    	while(!engine.isMate()) {
    	    //! It doesn't check for loaded games, we should fix it later based on the activeColor in board

    	    // std::cout << print::Prints::getPrintableBoard( this->board.getCurrentPositionFen() ) << std::endl;

            std::cout << "It's white's turn: \n\n";
            
            this->takePlayerTurn();
            
            if (engine.isMate()) break;

            // std::cout << print::Prints::getPrintableBoard( this->board.getCurrentPositionFen() ) << std::endl;
            std::cout << "It's black's turn: \n\n";
            
            this->takePlayerTurn();

            if (engine.isMate()) break;
    	}
    }




    void Driver::takePlayerTurn() {
        std::string playerInput;

        bool isWhiteTurn = (engine.board.getActiveColor() == chess::Board::WHITE);
        std::string currentBoard = print::Prints::getBasicBoard(engine.board);

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

            if (playerInput.length() != 4 && playerInput.length() != 5) {
                std::cout << "Invalid move length. Please enter your move in the format 'e2e4' or 'e7e8q'.\n";
                continue;
            }

            chess::Coords fromCoords(playerInput.substr(0, 2));
            chess::Coords toCoords(playerInput.substr(2, 2));
        
            if (!chess::Coords::isInBounds(fromCoords) || !chess::Coords::isInBounds(toCoords)) {
                std::cout << "Invalid move format. Please enter your move in the format 'e2e4'.\n";
                continue;
            }
        
            uint8_t piece = engine.board.get(fromCoords);
        

            if (piece == chess::Board::EMPTY) {
                std::cout << "There is no piece at the source square. Please enter a valid move.\n";
                continue;
            }

            if (isWhiteTurn != (engine.board.getColor(fromCoords) == chess::Board::WHITE)) {
                std::cout << "It's not your turn to move that piece. Please enter a valid move.\n";
                continue;
            }

            // TODO: check whether it's redundant or not
            if (engine.board.isSameColor(fromCoords, toCoords)) {
                std::cout << "You cannot move to a square occupied by your own piece.\n";
                continue;
            }

    #ifdef DEBUG
            auto chrono_start = std::chrono::high_resolution_clock::now();
    #endif  

            const uint8_t pieceType  = piece & chess::Board::MASK_PIECE_TYPE;
            const uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

            bool moveOk = false;

            // Optional promotion character (5th char): e7e8q, e2e1N, ...
            char promoChar = 'q';
            if (playerInput.length() == 5) {
                promoChar = static_cast<char>(std::tolower(static_cast<unsigned char>(playerInput[4])));
                if (promoChar != 'q' && promoChar != 'r' && promoChar != 'b' && promoChar != 'n') {
                    std::cout << "Invalid promotion piece. Use q, r, b or n.\n";
                    continue;
                }
            }

            const bool isPromotionCandidate =
                (pieceType == chess::Board::PAWN) &&
                ((pieceColor == chess::Board::WHITE && toCoords.rank == 7) ||
                 (pieceColor == chess::Board::BLACK && toCoords.rank == 0));

            if (isPromotionCandidate) {
                // If user didn't specify, default to queen
                if (playerInput.length() == 4) {
                    promoChar = 'q';
                }
                moveOk = engine.board.moveBB(fromCoords, toCoords, promoChar);
            } else {
                moveOk = engine.board.moveBB(fromCoords, toCoords);
            }

            if (!moveOk) {
                std::cout << "Invalid move. Please try again.";
                continue;
            }

    #ifdef DEBUG
            auto chrono_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> elapsed = chrono_end - chrono_start;
            std::cout << "[DEBUG] MoveBB executed in " << elapsed.count() << " microseconds.\n";
    #endif
        
            // After successful move, detect terminal state for next side to move
            uint8_t nextColor = engine.board.getActiveColor();
            if (engine.board.isCheckmate(nextColor)) {
                std::cout << "\nCheckmate! " << (nextColor == chess::Board::WHITE ? "Black" : "White") << " wins.\n";
                return; // exit turn early
            }
            if (engine.board.isStalemate(nextColor)) {
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


    void Driver::playGameVsEngine(bool isHumanWhite) {
        isHumanWhite ? this->HumanFirst() : this->EngineFirst();
    }

    void Driver::EngineFirst() {
        while (!engine.isMate()) {
            std::cout << "Engine's thinking... \n";
            
#ifdef DEBUG
            auto chrono_start = std::chrono::high_resolution_clock::now();
#endif              
            engine.search(engine.depth);
#ifdef DEBUG
            auto chrono_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = chrono_end - chrono_start;
            std::cout << "[DEBUG] Engine search took " << elapsed.count() << " ms.\n";
            std::cout << "[DEBUG] Nodes searched so far: " << engine.nodesSearched << "\n";
#endif

            if (engine.isMate()) break;

            std::cout << "It's your turn: \n\n";
            this->takePlayerTurn();
        }
    }

    void Driver::HumanFirst() {
        while (!engine.isMate()) {
            std::cout << "It's your turn: \n\n";
            this->takePlayerTurn();

            if (engine.isMate()) break;

            std::cout << "Engine's thinking... \n";
            
#ifdef DEBUG
            auto chrono_start = std::chrono::high_resolution_clock::now();
#endif    
            engine.search(engine.depth);
#ifdef DEBUG
            auto chrono_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = chrono_end - chrono_start;
            std::cout << "[DEBUG] Engine search took " << elapsed.count() << " ms.\n";
            std::cout << "[DEBUG] Nodes searched so far: " << engine.nodesSearched << "\n";
#endif
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
}