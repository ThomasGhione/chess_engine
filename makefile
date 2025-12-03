# Piccola nota: @Comando non scrive il comando ma solo l'output

SHELL := /bin/bash

# Flag per avere l'esecuzione parallela
NUMBER_OF_CORES = $(nproc)
MAKEFLAGS += -j$(NUMBER_OF_CORES)

# Variabili compilatore
CXX = g++
TEST_FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -DDEBUG -fopenmp -march=native -flto -fext-numeric-literals -g
PRODFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -DDEBUG -fopenmp -march=native -flto

# Nome file finali
NAME_APP = chess
TEST_APP = tests/outputTest

# Path dei file separati
MAIN_SRC = main.cpp
ENGINE_SRCS = $(wildcard ./engine/*.cpp)
COORDS_SRCS = $(wildcard ./coords/*.cpp)
PRINTER_SRCS = $(wildcard ./printer/*.cpp)
DRIVER_SRCS = $(wildcard ./driver/*.cpp)
PIECE_SRCS = $(wildcard ./piece/*.cpp)
GAMESTATUS_SRCS = $(wildcard ./gamestatus/*.cpp)
BOARD_SRCS = $(wildcard ./board/*.cpp)

# Path dei file in unica variabile
ALL_MODULE_SRCS = $(ENGINE_SRCS) $(COORDS_SRCS) $(PRINTER_SRCS) $(DRIVER_SRCS) \
                  $(PIECE_SRCS) $(GAMESTATUS_SRCS) $(BOARD_SRCS)

# Trucchetto per avere i path dei file .o
MAIN_OBJ = main.o
MODULE_OBJS = $(ALL_MODULE_SRCS:.cpp=.o)
ALL_OBJS = $(MAIN_OBJ) $(MODULE_OBJS)

# Path dei file di test
TEST_MAIN_SRC = $(wildcard ./tests/*.cpp)
TEST_ENGINE_SRCS = $(wildcard ./engine/test/*.cpp)
TEST_COORDS_SRCS = $(wildcard ./coords/test/*.cpp)
TEST_PRINTER_SRCS = $(wildcard ./printer/test/*.cpp)
TEST_DRIVER_SRCS = $(wildcard ./driver/test/*.cpp)
TEST_PIECE_SRCS = $(wildcard ./piece/test/*.cpp)
TEST_GAMESTATUS_SRCS = $(wildcard ./gamestatus/test/*.cpp)
TEST_BOARD_SRCS = $(wildcard ./board/test/*.cpp)

# Path dei file di test in unica variabile
ALL_TEST_MODULE_SRCS = $(TEST_ENGINE_SRCS) $(TEST_COORDS_SRCS) $(TEST_PRINTER_SRCS) \
											 $(TEST_DRIVER_SRCS) $(TEST_PIECE_SRCS) $(TEST_GAMESTATUS_SRCS) \
											 $(TEST_BOARD_SRCS)

# Path dei file .o di test
TEST_MAIN_OBJ = $(TEST_MAIN_SRC:.cpp=.o)
TEST_OBJS = $(ALL_TEST_MODULE_SRCS:.cpp=.o)

# Target principali
.PHONY: prod parallel_prod debug test analyze clean help

# Default per usare make secco
all: prod

# Produzione parallela
# Nota: Piu' veloce dopo la prima volta ma lascia i file .o
prod: $(NAME_APP)
	@printf "\n✅ Build completato: $(NAME_APP)"

# Produzione sequenziale
# Nota: Non e' piu' lento ma non genera i file .o
prod_sequential:
	$(CXX) $(PRODFLAGS) $(MAIN_SRC) $(ALL_MODULE_SRCS) -o $(NAME_APP)
	@printf "\n✅ Build completato: $(NAME_APP)"

# Comando per generare eseguibile per il debug
# Aggiunge il flag -g per mettere le informazioni di debug
debug: PRODFLAGS += -g
debug: $(NAME_APP)
	@printf "\n✅ Build debug completato: $(NAME_APP)"

# Comando per generare eseguibile per il debug
test: $(TEST_APP)
	@printf "\n✅ Test compilato: $(TEST_APP)"

# Generazione file finale 'chess'
$(NAME_APP): $(ALL_OBJS)
	@printf "\nLinking $(NAME_APP)..."
	$(CXX) $(PRODFLAGS) $(ALL_OBJS) -o $(NAME_APP)

# Creazione dei file .o
%.o: %.cpp
	@printf "\nCompiling $<..."
	$(CXX) $(PRODFLAGS) -c $< -o $@

# Generazione file finale 'outputTest'
$(TEST_APP): $(MODULE_OBJS) $(TEST_OBJS) $(TEST_MAIN_OBJ)
	@printf "\nLinking test $(TEST_APP)..."
	$(CXX) $(TEST_FLAGS) $(MODULE_OBJS) $(TEST_OBJS) $(TEST_MAIN_OBJ) -o $(TEST_APP)

# Analisi del codice
analyze:
	@printf "\nAnalyzing code..."
	cppcheck main.cpp $(ALL_MODULE_SRCS) --enable=all --verbose --suppress=missingIncludeSystem

# Pulizia dei file temporanei
cls:
	@printf "\nCleaning..."
	rm -f $(NAME_APP) $(TEST_APP) $(ALL_OBJS) $(TEST_OBJS) $(TEST_MAIN_OBJ)
	rm -f doc/main-doc.{aux,log,pdf,toc}
	@printf "\n✅ Clean completato"

# Helper per vedere i comandi
help:
	@printf "\n=== Chess Build System ==="
	@printf ""
	@printf "Target disponibili:"
	@printf "  make prod           - Compilazione monolitica (veloce prima volta)"
	@printf "  make parallel_prod  - Compilazione parallela (veloce dopo modifiche)"
	@printf "  make debug          - Compilazione con debug symbols"
	@printf "  make test           - Compilazione test"
	@printf "  make analyze        - Analisi statica del codice"
	@printf "  make clean          - Rimozione file compilati"
	@printf "  make debug-vars     - stampa variabili"
	@printf ""
	@printf "=================================="

# Per mostrare le variabili
.PHONY: debug-vars
debug-vars:
	@printf "\n"
	@printf "SHELL = $(SHELL)"
	@printf "Comando compilazione = $(CXX)"
	@printf "Test flag = $(TEST_FLAGS)"
	@printf "Prod flag = $(PRODFLAGS)"
	@printf "Percorso dei moduli = $(ALL_MODULE_SRCS)"
	@printf "Percorso dei moduli di test = $(ALL_TEST_MODULE_SRCS)"
	@printf "MODULE_OBJS = $(MODULE_OBJS)"
