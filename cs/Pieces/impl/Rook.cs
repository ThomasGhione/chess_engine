using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using chess_engine_cs.ColorType;

namespace chess_engine_cs.Pieces.impl
{
    class Rook : Piece
    {
        public Rook(Color color) : base(color)
        {
            Symbol = color == Color.White ? 'R' : 'r';
            PieceType = types.PieceTypes.FULL;
        }
    }
}
