chcp 65001
g++ -std=c++23 -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-variable -Wno-type-limits -DDEBUG -O2 main.cpp ./board/*.cpp ./coords/*.cpp ./engine/*.cpp ./piece/*.cpp ./printer/*.cpp ./driver/*.cpp -o "chess"
chess.exe