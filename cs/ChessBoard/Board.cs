using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using chess_engine_cs.Pieces;
using chess_engine_cs.ColorType;
using chess_engine_cs.Pieces.impl;
using chess_engine_cs.GameStatusEngine;

namespace chess_engine_cs.ChessBoard
{
    class Board
    {
        private Piece[,] board = new Piece[8, 8];
        private GameStatus gs;

        public Board()
        {
            for (int i = 0; i < 8; ++i)
            {
                board[i, 1] = new Pawn(Color.White);
                board[i, 6] = new Pawn(Color.Black);
            }

            board[0, 0] = board[0, 7] = new Rook(Color.White);
            board[0, 1] = board[0, 6] = new Knight(Color.White);
            board[0, 2] = board[0, 5] = new Bishop(Color.White);
            board[0, 3] = new Queen(Color.White);
            board[0, 4] = new King(Color.White);

            board[0, 0] = board[7, 7] = new Rook(Color.Black);
            board[7, 1] = board[7, 6] = new Knight(Color.Black);
            board[7, 2] = board[7, 5] = new Bishop(Color.Black);
            board[7, 3] = new Queen(Color.Black);
            board[7, 4] = new King(Color.Black);
        }

        public void DrawBoard()
        {
            Console.WriteLine($"\n\n A B C D E F G H LAST MOVE: {getPiece(gs.LastMove.Piece)}{gs.LastMove.File1}{gs.LastMove.Rank1}{gs.LastMove.File2}{gs.LastMove.Rank2}\n\n\n");
            for (int rank = 7; rank >= 0; rank -= 2)
            {

                Console.WriteLine($" █████ █████ █████ █████ \n{rank + 1} " +
                $"{"██" + PrintPiece(rank, 0) + "██"} {" " + PrintPiece( rank, 1) + " "} {"██" + PrintPiece( rank, 2) + "██"} {" " + PrintPiece( rank, 3) + " "} {"██" + PrintPiece( rank, 4) + "██"} {" " + PrintPiece( rank, 5) + " "} {"██" + PrintPiece( rank, 6) + "██"} {" " + PrintPiece( rank, 7) + " "} {rank + 1}\n █████ █████ █████ █████ ");

                Console.WriteLine($" █████ █████ █████ █████\n{rank + 1} " +
                $"{" " + PrintPiece( rank, 0) + " "} {"██" + PrintPiece( rank, 1) + "██"} {" " + PrintPiece( rank, 2) + " "} {"██" + PrintPiece( rank, 3) + "██"} {" " + PrintPiece( rank, 4) + " "} {"██" + PrintPiece( rank, 5) + "██"} {" " + PrintPiece( rank, 6) + " "} {"██" + PrintPiece( rank, 7) + "██"} {rank + 1}\n █████ █████ █████ █████");
                

 
            }
            Console.WriteLine($"\n\n A B C D E F G H TURN: {gs.Turn}\n\n");
        }

        private char PrintPiece(int rank, int pos) 
        {
            if (board[rank, pos].PieceType != Pieces.types.PieceTypes.EMPTY) return getPiece(board[rank, pos]);
            return ' '; //TOFIX
        }

        private char getPiece(Piece piece)
        {
            return piece.Symbol;
        }
    }
}
