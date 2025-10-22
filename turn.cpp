/*
void inputMove(gameStatus &gs) noexcept {
  cout << "\nPress M/m for more infos, or input your move: ";

  coords cFrom, cTo;

  ifWrongMove: // if something goes wrong while inputting the move or if it isn't valid then it goes back here and ask again until it's valid
  cin >> cFrom.file;

  if (cFrom.file == 'M' || cFrom.file == 'm') {
      cout << "MORE INFOS:\n    Format: file1rank1 file2rank2; example: e2 e4\n    Press M/m to print this menu again\n    Press Q/q to exit the program\n    Press P/p to print every move\nInput: ";
      // goto ifWrongMove;
  }

  //TODO fix
  // if user inputs P/p then print all moves
  if (cFrom.file == 'P' || cFrom.file == 'p') {
      printAllMoves(gs.listOfMoves);
      goto ifWrongMove;
  }

  
  if (cFrom.file == 'Q' || cFrom.file == 'q') // quits the game
      exit(EXIT_SUCCESS);

  // gets the remaining inputs
  cin >> cFrom.rank >> cTo.file >> cTo.rank;

  #ifdef DEBUG
      auto start = chrono::steady_clock::now();
  #endif

  // check for specific errors, due to the input being invalid or meaningless
  // the coords are invalid if: "< a1" || "> h8"
  if (cFrom.rank < 1 || cFrom.rank > 8 || cTo.rank < 1 || cTo.rank > 8 || cFrom.file < 65 || (cFrom.file > 72 && cFrom.file < 97) || cFrom.file > 104 || cTo.file < 65 || (cTo.file > 72 && cTo.file < 97) || cTo.file > 104) {
      cout << "Invalid coords! Choose again: ";
      cFrom.file = cTo.file = '0';
      cFrom.rank = cTo.rank = 0;
      goto ifWrongMove;
  }

  // can't choose an empty square
  if (gs.chessboard[cFrom.rank - 1][ctoi(cFrom.file) - 1].piece.id == EMPTY) {
      cout << "You can't choose an EMPTY square! Choose again: ";
      goto ifWrongMove;
  }

  // can't choose an opponent piece
  if ((gs.chessboard[cFrom.rank - 1][ctoi(cFrom.file) - 1].piece.id & PLAYERMASK) != gs.player) {
      cout << "It's " << print::getPlayer(gs.player) << "'s turn! Choose again: ";
      goto ifWrongMove;
  }

  // can't take your own pieces
  if ((gs.chessboard[cTo.rank - 1][ctoi(cTo.file) - 1].piece.id & PLAYERMASK) == gs.player) {
      cout << "You can't take your pieces! Choose again: ";
      goto ifWrongMove;
  }        

  
  // check if it's a legal move
  move pieceToMove = { { cFrom, gs.chessboard[cFrom.rank - 1][ctoi(cFrom.file) - 1].piece.id}, cTo };
  if (!isMoveValid(gs, pieceToMove)) {
      cout << "This move isn't legal! Choose again: ";
      goto ifWrongMove;
  }

  // update wherePieceAt
  //TODO DEBUG ONLY:
  //printPieceOrEmptyCoordsV(gs.wherePieceAt);

  // move is valid, therefore change player's turn
  gs.player = (gs.player == WHITE) ? BLACK : WHITE;
  if (gs.player == BLACK)
      ++gs.turns;
  
  // update lastMove 
  gs.lastMove = { { cFrom, gs.chessboard[cFrom.rank - 1][ctoi(cFrom.file) - 1].piece.id }, cTo };

  // update listOfMoves
  gs.listOfMoves.push_back(gs.lastMove);

  //move the piece to the selected square
  gs.chessboard[cTo.rank - 1][ctoi(cTo.file) - 1].piece = gs.chessboard[cFrom.rank - 1][ctoi(cFrom.file) - 1].piece;        
  gs.chessboard[cFrom.rank - 1][ctoi(cFrom.file) - 1].piece.id = EMPTY;

  #ifdef DEBUG
      auto end = chrono::steady_clock::now();
      auto diff = end - start;
  #endif

  print::getBoard(gs); // print the chessboard after every move

  #ifdef DEBUG
      cout << "Time to calculate if the move was legal: " << chrono::duration <double, nano> (diff).count() << " ns\n";
  #endif
}
*/
