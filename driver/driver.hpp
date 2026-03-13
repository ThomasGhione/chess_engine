#ifndef DRIVER_HPP
#define DRIVER_HPP

#include <vector>
#include <string>
#include <iostream>
#include <filesystem>


#include "../uci/uci.hpp"

namespace driver {

    class Driver {

        public:

            struct Metadata {
                std::string id = "1.0.0";
                std::string license = "MIT License"; 
                std::string name = "HydraY! 1.0.0";
                std::string authors[3] = { "Thomas Ghione", 
                                           "Daniele Ferretti", 
                                           "Simone Tomasella" };
                std::string platforms[1] = { "Linux x86_64" }; // supported platforms
                // TODO: are these really needed?
                // size_t defaultThreads = 4;
                // size_t defaultTTSizeMB = 64;
                // bool debugMode = false;
            };
            Metadata metadata;

            constexpr static int32_t MAX_PARAM_LENGTH = 3;
            constexpr static int32_t MODE = 1;
            constexpr static int32_t COLOR = 2;
            constexpr static int32_t NO_ARGS = 1;

            engine::Engine& engine;

            Driver(engine::Engine& engine);

            void startGame(int argc, char *argv[]) noexcept;
            static std::string getBasicBoard(const chess::Board& board);
            
        private:

            bool vsBot = false;

            bool checkAndDownloadStockfish() noexcept;

            void parse(int argc, char *argv[]) noexcept;
            static bool parseColorOption(const char* colorArg, bool& outIsWhite) noexcept;
            static bool parseRequiredColorArg(int argc, char* argv[], const char* missingArgMessage, bool& outIsWhite) noexcept;
            static void printInvalidOption() noexcept;
            bool applyUciMoveToBoard(const std::string& uciMove, bool verboseDebug = false) noexcept;

            bool loadGame() noexcept;
            void saveGame() noexcept; // botColor: true = bot is white, false = bot is black
            void endGame() noexcept;
            void printGameOnFile() noexcept;
            
            static void quit(const std::string& input) noexcept;
            
            void playGameVsHuman() noexcept;
            void playGameVsEngine(const bool isWhite) noexcept;
            void botVsBot() noexcept;
            
            void playerTurn() noexcept;
            void engineTurn() noexcept;
            bool playOneTurn(bool playerTurn) noexcept;
            void playAlternatingTurns(bool firstPlayerTurn, bool secondPlayerTurn, bool printBoard) noexcept;

            static uint32_t mainMenu() noexcept;
			static uint32_t extraMenu() noexcept;
        	static uint32_t playWithEngineMenu() noexcept;

            static void clearScreen() noexcept;
    };
}

#endif
