#include "driver.hpp"

namespace driver {

    namespace {
        void waitForUserConfirmation() {
            // TODO: make this waiting also for Enter key
            
            char dummy;
            std::cin >> dummy; 
        }
    }

    bool Driver::checkAndDownloadStockfish() noexcept {
        // TODO: check if stockfish is already downloaded, if not, download it and place it in the correct folder
        
        if (!std::filesystem::exists("./stockfish")) {
            std::filesystem::create_directory("./stockfish");
        }

        #ifdef _WIN32
            
            if (!std::filesystem::exists("./stockfish/windows")) {
                std::filesystem::create_directory("./stockfish/windows");
            }

            std::string stockFishPathWindows = "./stockfish/windows/stockfish-windows-x86-64-avx2.exe";

            if (!std::filesystem::exists(stockFishPathWindows)) {
                std::cout << "Error: Stockfish not found in ./stockfish/windows/\nDownload the latest windows avx2 version on the official website and place it in the folder ./stockfish/windows/\nPress any key to continue...";
                waitForUserConfirmation();

                return false;
            }

            return true;
        
        #else

            if (!std::filesystem::exists("./stockfish/linux")) {
                std::filesystem::create_directory("./stockfish/linux");
            }

            std::string stockFishPathLinux = "./stockfish/linux/stockfish-ubuntu-x86-64-avx2";

            if (!std::filesystem::exists(stockFishPathLinux)) {
                std::cout << "Error: Stockfish not found in ./stockfish/linux/\nDownload the latest linux avx2 version on the official website and place it in the folder ./stockfish/linux/\nPress any key to continue...";
                waitForUserConfirmation();

                return false;
            }

            return true;

        #endif
    }
}