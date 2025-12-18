#include "driver.hpp"

#include "../printer/menu.hpp"
#include "../engine/engine.hpp"

namespace driver {

    Driver::Driver(print::Menu m, engine::Engine e) 
        : menu(m)
        , engine(e) 
    {}

    void Driver::quit(std::string input){
        if(input=="Q"||input=="q"){
            std::cout << "Thank you for playing! See you next time." << std::endl;
            exit(EXIT_SUCCESS);
        }
    } 

    void Driver::startGame() {
        while (true) {
            uint8_t mainMenuChoice = menu.mainMenu();

            switch (mainMenuChoice) {
                
                case '1': {
                    uint8_t colorChoice = menu.playWithEngineMenu();
                    Driver::quit(std::string(1, colorChoice));
                    switch (colorChoice) {
                        case '1':
                            this->playGameVsEngine(true);
                            break;
                        case '2':
                            this->playGameVsEngine(false);
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
                    Driver::quit(std::string(1, twoPlayerChoice));

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
                    std::cout << "Thank you for playing! See you next time." << std::endl;
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


    void Driver::endGame() {
        if (this->engine.isCheckMate) {
            uint8_t nextColor = this->engine.board.getActiveColor();
            if (this->engine.board.isCheckmate(nextColor)) {
                std::cout << "\nCheckmate! "
                            << (nextColor == chess::Board::WHITE ? "Black" : "White")
                            << " wins.\n";
            } else if (this->engine.board.isStalemate(nextColor)) {
                std::cout << "\nStalemate. Game drawn.\n";
            }
            std::cout << "Press Enter to return to the menu...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cin.get();

            engine.reset();
        } 
    }


    void Driver::playGameVsHuman() {
    	while(!engine.isCheckMate) {
    	    //! It doesn't check for loaded games, we should fix it later based on the activeColor in board
            this->playerTurn();
            if (engine.isCheckMate) { endGame(); return; }

            this->playerTurn();
            if (engine.isCheckMate) { endGame(); return; }
    	}
    }

    void Driver::playGameVsEngine(bool isHumanWhite) {
        if (isHumanWhite) {
            while (!engine.isCheckMate) {
                this->playerTurn();
                if (engine.isCheckMate) { endGame(); return; }
                
                this->engineTurn();
                if (engine.isCheckMate) { endGame(); return; }
            }
        } 
        else {
            while (!engine.isCheckMate) {
                this->engineTurn();
                if (engine.isCheckMate) { endGame(); return; }

                this->playerTurn();
                if (engine.isCheckMate) { endGame(); return; }
            } 
        }
    }


    void Driver::playerTurn() {
        engine.board.getActiveColor() == chess::Board::WHITE ? std::cout << "\nWhite's turn.\n\n" : std::cout << "\nBlack's turn.\n\n";

        std::string playerInput;

        bool isWhiteTurn = (engine.board.getActiveColor() == chess::Board::WHITE);

        bool error = true;
        while (error) {
            std::string currentBoard = print::Prints::getBasicBoard(engine.board);
            std::cout << currentBoard << "\n";

            std::cout << "Enter your move (write 's' to save the game or 'q' to quit): ";
            std::cin >> playerInput;

            //! TODO Check if player wants to save or quit
            /*
            if (playerInput == "q") {
                this->saveGame();
                return;
            }
            */

            if (playerInput == "q") {
                std::cout << "Thank you for playing! See you next time." << std::endl;
                exit(EXIT_SUCCESS);
            }

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

            /*
            std::cout << "[DEBUG] fromCoords: " << fromCoords.toString() << " (index=" << (int)fromCoords.index
                      << ", rank=" << (int)fromCoords.rank() << ", file=" << (int)fromCoords.file() << ")\n";
            std::cout << "[DEBUG] piece at from: " << (int)piece << "\n";
            std::cout << "[DEBUG] activeColor: " << (int)engine.board.getActiveColor() << " (WHITE=0, BLACK=8)\n";
            std::cout << "[DEBUG] isWhiteTurn: " << (isWhiteTurn ? "true" : "false") << "\n";
            std::cout << "[DEBUG] getColor(from): " << (int)engine.board.getColor(fromCoords) << "\n";
            */

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
                ((pieceColor == chess::Board::WHITE && toCoords.rank() == 0) ||
                 (pieceColor == chess::Board::BLACK && toCoords.rank() == 7));

            if (isPromotionCandidate) {
                // If user didn't specify, default to queen
                if (playerInput.length() == 4) {
                    promoChar = 'q';
                }
                moveOk = engine.board.moveBB(fromCoords, toCoords, promoChar);
            } 
            else {
                moveOk = engine.board.moveBB(fromCoords, toCoords);
            }

            if (!moveOk) {
                std::cout << "Invalid move. Please try again.\n";
                continue;
            }

    #ifdef DEBUG
            auto chrono_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> elapsed = chrono_end - chrono_start;
            std::cout << "[DEBUG] MoveBB executed in " << elapsed.count() << " microseconds.\n";
    #endif

            // Print the updated board after successful move
            std::cout << "[DEBUG] About to print updated board...\n";
            std::cout << "\n" << print::Prints::getBasicBoard(engine.board) << "\n";
            std::cout << "[DEBUG] Board printed successfully.\n";

            // TODO To be eliminated because Engine will handle it in future
            engine.setIsCheckMate();

            error = false;
        }  

        return;
    }

    void Driver::engineTurn() {
        std::cout << "Engine's thinking... \n";
#ifdef DEBUG
                auto chrono_start = std::chrono::high_resolution_clock::now();
#endif
        engine.search(engine.depth);
#ifdef DEBUG
                auto chrono_end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = chrono_end - chrono_start;
                std::cout << "[DEBUG] Engine search: " << elapsed.count() << "ms.\n";
                std::cout << "[DEBUG] Nodes visited: " << engine.nodesSearched << "\n";
#endif
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
}