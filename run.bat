chcp 65001
if exist chess.exe del chess.exe
g++ -std=c++23 -Wall -Wextra -Wpedantic -Wno-unused-parameter -DDEBUG -O2 main.cpp ./coords/*.cpp ./engine/*.cpp ./piece/*.cpp ./board/*.cpp ./printer/*.cpp ./driver/*.cpp ./gamestatus/*.cpp -o "chess"
chess.exe
