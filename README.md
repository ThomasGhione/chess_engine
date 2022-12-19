## chess_engine ##



# Description

This is a script entirely made in C++ which lets you play chess
using the terminal (no graphics libraries), it's tested on W11
and Ubuntu 20.04 so far.

TODO:
As soon as I complete implementing every rule I'll start 
programming an engine to play against.


# Compile

W11: 
g++ *.cpp
a.exe
  
try running "chcp 65001" if chessboard doesn't display
properly. If it doesn't work on Powershell try it on the CMD.
    
UBUNTU 20.04:
g++ *.cpp
./a.out

MORE INFOS:
Run with -DDEBUG to get the execution time of each move
(excluding how much it takes for the user to input the move and
the time to print the board, so it only calculates how much time
it takes to check if the move is legal).
Run with -O3 to get aggressive optimizations, from my tests it
looks like it doesn't break the program, and it also looks like
the very first move is gonna take relatively more time but this
issue goes away right after.

# How does it work?

After running the program it's gonna ask you which color you
want to play with, which should make no sense, I left it because
it's gonna be useful after I complete the first version of the
engine.
After inputting "W" to play as white, to input every move you
will have to select the coordinates of the square of the piece
you want to move and then select the coordinates of the
destination square; for example let's say white's first move
is pawn to e4, we have to input this: "e2 e4" (which can be seen
as: "move the piece in e2 to e4), you can also input moves in
this way (same example as before): "e 2 e 4" or "e2e4"; or you
can input "Q" to quit or "M" to print more infos.

# Contributing

Pull requests are welcome.
For major changes, please open an issue first to discuss 
what you would like to change.
