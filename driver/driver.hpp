#ifndef DRIVER_HPP
#define DRIVER_HPP

#include <vector>
#include <string>

#include "../printer/menu.hpp"
#include "../engine/engine.hpp"
#include "../uci/uci.hpp"

namespace driver {

    class Driver {

        public:

            struct Metadata {
                std::string id = "beta-0.1.0";
                std::string license = "MIT License"; 
                std::string name = "Unnamed Chess Engine! Beta Version";
                std::string authors[3] = { "Thomas Ghione", 
                                           "Daniele Ferretti", 
                                           "Simone Tomasella" };
                std::string platforms[1] = { "Linux x86_64" }; // supported platforms
                // TODO: servono davvero?
                // size_t defaultThreads = 4;
                // size_t defaultTTSizeMB = 64;
                // bool debugMode = false;
            };
            Metadata metadata;

            constexpr static int32_t MAX_PARAM_LENGTH = 3;
            constexpr static int32_t MODE = 1;
            constexpr static int32_t COLOR = 2;
            constexpr static int32_t NO_ARGS = 1;


            print::Menu menu;
            engine::Engine& engine;  // Cambiato da copia a riferimento

            Driver(print::Menu& menu, engine::Engine& engine);  // Passaggio per riferimento

            void startGame(int argc, char *argv[]) noexcept;

        private:

            bool vsBot = false;

            void parse(int argc, char *argv[]) noexcept;
            static bool parseColorOption(const char* colorArg, bool& outIsWhite) noexcept;
            static bool parseRequiredColorArg(int argc, char* argv[], const char* missingArgMessage, bool& outIsWhite) noexcept;
            static void printInvalidOption() noexcept;
            bool applyUciMoveToBoard(const std::string& uciMove, bool verboseDebug = false) noexcept;

            bool loadGame() noexcept;
            void saveGame() noexcept; // botColor: true = bot is white, false = bot is black
            void endGame() noexcept;
            void printGameOnFile() noexcept;
            
            void quit(const std::string& input) noexcept;
            
            void playGameVsHuman() noexcept;
            void playGameVsEngine(const bool isWhite) noexcept;
            void botVsBot() noexcept;
            void botVsStockfish(const bool botColor) noexcept;
            void betaVsAlpha(const bool betaIsWhite) noexcept;
            
            void playerTurn() noexcept;
            void engineTurn() noexcept;
            bool playOneTurn(bool playerTurn) noexcept;
            void playAlternatingTurns(bool firstPlayerTurn, bool secondPlayerTurn, bool printBoard) noexcept;

    };
}

#endif
