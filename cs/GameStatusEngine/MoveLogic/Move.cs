using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using chess_engine_cs.Pieces;

namespace chess_engine_cs.GameStatusEngine.MoveLogic
{
    class Move
    {
        public int Rank1 { get; private set; }
        public int File1 { get; private set; }
        public int Rank2 { get; private set; }
        public int File2 { get; private set; }
        public Piece Piece { get; private set; }

        public Move(int rank1, int file1, int rank2, int file2, Piece piece)
        {
            Rank1 = rank1;
            File1 = file1;
            Rank2 = rank2;
            File2 = file2;
            Piece = piece;
        }
    }
}
