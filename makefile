# Variabili compilatore
CXX = g++
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -fext-numeric-literals -g
PRODFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -o "chess"

#Directory to be added:
PRODPATH = ./engine/*.cpp ./coords/*.cpp ./printer/*.cpp ./driver/*.cpp
#PRODPATH =  ./engine/*.cpp ./printer/*.cpp ./board/*.cpp

TESTPATH = ./tests/*.cpp

CPPANALYZE = cppcheck
CPPCHEACKFLAGS = --enable=all --verbose --suppress=missingIncludeSystem

prod: 
	$(CXX) $(PRODFLAGS) main.cpp $(PRODPATH)

debug:
	$(CXX) $(PRODFLAGS) -g main.cpp $(PRODPATH)

test:
	$(CXX) $(FLAGS) $(TESTPATH) $(PRODPATH) -o "outputTest"
	mv outputTest tests/outputTest

analyze:
	$(CPPANALYZE) main.cpp $(PRODPATH) $(CPPCHEACKFLAGS)  

cls: 
	rm -f chess
	rm -f tests/outputTest
	rm -f doc/main-doc.{aux,log,pdf,toc}

