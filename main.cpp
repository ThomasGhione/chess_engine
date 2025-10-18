#include "./engine/engine.hpp"
#include "./driver/driver.hpp"

using namespace chess;
using namespace print;
using namespace engine;
using namespace driver;

int main() {
/*
    Menu menu = Menu();
    Engine engine = Engine();

    Driver driver = Driver(menu, engine);

    driver.startGame(); */

  /*
      // 1. CREAZIONE BOARD VUOTA
    Board board;
    std::cout << "1. Creazione board vuota:\n";
    std::cout << Prints::getBasicBoard(board);
    std::cout << "Dimensione board: " << Board::size() << " byte\n\n";

    // 2. SETUP POSIZIONE INIZIALE
    std::cout << "2. Setup posizione iniziale:\n";
    
    // Pedoni
    for (int col = 0; col < 8; ++col) {
        board.set(1, col, Board::BLACK | Board::PAWN);   // Pedoni neri
        board.set(6, col, Board::WHITE | Board::PAWN);   // Pedoni bianchi
    }
    
    // Pezzi neri
    board.set(0, 0, Board::BLACK | Board::ROOK);
    board.set(0, 1, Board::BLACK | Board::KNIGHT);
    board.set(0, 2, Board::BLACK | Board::BISHOP);
    board.set(0, 3, Board::BLACK | Board::QUEEN);
    board.set(0, 4, Board::BLACK | Board::KING);
    board.set(0, 5, Board::BLACK | Board::BISHOP);
    board.set(0, 6, Board::BLACK | Board::KNIGHT);
    board.set(0, 7, Board::BLACK | Board::ROOK);
    
    // Pezzi bianchi
    board.set(7, 0, Board::WHITE | Board::ROOK);
    board.set(7, 1, Board::WHITE | Board::KNIGHT);
    board.set(7, 2, Board::WHITE | Board::BISHOP);
    board.set(7, 3, Board::WHITE | Board::QUEEN);
    board.set(7, 4, Board::WHITE | Board::KING);
    board.set(7, 5, Board::WHITE | Board::BISHOP);
    board.set(7, 6, Board::WHITE | Board::KNIGHT);
    board.set(7, 7, Board::WHITE | Board::ROOK);
    
    std::cout << Prints::getBasicBoard(board);

    // 3. ACCESSO PER COORDINATE
    std::cout << "3. Accesso per coordinate:\n";
    std::cout << "Casella e2: " << char(board.get(1, 4) + '0') << " (pedone nero)\n";
    std::cout << "Casella e7: " << char(board.get(6, 4) + '0') << " (pedone bianco)\n";
    std::cout << "Re nero in e8: " << char(board.get(0, 4) + '0') << "\n\n";

    // 4. ACCESSO PER INDICE LINEARE
    std::cout << "4. Accesso per indice lineare:\n";
    std::cout << "Indice 0 (a8): " << char(board[0] + '0') << " (torre nera)\n";
    std::cout << "Indice 63 (h1): " << char(board[63] + '0') << " (torre bianca)\n\n";

    // 5. OPERAZIONI BULK CON LE RIGHE
    std::cout << "5. Operazioni bulk con le righe:\n";
    std::cout << "Riga 0 (neri): 0x" << std::hex << board.get_row(0) << std::dec << "\n";
    std::cout << "Riga 7 (bianchi): 0x" << std::hex << board.get_row(7) << std::dec << "\n\n";

    // 6. SIMULAZIONE MOSSA
    std::cout << "6. Simulazione mossa e2-e4:\n";
    
    // Salva stato prima della mossa
    Board board_before = board;
    
    // Muovi pedone nero da e2 a e4
    board.set(1, 4, Board::EMPTY);  // Libera e2
    board.set(3, 4, Board::BLACK | Board::PAWN);  // Metti pedone in e4
    
    std::cout << Prints::getBasicBoard(board);

    // 7. CONFRONTO BOARD
    std::cout << "7. Confronto board:\n";
    std::cout << "Board uguali a prima della mossa? " << (board == board_before ? "Sì" : "No") << "\n";
    std::cout << "Board diverse da prima della mossa? " << (board != board_before ? "Sì" : "No") << "\n\n";

    // 8. SCANSIONE DI TUTTA LA BOARD
    std::cout << "8. Scansione di tutta la board:\n";
    int total_pieces = 0;
    for (int i = 0; i < 64; ++i) {
        if (board[i] != Board::EMPTY) {
            total_pieces++;
        }
    }
    std::cout << "Pezzi totali sulla board: " << total_pieces << "\n\n";

    // 10. USO ITERATORI
    std::cout << "10. Uso iteratori per somma valori:\n";
    uint32_t sum = 0;
    for (auto row_value : board) {
        sum += row_value;
    }
    std::cout << "Somma di tutti i valori delle righe: " << sum << " (dovrebbe essere 0)\n";
*/
  engine::Engine e = engine::Engine();
  e.startGame();
  return 0;
}
