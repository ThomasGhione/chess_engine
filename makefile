prod: 
	g++ -std=c++23 -Wall -Wextra -Wpedantic -O2 *.cpp ./printer/*.cpp ./piece/*.cpp ./board/*.cpp -o "chess"

debug:
	g++ -std=c++23 -Wall -Wextra -Wpedantic -g *.cpp ./printer/*.cpp ./piece/*.cpp ./board/*.cpp -o "chess"

clean: rm -f chess