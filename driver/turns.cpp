#include "driver.hpp"

namespace driver {
    void Driver::playerTurn() noexcept{
        engine.getActiveColor() == chess::Board::WHITE ? std::cout << "\nWhite's turn.\n\n" : std::cout << "\nBlack's turn.\n\n";

        std::string playerInput;

        bool error = true;
        while (error) {
            const std::string currentBoard = Driver::getBasicBoard(engine.board);
            std::cout << currentBoard << "\n";

            std::cout << "Enter your move (write 's' to save the game or 'q' to quit): ";
            std::cin >> playerInput;

            if (playerInput == "s") [[unlikely]] {
                this->saveGame();
                continue;
            }

            if (playerInput == "q") [[unlikely]] {
                std::cout << "Thank you for playing! See you next time." << std::endl;
                exit(EXIT_SUCCESS);
            }

            if (playerInput.length() != 4 && playerInput.length() != 5) [[unlikely]] {
                std::cout << "Invalid move length. Please enter your move in the format 'e2e4' or 'e7e8q'.\n";
                continue;
            }

            const chess::Coords fromCoords(playerInput.substr(0, 2));
            const chess::Coords toCoords(playerInput.substr(2, 2));
        
            if (!chess::Coords::isInBounds(fromCoords) || !chess::Coords::isInBounds(toCoords)) [[unlikely]] {
                std::cout << "Invalid move format. Please enter your move in the format 'e2e4'.\n";
                continue;
            }

            const bool isWhiteTurn = (engine.getActiveColor() == chess::Board::WHITE);
            if (isWhiteTurn != (engine.board.getColor(fromCoords) == chess::Board::WHITE)) [[unlikely]] {
                std::cout << "It's not your turn to move that piece. Please enter a valid move.\n";
                continue;
            }

#ifdef DEBUG
            auto chrono_start = std::chrono::high_resolution_clock::now();
#endif  

            const uint8_t piece = engine.board.get(fromCoords);
            const uint8_t pieceType  = piece & chess::Board::MASK_PIECE_TYPE;
            const uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

            bool moveOk = false;

            // Optional promotion character (5th char): e7e8q, e2e1N, ...
            char promoChar = '\0';
            if (playerInput.length() == 5) {
                promoChar = static_cast<char>(std::tolower(static_cast<unsigned char>(playerInput[4])));
                if (promoChar != 'q' && promoChar != 'r' && promoChar != 'b' && promoChar != 'n') [[unlikely]] {
                    std::cout << "Invalid promotion piece. Use q, r, b or n.\n";
                    continue;
                }
            }

            const bool isPromotionCandidate =
                (pieceType == chess::Board::PAWN) &&
                ((pieceColor == chess::Board::WHITE && toCoords.rank() == 0) ||
                 (pieceColor == chess::Board::BLACK && toCoords.rank() == 7));

            if (isPromotionCandidate && playerInput.length() == 4) {
                // If user didn't specify, default to queen
                promoChar = 'q';
            }

            moveOk = engine.movePiece(fromCoords, toCoords, promoChar);
            if (!moveOk) [[unlikely]] {
                std::cout << "Invalid move. Please try again.\n";
                continue;
            }

#ifdef DEBUG
            auto chrono_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> elapsed = chrono_end - chrono_start;
            std::cout << "[DEBUG] move executed in " << elapsed.count() << " microseconds.\n";
#endif

            std::cout << "\n" << Driver::getBasicBoard(engine.board) << "\n";
            
            error = false;
        }  

        return;
    }

    void Driver::engineTurn() noexcept {
        std::cout << "Engine's thinking... \n";
#ifdef DEBUG
        auto chrono_start = std::chrono::high_resolution_clock::now();
#endif
        this->engine.search(engine::Engine::DEFAULTDEPTH);
#ifdef DEBUG
        auto chrono_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = chrono_end - chrono_start;
        std::cout << "[DEBUG] Engine search: " << elapsed.count() << "ms.\n";
        std::cout << "[DEBUG] Nodes visited: " << engine.nodesSearched << "\n";
#endif
    }
}
