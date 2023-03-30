using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using chess_engine_cs.GameStatusEngine.MoveLogic;

namespace chess_engine_cs.GameStatusEngine
{
    class GameStatus
    {
        public Move LastMove { get; set; }
        public int Turn { get; set; }

        public GameStatus()
        {
            LastMove = null;
            Turn = 1;
        }

        public void IncTurn(int amount) 
        {
            Turn += amount;
        }
    }
}
