#include "../engine.hpp"
#include "../../tests/ut.hpp"
#include <random>
#include <vector>
#include <string>
#include <ctime>

namespace ut = boost::ut;

namespace {

  /*
  // ==================== HELPER FUNCTIONS ====================

  std::mt19937& getRandomGenerator() {
    static std::mt19937 gen(std::time(nullptr));
    return gen;
  }

  uint8_t randomSquare() {
    std::uniform_int_distribution<> dis(0, 63);
    return static_cast<uint8_t>(dis(getRandomGenerator()));
  }

  bool areKingsValid(uint8_t whiteKing, uint8_t blackKing) {
    if (whiteKing == blackKing) return false;
    int wFile = whiteKing & 7;
    int wRank = whiteKing >> 3;
    int bFile = blackKing & 7;
    int bRank = blackKing >> 3;
    int fileDiff = std::abs(wFile - bFile);
    int rankDiff = std::abs(wRank - bRank);
    return !(fileDiff <= 1 && rankDiff <= 1);
  }

  std::pair<uint8_t, uint8_t> placeKingsRandomly() {
    uint8_t whiteKing = randomSquare();
    uint8_t blackKing = randomSquare();

    while (!areKingsValid(whiteKing, blackKing)) {
      blackKing = (blackKing + 1) % 64;
    }

    return {whiteKing, blackKing};
  }

  std::string buildFEN(uint8_t whiteKing, uint8_t blackKing, const std::vector<std::pair<uint8_t, char>>& pieces) {
    std::array<char, 64> boardArray;
    boardArray.fill('.');

    boardArray[whiteKing] = 'K';
    boardArray[blackKing] = 'k';

    for (const auto& [square, piece] : pieces) {
      boardArray[square] = piece;
    }

    std::string fen;
    for (int rank = 0; rank < 8; ++rank) {
      int emptyCount = 0;
      for (int file = 0; file < 8; ++file) {
        int index = rank * 8 + file;
        if (boardArray[index] == '.') {
          emptyCount++;
        } else {
          if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
            emptyCount = 0;
          }
          fen += boardArray[index];
        }
      }
      if (emptyCount > 0) fen += std::to_string(emptyCount);
      if (rank < 7) fen += '/';
    }
    fen += " w - - 0 1";
    return fen;
  }

  bool isBoardLegal(const chess::Board& board) {
    if (board.getActiveColor() == chess::Board::WHITE) {
      uint64_t blackKingBB = board.kings_bb[1];
      if (blackKingBB == 0) return false;
      uint8_t blackKingPos = __builtin_ctzll(blackKingBB);
      return !board.isSquareAttacked(blackKingPos, chess::Board::WHITE);
    }
    return true;
  }

  bool tryAddPieceAtSquare(uint8_t whiteKing, uint8_t blackKing,
                           std::vector<std::pair<uint8_t, char>>& pieces,
                           uint8_t square, char pieceType) {
    pieces.push_back({square, pieceType});

    std::string fen = buildFEN(whiteKing, blackKing, pieces);
    chess::Board board(fen);

    if (isBoardLegal(board)) {
      return true;
    } else {
      pieces.pop_back();
      return false;
    }
  }

  void addRook(uint8_t whiteKing, uint8_t blackKing,
               std::vector<std::pair<uint8_t, char>>& pieces,
               std::vector<uint8_t>& occupied) {
    while (true) {
      uint8_t square = randomSquare();
      while (std::find(occupied.begin(), occupied.end(), square) != occupied.end()) {
        square = (square + 1) % 64;
      }

      if (tryAddPieceAtSquare(whiteKing, blackKing, pieces, square, 'R')) {
        occupied.push_back(square);
        return;
      }
    }
  }

  void addQueen(uint8_t whiteKing, uint8_t blackKing,
                std::vector<std::pair<uint8_t, char>>& pieces,
                std::vector<uint8_t>& occupied) {
    while (true) {
      uint8_t square = randomSquare();
      while (std::find(occupied.begin(), occupied.end(), square) != occupied.end()) {
        square = (square + 1) % 64;
      }

      if (tryAddPieceAtSquare(whiteKing, blackKing, pieces, square, 'Q')) {
        occupied.push_back(square);
        return;
      }
    }
  }

  void addKnight(uint8_t whiteKing, uint8_t blackKing,
                 std::vector<std::pair<uint8_t, char>>& pieces,
                 std::vector<uint8_t>& occupied) {
    while (true) {
      uint8_t square = randomSquare();
      while (std::find(occupied.begin(), occupied.end(), square) != occupied.end()) {
        square = (square + 1) % 64;
      }

      if (tryAddPieceAtSquare(whiteKing, blackKing, pieces, square, 'N')) {
        occupied.push_back(square);
        return;
      }
    }
  }

  void addBishop(uint8_t whiteKing, uint8_t blackKing,
                 std::vector<std::pair<uint8_t, char>>& pieces,
                 std::vector<uint8_t>& occupied,
                 bool lightSquare) {
    int targetParity = lightSquare ? 0 : 1;

    while (true) {
      uint8_t square = randomSquare();

      // Find square with correct color
      while (true) {
        int sum = (square & 7) + (square >> 3);
        if ((sum % 2) == targetParity && std::find(occupied.begin(), occupied.end(), square) == occupied.end()) {
          break;
        }
        square = (square + 1) % 64;
      }

      if (tryAddPieceAtSquare(whiteKing, blackKing, pieces, square, 'B')) {
        occupied.push_back(square);
        return;
      }
    }
  }

  // ==================== POSITION GENERATORS ====================

  chess::Board generatePositionKR() {
    auto [whiteKing, blackKing] = placeKingsRandomly();
    std::vector<uint8_t> occupied = {whiteKing, blackKing};
    std::vector<std::pair<uint8_t, char>> pieces;

    addRook(whiteKing, blackKing, pieces, occupied);

    std::string fen = buildFEN(whiteKing, blackKing, pieces);
    return chess::Board(fen);
  }

  chess::Board generatePositionK2R() {
    auto [whiteKing, blackKing] = placeKingsRandomly();
    std::vector<uint8_t> occupied = {whiteKing, blackKing};
    std::vector<std::pair<uint8_t, char>> pieces;

    addRook(whiteKing, blackKing, pieces, occupied);
    addRook(whiteKing, blackKing, pieces, occupied);

    std::string fen = buildFEN(whiteKing, blackKing, pieces);
    return chess::Board(fen);
  }

  chess::Board generatePositionKQ() {
    auto [whiteKing, blackKing] = placeKingsRandomly();
    std::vector<uint8_t> occupied = {whiteKing, blackKing};
    std::vector<std::pair<uint8_t, char>> pieces;

    addQueen(whiteKing, blackKing, pieces, occupied);

    std::string fen = buildFEN(whiteKing, blackKing, pieces);
    return chess::Board(fen);
  }

  chess::Board generatePositionK2B() {
    auto [whiteKing, blackKing] = placeKingsRandomly();
    std::vector<uint8_t> occupied = {whiteKing, blackKing};
    std::vector<std::pair<uint8_t, char>> pieces;

    addBishop(whiteKing, blackKing, pieces, occupied, true);   // Light square
    addBishop(whiteKing, blackKing, pieces, occupied, false);  // Dark square

    std::string fen = buildFEN(whiteKing, blackKing, pieces);
    return chess::Board(fen);
  }

  chess::Board generatePositionKNBLight() {
    auto [whiteKing, blackKing] = placeKingsRandomly();
    std::vector<uint8_t> occupied = {whiteKing, blackKing};
    std::vector<std::pair<uint8_t, char>> pieces;

    addKnight(whiteKing, blackKing, pieces, occupied);
    addBishop(whiteKing, blackKing, pieces, occupied, true);  // Light square

    std::string fen = buildFEN(whiteKing, blackKing, pieces);
    return chess::Board(fen);
  }

  chess::Board generatePositionKNBDark() {
    auto [whiteKing, blackKing] = placeKingsRandomly();
    std::vector<uint8_t> occupied = {whiteKing, blackKing};
    std::vector<std::pair<uint8_t, char>> pieces;

    addKnight(whiteKing, blackKing, pieces, occupied);
    addBishop(whiteKing, blackKing, pieces, occupied, false);  // Dark square

    std::string fen = buildFEN(whiteKing, blackKing, pieces);
    return chess::Board(fen);
  }
  */

  // ==================== COMMON MATE FINDER ====================

  bool findMate(chess::Board board, int maxHalfMoves = 100, int searchDepth = 10) {
    engine::Engine e(board.getCurrentFen());
    e.depth = searchDepth;

    for (int ply = 0; ply < maxHalfMoves; ++ply) {
      e.updateGameResult();
      if (e.isMate()) {
        return true;
      }

      uint8_t currentColor = e.board.getActiveColor();
      if (e.board.isStalemate(currentColor)) {
        printf("Stalemate at half-move %d\n", ply);
        return false;
      }

      auto moves = e.generateLegalMoves(e.board);
      if (moves.size == 0) {
        e.updateGameResult();
        bool isMate = e.isMate();
        if (!isMate) {
          printf("No legal moves but not checkmate at half-move %d\n", ply);
        }
        return isMate;
      }

      bool whiteToMove = (e.board.getActiveColor() == chess::Board::WHITE);
      chess::Board::Move bestMove = e.getBestMove(moves, whiteToMove);

      chess::Board::MoveState state;
      e.board.doMove(bestMove, state);
    }

    printf("Max moves (%d) reached without checkmate\n", maxHalfMoves);
    return false;
  }

  // ==================== TEST HELPER ====================

  bool runEndgameTest(const std::string& testName, chess::Board (*generator)(), int iteration) {
    //printf("\n[%s - Iteration %d]\n", testName.c_str(), iteration);

    chess::Board board = generator();
    bool foundMate = findMate(board);

    ut::expect(foundMate) << "Test fallito: "
      << testName << "FEN: " << board.getCurrentFen() << "\n";

    return foundMate;
  }
}

// ==================== TEST SUITE ====================

ut::suite EndingGameSuite = [] {
  using namespace ut;

  /*
  // Questi test li rimettiamo quando passiamo quelli statici.
  

  "Endgame: K+R vs K"_test = [] {
    for (int i = 1; i <= 5; ++i) {
      bool hasToStop = runEndgameTest("K+R vs K", generatePositionKR, i);
      if(hasToStop){
        break;
      }
    }
  };

  "Endgame: K+2R vs K"_test = [] {
    for (int i = 1; i <= 5; ++i) {
      bool hasToStop =runEndgameTest("K+2R vs K", generatePositionK2R, i);
      if(hasToStop){
        break;
      }
    }
  };

  "Endgame: K+Q vs K"_test = [] {
    for (int i = 1; i <= 5; ++i) {
      bool hasToStop =runEndgameTest("K+Q vs K", generatePositionKQ, i);
      if(hasToStop){
        break;
      }
    }
  };

  "Endgame: K+2B vs K"_test = [] {
    for (int i = 1; i <= 5; ++i) {
      bool hasToStop =runEndgameTest("K+2B vs K", generatePositionK2B, i);
      if(hasToStop){
        break;
      }
    }
  };

  "Endgame: K+N+B(light) vs K"_test = [] {
    for (int i = 1; i <= 5; ++i) {
      bool hasToStop =runEndgameTest("K+N+B(light) vs K", generatePositionKNBLight, i);
      if(hasToStop){
        break;
      }
    }
  };

  "Endgame: K+N+B(dark) vs K"_test = [] {
    for (int i = 1; i <= 5; ++i) {
      bool hasToStop =runEndgameTest("K+N+B(dark) vs K", generatePositionKNBDark, i);
      if(hasToStop){
        break;
      }
    }
  };
  */

  "Queen endgame"_test = []{
    const std::string fen = "K7/8/7Q/8/8/8/6k1/8 w - - 0 1 ";
    chess::Board board(fen);

    bool foundMate = findMate(board);

    ut::expect(foundMate)
      << "Queen endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };

  
  "Rook endgame"_test = []{
    const std::string fen = "8/8/8/8/8/4k3/7R/6K1 w - - 0 1";
    chess::Board board(fen);

    bool foundMate = findMate(board);

    ut::expect(foundMate)
      << "Rook endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };
  "Double rook endgame"_test = []{
    const std::string fen = "8/8/8/8/8/4k3/R6R/6K1 w - - 0 1";
    chess::Board board(fen);

    bool foundMate = findMate(board);

    ut::expect(foundMate)
      << "Double Rook endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };


  "Knight and bishop endgame"_test = []{
    const std::string fen = "8/8/8/8/8/4k3/8/2N1B1K1 w - - 0 1";
    chess::Board board(fen);

    bool foundMate = findMate(board);

    ut::expect(foundMate)
      << "Knight and bishop endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };

  "Double bishop endgame"_test = []{
    const std::string fen = "8/8/8/8/8/4k3/8/4BBK1 w - - 0 1";
    chess::Board board(fen);

    bool foundMate = findMate(board);

    ut::expect(foundMate)
      << "Double bishop endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };

  "critical position 7, avoid stallmate"_test = []{
    const std::string fen = "6k1/1pp2pp1/3p2p1/p5K1/r7/8/8/8 b - - 1 34";
    chess::Board board(fen);

    bool foundMate = findMate(board);

    ut::expect(foundMate)
      << "Rook and some pawn: "
      << "Failed to find checkmate within 50 moves (100 plies). [expect mate in 6]\n";
  };

  "Mate in 17 moves"_test = []{
    const std::string fen = "8/8/5k2/4pp2/R5p1/2P1K3/PP4PP/8 b - - 0 37";
    chess::Board board(fen);

    bool foundMate = findMate(board);

    ut::expect(foundMate)
      << "Rook and some pawn: "
      << "Failed to find checkmate within 50 moves (100 plies). [expect mate in 17]\n";
  };

  "Mate in 5 moves"_test = []{
    const std::string fen = "1kbr4/pn3R2/2p3P1/2p1N2p/7P/P1N5/BPP2PP1/2K5 w - - 1 36";
    chess::Board board(fen);

    bool foundMate = findMate(board);

    ut::expect(foundMate)
      << "Rook and Knights: "
      << "Failed to find checkmate within 50 moves (100 plies). [expect mate in 5]\n";
  };

  "Queen vs rook and bishop"_test = []{
    const std::string fen = "8/7r/6bk/4Q3/2P5/2K1P3/PP4P1/8 b - - 8 51";
    chess::Board board(fen);

    bool foundMate = findMate(board);

    ut::expect(foundMate)
      << "Queen ending game: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };
  
};
