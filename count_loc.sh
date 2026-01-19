#!/bin/bash
# Count lines of code for the chess engine including tests (excluding only stockfish, doc, script, games, and ut.hpp library)

cloc . \
  --exclude-dir=stockfish,doc,script,games \
  --exclude-lang=JSON,Markdown \
  --not-match-f='ut\.hpp' \
  --by-file-by-lang
