using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using chess_engine_cs.ColorType;

namespace chess_engine_cs.Pieces.impl
{
    class King : Piece
    {
        public King(Color color) : base(color)
        {
            Symbol = color == Color.White ? 'K' : 'k';
            PieceType = types.PieceTypes.FULL;
        }
    }
}
