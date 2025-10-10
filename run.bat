chcp 65001
g++ -std=c++23 -Wall -Wextra -Wpedantic -DDEBUG -O2 *.cpp ./board/*.cpp ./coords/*.cpp ./engine/*.cpp ./piece/*.cpp ./printer/*.cpp -o "chess"
chess.exe