#include "../board/board.hpp"

//#include <iostream>
//#include <cstdint>
#include "ut.hpp"

namespace ut = boost::ut;

ut::suite boardSuite = [] {
  using namespace ut;

  "Default constructor"_test = []{
    chess::Board b = chess::Board();
  
    
    expect(b.get(0,0) == (chess::Board::EMPTY) );
  };

  "fen_to_board"_test = []{
    chess::Board b = chess::Board();
    
    const std::string FEN_TEST = "r1bqkbnr/pp1ppppp/2n5/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3";

    b.fromFenToBoard(FEN_TEST);
    
    // Pezzi bianco non leggeri
    expect(b.getByNoteCoords("a1") ==  ( (chess::Board::WHITE) | (chess::Board::ROOK)) );
    expect(b.getByNoteCoords("b1") ==  ( (chess::Board::WHITE) | (chess::Board::KNIGHT)) );
    expect(b.getByNoteCoords("c1") ==  ( (chess::Board::WHITE) | (chess::Board::BISHOP)) );
    expect(b.getByNoteCoords("d1") ==  ( (chess::Board::WHITE) | (chess::Board::QUEEN)) );
    expect(b.getByNoteCoords("e1") ==  ( (chess::Board::WHITE) | (chess::Board::KING)) );
    expect(b.getByNoteCoords("h1") ==  ( (chess::Board::WHITE) | (chess::Board::ROOK)) );
    expect(b.getByNoteCoords("f3") ==  ( (chess::Board::WHITE) | (chess::Board::KNIGHT)) );
    expect(b.getByNoteCoords("f1") ==  ( (chess::Board::WHITE) | (chess::Board::BISHOP)) );


    // Pezzi nero non leggeri
    expect(b.getByNoteCoords("a8") ==  ( (chess::Board::BLACK) | (chess::Board::ROOK)) );
    expect(b.getByNoteCoords("c6") ==  ( (chess::Board::BLACK) | (chess::Board::KNIGHT)) );
    expect(b.getByNoteCoords("c8") ==  ( (chess::Board::BLACK) | (chess::Board::BISHOP)) );
    expect(b.getByNoteCoords("d8") ==  ( (chess::Board::BLACK) | (chess::Board::QUEEN)) );
    expect(b.getByNoteCoords("e8") ==  ( (chess::Board::BLACK) | (chess::Board::KING)) );
    expect(b.getByNoteCoords("h8") ==  ( (chess::Board::BLACK) | (chess::Board::ROOK)) );
    expect(b.getByNoteCoords("g8") ==  ( (chess::Board::BLACK) | (chess::Board::KNIGHT)) );
    expect(b.getByNoteCoords("f8") ==  ( (chess::Board::BLACK) | (chess::Board::BISHOP)) );

    // Pedoni bianco
    expect(b.getByNoteCoords("a2") ==  ( (chess::Board::WHITE) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("b2") ==  ( (chess::Board::WHITE) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("c2") ==  ( (chess::Board::WHITE) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("d2") ==  ( (chess::Board::WHITE) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("e4") ==  ( (chess::Board::WHITE) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("f2") ==  ( (chess::Board::WHITE) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("g2") ==  ( (chess::Board::WHITE) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("h2") ==  ( (chess::Board::WHITE) | (chess::Board::PAWN)) );
    
    // Pedoni nero
    expect(b.getByNoteCoords("a7") ==  ( (chess::Board::BLACK) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("b7") ==  ( (chess::Board::BLACK) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("c5") ==  ( (chess::Board::BLACK) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("d7") ==  ( (chess::Board::BLACK) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("e7") ==  ( (chess::Board::BLACK) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("f7") ==  ( (chess::Board::BLACK) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("g7") ==  ( (chess::Board::BLACK) | (chess::Board::PAWN)) );
    expect(b.getByNoteCoords("h7") ==  ( (chess::Board::BLACK) | (chess::Board::PAWN)) );
  };

  "fen_to_board"_test = []{
    chess::Board b = chess::Board();
    
    const std::string FEN_TEST = "r3kb1r/pp1n1ppp/1qpp4/8/2P2B2/2Q2BP1/PP2P2P/R3K2R b KQkq - 5 15";

    b.fromFenToBoard(FEN_TEST);
    
    char coll[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

    uint8_t wpawn = ( (chess::Board::WHITE) | (chess::Board::PAWN));
    uint8_t bpawn = ( (chess::Board::BLACK) | (chess::Board::PAWN));
    
    uint8_t wrook = ( (chess::Board::WHITE) | (chess::Board::ROOK));
    uint8_t brook = ( (chess::Board::BLACK) | (chess::Board::ROOK));
    
    uint8_t wknight = ( (chess::Board::WHITE) | (chess::Board::KNIGHT));
    uint8_t bknight = ( (chess::Board::BLACK) | (chess::Board::KNIGHT));
    
    uint8_t wbishop = ( (chess::Board::WHITE) | (chess::Board::BISHOP));
    uint8_t bbishop = ( (chess::Board::BLACK) | (chess::Board::BISHOP));
    
    uint8_t wqueen = ( (chess::Board::WHITE) | (chess::Board::QUEEN));
    uint8_t bqueen = ( (chess::Board::BLACK) | (chess::Board::QUEEN));


    uint8_t wking = ( (chess::Board::WHITE) | (chess::Board::KING));
    uint8_t bking = ( (chess::Board::BLACK) | (chess::Board::KING));

    uint8_t empty = (chess::Board::EMPTY) ;

    std::array<uint8_t, 64> expectPos = {wrook, empty, empty, empty, wking, empty, empty, wrook,
                              wpawn, wpawn, empty, empty, wpawn, empty, empty, wpawn,
                              empty, empty, wqueen, empty, empty, wbishop, wpawn, empty,
                              empty, empty, wpawn, empty, empty, wbishop, empty, empty,
                              empty, empty, empty, empty, empty, empty, empty, empty,
                              empty, bqueen, bpawn, bpawn, empty, empty, empty, empty,
                              bpawn, bpawn, empty, bknight, empty, bpawn, bpawn, bpawn,
                              brook, empty, empty, empty, bking, bbishop, empty, brook};
    
    int indexArray = 0;
    for(int i = 1; i <= 8; i++){
      for(char colll : coll){
        expect(b.getByNoteCoords(colll + std::to_string(i)) == expectPos.at(indexArray));

        indexArray++;
      }
    }
  };
  
  "fen_to_board"_test = []{
    chess::Board b = chess::Board();
    
    const std::string FEN_TEST = "r3kb1r/pp1n1ppp/2p1bq2/8/2Pp4/5PP1/PP1BP1BP/2RQK1NR b Kkq - 4 11";

    b.fromFenToBoard(FEN_TEST);
    
    char coll[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

    uint8_t wpawn = ( (chess::Board::WHITE) | (chess::Board::PAWN));
    uint8_t bpawn = ( (chess::Board::BLACK) | (chess::Board::PAWN));
    
    uint8_t wrook = ( (chess::Board::WHITE) | (chess::Board::ROOK));
    uint8_t brook = ( (chess::Board::BLACK) | (chess::Board::ROOK));
    
    uint8_t wknight = ( (chess::Board::WHITE) | (chess::Board::KNIGHT));
    uint8_t bknight = ( (chess::Board::BLACK) | (chess::Board::KNIGHT));
    
    uint8_t wbishop = ( (chess::Board::WHITE) | (chess::Board::BISHOP));
    uint8_t bbishop = ( (chess::Board::BLACK) | (chess::Board::BISHOP));
    
    uint8_t wqueen = ( (chess::Board::WHITE) | (chess::Board::QUEEN));
    uint8_t bqueen = ( (chess::Board::BLACK) | (chess::Board::QUEEN));


    uint8_t wking = ( (chess::Board::WHITE) | (chess::Board::KING));
    uint8_t bking = ( (chess::Board::BLACK) | (chess::Board::KING));

    uint8_t empty = (chess::Board::EMPTY) ;

    std::array<uint8_t, 64> expectPos = {empty, empty, wrook, wqueen, wking, empty, wknight, wrook,
                              wpawn, wpawn, empty, wbishop, wpawn, empty, wbishop, wpawn,
                              empty, empty, empty, empty, empty, wpawn, wpawn, empty,
                              empty, empty, wpawn, bpawn, empty, empty, empty, empty,
                              empty, empty, empty, empty, empty, empty, empty, empty,
                              empty, empty, bpawn, empty, bbishop, bqueen, empty, empty,
                              bpawn, bpawn, empty, bknight, empty, bpawn, bpawn, bpawn,
                              brook, empty, empty, empty, bking, bbishop, empty, brook};
    
    int indexArray = 0;
    for(int i = 1; i <= 8; i++){
      for(char colll : coll){
        expect(b.getByNoteCoords(colll + std::to_string(i)) == expectPos.at(indexArray));

        indexArray++;
      }
    }
  };
};
