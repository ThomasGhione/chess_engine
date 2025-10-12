# Variabili compilatore
CXX = g++
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -fext-numeric-literals -g

prod: 
	#g++ -std=c++23 -Wall -Wextra -Wpedantic -O2 *.cpp ./board/*.cpp ./coords/*.cpp ./engine/*.cpp ./piece/*.cpp ./printer/*.cpp -o "chess"
	g++ -std=c++23 -Wall -Wextra -Wpedantic -O2 main.cpp -o "chess"

debug:
	g++ -std=c++23 -Wall -Wextra -Wpedantic -g *.cpp ./board/*.cpp ./coords/*.cpp ./engine/*.cpp ./piece/*.cpp ./printer/*.cpp -o "chess"

test:
	$(CXX) $(FLAGS) tests/*.cpp -o "outputTest"
	mv outputTest tests/outputTest

clean: rm -f chess
