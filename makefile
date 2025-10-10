prod: 
	g++ -std=c++23 -Wall -Wextra -Wpedantic -O2 *.cpp ./board/*.cpp ./coords/*.cpp ./engine/*.cpp ./piece/*.cpp ./printer/*.cpp -o "chess"

debug:
	g++ -std=c++23 -Wall -Wextra -Wpedantic -g *.cpp ./board/*.cpp ./coords/*.cpp ./engine/*.cpp ./piece/*.cpp ./printer/*.cpp -o "chess"
clean: rm -f chess