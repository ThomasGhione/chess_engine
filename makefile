# Variabili compilatore
CXX = g++
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -fext-numeric-literals -g
PRODFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -o "chess"

#Directory to be added:
# PRODPATH = main.cpp ./engine/*.cpp ./board/*.cpp ./coords/*.cpp ./printer/*.cpp ./piece/*.cpp
PRODPATH =  ./engine/*.cpp ./printer/*.cpp ./board/*.cpp

TESTPATH = ./tests/*.cpp

prod: 
	$(CXX) $(PRODFLAGS) main.cpp $(PRODPATH)

debug:
	$(CXX) $(PRODFLAGS) -g $(PRODPATH)

test:
	$(CXX) $(FLAGS) $(TESTPATH) $(PRODPATH) -o "outputTest"
	mv outputTest tests/outputTest

clean: rm -f chess
