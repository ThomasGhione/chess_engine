# Variabili compilatore
CXX = g++
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -fext-numeric-literals -g
PRODFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -o "chess"

#Directory to be added:
# PRODPATH = main.cpp ./engine/*.cpp ./board/*.cpp ./coords/*.cpp ./printer/*.cpp ./piece/*.cpp
PRODPATH = main.cpp ./engine/*.cpp ./printer/*.cpp

prod: 
	$(CXX) $(PRODFLAGS) $(PRODPATH)

debug:
	$(CXX) $(PRODFLAGS) -g $(PRODPATH)

test:
	$(CXX) $(FLAGS) tests/*.cpp -o "outputTest"
	mv outputTest tests/outputTest

clean: rm -f chess
