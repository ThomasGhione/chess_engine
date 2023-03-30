using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using chess_engine_cs.ColorType;
using chess_engine_cs.Pieces.types;

namespace chess_engine_cs.Pieces
{
    class Piece
    {
        public Color Color { get; set; }
        public char Symbol { get; protected set; }
        public PieceTypes PieceType { get; protected set; }

        public Piece(Color color)
        {
            Color = color;
        }
    }
}
