chcp 65001
g++ -std=c++23 -Wall -Wextra -Wpedantic -Wno-unused-parameter -DDEBUG -O2 main.cpp ./coords/*.cpp ./engine/*.cpp ./printer/*.cpp ./driver/*.cpp -o "chess"
chess.exe