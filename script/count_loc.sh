#!/bin/bash
# Count lines of code for the chess engine including tests (excluding only stockfish, doc, script, games, and ut.hpp library)

cloc . \
  --exclude-dir=output,doc,script,games,saves \
  --exclude-lang=JSON,Markdown,make \
  --not-match-f='ut\.hpp' \
  --by-file-by-lang
