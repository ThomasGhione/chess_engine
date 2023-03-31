using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using chess_engine_cs.ColorType;

namespace chess_engine_cs.Pieces.impl
{
    class Bishop : Piece
    {
        public Bishop(Color color) : base(color)
        {
            Symbol = color == Color.White ? 'B' : 'b';
            PieceType = types.PieceTypes.FULL;
            
        }
    }
}
