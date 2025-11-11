## chess_engine ##

# Description
This is a program made in C++ which lets you play chess using the terminal (no graphics libraries).

# Tested systems
- Windows 10-11
- WSL 2
- Linux distros

# Compile
Note: Make is required.

## Windows
First of all, compile the project:
```make prod```

Then, you can run it by:
```a.exe```

Try running "chcp 65001" if chessboard doesn't display properly.
If it doesn't work on Powershell then try it on the CMD.

## Linux
First of all, compile the project:
```make prod```

Then, you can run it by:
```./chess.out```

## MORE INFOS:
Those information is needed only for manual compilation.

Run with -DDEBUG to get the execution time of each move (excluding how much it takes for the user to input the move.
Run with -O3 to get aggressive optimizations, from my tests it looks like it doesn't break the program, and it also
looks like the very first move is gonna take relatively more time but this issue goes away right after.

# How does it work?

The program start with a menu where you can choose what to do.
Once game is started you will have to type:
The coordinates of the square of the piece you want to move and then the coordinates of the destination square.
For example:

Move is pawn to e4, we have to input this: "e2 e4" 
(which can be seen as: "move the piece in e2 to e4)

# Contributing

Pull requests are welcome.
For major changes, please open an issue first to discuss what you would like to change.
