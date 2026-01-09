# Piccola nota: @Comando non scrive il comando ma solo l'output

SHELL := /bin/bash

# Flag per avere l'esecuzione parallela
NUMBER_OF_CORES = $(nproc)
MAKEFLAGS += -j$(NUMBER_OF_CORES)

# Variabili compilatore
CXX = g++
TEST_FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -DDEBUG -fopenmp -march=native -flto=8 -fext-numeric-literals -g
PRODFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -DDEBUG -fopenmp -march=native -flto=8 -pg

# Nome file finali
NAME_APP = chess
TEST_APP = tests/outputTest
PERF_APP = tests/outputPerformance

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

# Path dei file header (escludendo stockfish e test utils)
ENGINE_HDRS = $(wildcard ./engine/*.hpp)
COORDS_HDRS = $(wildcard ./coords/*.hpp)
PRINTER_HDRS = $(wildcard ./printer/*.hpp)
DRIVER_HDRS = $(wildcard ./driver/*.hpp)
PIECE_HDRS = $(wildcard ./piece/*.hpp)
GAMESTATUS_HDRS = $(wildcard ./gamestatus/*.hpp)
BOARD_HDRS = $(wildcard ./board/*.hpp)

# Path di tutti gli header
ALL_MODULE_HDRS = $(ENGINE_HDRS) $(COORDS_HDRS) $(PRINTER_HDRS) $(DRIVER_HDRS) \
                  $(PIECE_HDRS) $(GAMESTATUS_HDRS) $(BOARD_HDRS)

# Tutti i file da analizzare (.cpp e .hpp)
ALL_ANALYSIS_FILES = $(MAIN_SRC) $(ALL_MODULE_SRCS) $(ALL_MODULE_HDRS)

# Trucchetto per avere i path dei file .o
MAIN_OBJ = main.o
MODULE_OBJS = $(ALL_MODULE_SRCS:.cpp=.o)
ALL_OBJS = $(MAIN_OBJ) $(MODULE_OBJS)

# Path dei file di test
TEST_MAIN_SRC = tests/mainTest.cpp
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

# Path dei file di performance test
PERF_MAIN_SRC = tests/mainPerformanceTest.cpp
PERF_ENGINE_SRCS = $(wildcard ./engine/test/performance-test/*.cpp)

# Path dei file di performance test in unica variabile
ALL_PERF_MODULE_SRCS = $(PERF_ENGINE_SRCS)

# Path dei file .o di performance test
PERF_MAIN_OBJ = $(PERF_MAIN_SRC:.cpp=.o)
PERF_OBJS = $(ALL_PERF_MODULE_SRCS:.cpp=.o)

# Target principali
.PHONY: prod parallel_prod debug test performance all-tests analyze complexity test-valgrind cls help

# Default per usare make secco
all: prod

# Produzione parallela
# Nota: Piu' veloce dopo la prima volta ma lascia i file .o
prod: $(NAME_APP)
	@printf "\n✅ Build completato: $(NAME_APP)\n\n"

# Produzione sequenziale
# Nota: Non e' piu' lento ma non genera i file .o
prod_sequential:
	$(CXX) $(PRODFLAGS) $(MAIN_SRC) $(ALL_MODULE_SRCS) -o $(NAME_APP)
	@printf "\n✅ Build completato: $(NAME_APP)\n\n"

# Comando per generare eseguibile per il debug
# Aggiunge il flag -g per mettere le informazioni di debug
debug: PRODFLAGS += -g
debug: $(NAME_APP)
	@printf "\n✅ Build debug completato: $(NAME_APP)\n\n"

# Comando per generare eseguibile per il test
test: $(TEST_APP)
	@printf "\n✅ Test compilato: $(TEST_APP)\n\n"

# Comando per generare eseguibile per performance test
performance: $(PERF_APP)
	@printf "\n✅ Performance test compilato: $(PERF_APP)\n\n"

# Comando per eseguire tutti i test (funzionali + performance)
all-tests: test performance
	@printf "\n=== Running functional tests ===\n"
	-./$(TEST_APP)
	@printf "\n=== Running performance tests ===\n"
	-./$(PERF_APP)
	@printf "\n✅ All tests completed\n\n"

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

# Generazione file finale 'outputPerformance'
$(PERF_APP): $(MODULE_OBJS) $(PERF_OBJS) $(PERF_MAIN_OBJ)
	@printf "\nLinking performance test $(PERF_APP)..."
	$(CXX) $(TEST_FLAGS) $(MODULE_OBJS) $(PERF_OBJS) $(PERF_MAIN_OBJ) -o $(PERF_APP)

# Analisi completa del codice
# Esegue: cppcheck + clang-tidy + iwyu + scan-build + lizard
# Output: analisi.log, scan-build-report/, complexity-report.txt
# ⚠️  Tempo stimato: 10-30 minuti
analyze:
	@printf "\n=== Complete Static Analysis Suite ===\n"
	@printf "⚠️  This will take 10-30 minutes...\n"
	@printf "Analyzing %d files (.cpp + .hpp)...\n\n" $(words $(ALL_ANALYSIS_FILES))
	@mkdir -p doc/output-analisi
	@rm -f doc/output-analisi/analisi.log
	@rm -rf doc/output-analisi/scan-build-report
	@rm -f doc/output-analisi/complexity-report.csv
	@printf "=== STATIC ANALYSIS REPORT ===\n" > doc/output-analisi/analisi.log
	@printf "Date: %s\n" "$$(date '+%Y-%m-%d %H:%M:%S')" >> doc/output-analisi/analisi.log
	@printf "Files analyzed: %d\n\n" $(words $(ALL_ANALYSIS_FILES)) >> doc/output-analisi/analisi.log
	@printf "[0/5] Generating compilation database... "
	@bear -- $(MAKE) prod > /dev/null 2>&1
	@printf "Done\n"
	@printf "[1/5] Running cppcheck... "
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "1. CPPCHECK ANALYSIS\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@cppcheck $(ALL_ANALYSIS_FILES) --enable=all --suppress=missingIncludeSystem --quiet >> doc/output-analisi/analisi.log 2>&1
	@printf "Done\n"
	@printf "[2/5] Running clang-tidy... "
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "2. CLANG-TIDY ANALYSIS\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@clang-tidy -p . $(MAIN_SRC) $(ALL_MODULE_SRCS) $(ALL_MODULE_HDRS) >> doc/output-analisi/analisi.log 2>&1 || true
	@printf "Done\n"
	@printf "[3/5] Running include-what-you-use... "
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "3. INCLUDE-WHAT-YOU-USE ANALYSIS\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@for file in $(MAIN_SRC) $(ALL_MODULE_SRCS); do \
		printf "Checking $$file...\n" >> doc/output-analisi/analisi.log; \
		include-what-you-use -std=c++23 -fopenmp -DDEBUG -march=native $$file >> doc/output-analisi/analisi.log 2>&1 || true; \
	done 2>/dev/null
	@printf "Done\n"
	@printf "[4/5] Running scan-build (Clang Static Analyzer)... "
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "4. SCAN-BUILD (CLANG STATIC ANALYZER)\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@printf "Path-sensitive dataflow analysis in progress...\n" >> doc/output-analisi/analisi.log
	@scan-build -o doc/output-analisi/scan-build-report --status-bugs -v -v -analyzer-config aggressive-binary-operation-simplification=true make prod >> doc/output-analisi/analisi.log 2>&1 || true
	@printf "\n--- Scan-build text summary ---\n" >> doc/output-analisi/analisi.log
	@if [ -d doc/output-analisi/scan-build-report ] && [ -n "$$(ls -A doc/output-analisi/scan-build-report 2>/dev/null)" ]; then \
		printf "HTML Report location: doc/output-analisi/scan-build-report/\n" >> doc/output-analisi/analisi.log; \
		find doc/output-analisi/scan-build-report -name "*.html" -type f | head -1 | xargs -I {} printf "Main report: {}\n" >> doc/output-analisi/analisi.log 2>&1 || true; \
		printf "\nBugs found by scan-build:\n" >> doc/output-analisi/analisi.log; \
		find doc/output-analisi/scan-build-report -name "*.html" -exec grep -h "<!-- BUGTYPE" {} \; 2>/dev/null | sort | uniq -c >> doc/output-analisi/analisi.log || printf "No bugs found or unable to parse HTML reports\n" >> doc/output-analisi/analisi.log; \
	else \
		printf "No bugs detected by scan-build!\n" >> doc/output-analisi/analisi.log; \
	fi
	@printf "Done\n"
	@printf "[5/5] Running lizard (complexity analysis)... "
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "5. LIZARD COMPLEXITY ANALYSIS\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@if [ -f script/lizard-1.19.0/lizard.py ]; then \
		python3 script/lizard-1.19.0/lizard.py -l cpp -w -L 60 -C 15 --csv . > doc/output-analisi/complexity-report.csv 2>&1 || true; \
		python3 script/lizard-1.19.0/lizard.py -l cpp -w -L 60 -C 15 . >> doc/output-analisi/analisi.log 2>&1 || true; \
		printf "Done\n"; \
	else \
		printf "Lizard not found - skipping\n" >> doc/output-analisi/analisi.log; \
		printf "Skipped\n"; \
	fi
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "COMPLETE ANALYSIS FINISHED\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n" >> doc/output-analisi/analisi.log
	@printf "\n========================================\n"
	@printf "✅ Complete analysis finished!\n"
	@printf "========================================\n"
	@printf "\n📊 Reports generated:\n"
	@printf "   1. doc/output-analisi/analisi.log (main report)\n"
	@printf "   2. doc/output-analisi/scan-build-report/*/index.html (interactive HTML)\n"
	@if [ -f doc/output-analisi/complexity-report.csv ]; then \
		printf "   3. doc/output-analisi/complexity-report.csv (spreadsheet)\n"; \
	fi
	@printf "\n"

# Analisi di complessità del codice (solo lizard)
# Identifica funzioni troppo complesse da refactorare
complexity:
	@printf "\n=== Complexity Analysis ===\n"
	@mkdir -p doc/output-analisi
	@if [ -f script/lizard-1.19.0/lizard.py ]; then \
		printf "Analyzing code complexity...\n\n"; \
		python3 script/lizard-1.19.0/lizard.py -l cpp -w -L 60 -C 15 . 2>&1 | tee doc/output-analisi/complexity-report.txt; \
		printf "\n✅ Report saved to: doc/output-analisi/complexity-report.txt\n\n"; \
	else \
		printf "❌ Lizard not found at script/lizard-1.19.0/lizard.py\n\n"; \
	fi

# Test con Valgrind per memory leak detection
# NOTA: Richiede valgrind installato (sudo apt install valgrind)
# Esegue il programma con un input di test per verificare memory leaks
test-valgrind:
	@printf "\n=== Memory Leak Analysis (Valgrind) ===\n"
	@mkdir -p doc/output-analisi
	@if ! command -v valgrind > /dev/null 2>&1; then \
		printf "❌ Valgrind not installed.\n"; \
		printf "Install with: sudo apt install valgrind\n\n"; \
		exit 1; \
	fi
	@if [ ! -f $(NAME_APP) ]; then \
		printf "❌ Binary '$(NAME_APP)' not found. Run 'make prod' first.\n\n"; \
		exit 1; \
	fi
	@printf "Running valgrind on $(NAME_APP)...\n"
	@printf "This will run the program and check for memory issues.\n"
	@printf "⚠️  The program will wait for input - press Ctrl+D to exit gracefully.\n\n"
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=doc/output-analisi/valgrind-report.txt ./$(NAME_APP) || true
	@printf "\n✅ Valgrind analysis complete!\n"
	@printf "📊 Full report saved to: doc/output-analisi/valgrind-report.txt\n"
	@printf "\nSummary:\n"
	@grep -A 5 "LEAK SUMMARY" doc/output-analisi/valgrind-report.txt || printf "No leaks detected!\n"
	@printf "\n"

# Pulizia dei file temporanei
cls:
	@printf "\nCleaning..."
	rm -f $(NAME_APP) $(TEST_APP) $(PERF_APP) $(ALL_OBJS) $(TEST_OBJS) $(TEST_MAIN_OBJ) $(PERF_OBJS) $(PERF_MAIN_OBJ)
	rm -f doc/main-doc.{aux,log,pdf,toc}
	rm -f compile_commands.json
	rm -rf doc/output-analisi
	rm -rf gmon.out
	@printf "\n✅ Clean completato\n\n"

#
get-image:
	gprof ./chess gmon.out | python3 script/gprof2dot.py -s -w | dot -Tpng -Gdpi=200 -o output.png
	@printf "\n✅ Immagine creata\n\n"

# Helper per vedere i comandi
help:
	@printf "\n=== Chess Build System ===\n"
	@printf "\n📦 BUILD TARGETS:\n"
	@printf "  make prod           - Compilazione monolitica (veloce prima volta)\n"
	@printf "  make debug          - Compilazione con debug symbols\n"
	@printf "  make test           - Compilazione e test funzionali\n"
	@printf "  make performance    - Compilazione e performance test\n"
	@printf "  make all-tests      - Esecuzione completa (test + performance)\n"
	@printf "\n🔍 STATIC ANALYSIS:\n"
	@printf "  make analyze        - Analisi completa (5 tool) ⏱️ 10-30min\n"
	@printf "                        • cppcheck (bug detection)\n"
	@printf "                        • clang-tidy (modernization)\n"
	@printf "                        • iwyu (include optimization)\n"
	@printf "                        • scan-build (deep dataflow analysis)\n"
	@printf "                        • lizard (complexity metrics)\n"
	@printf "  make complexity     - Solo analisi complessità (lizard)\n"
	@printf "\n🧪 RUNTIME TESTING:\n"
	@printf "  make test-valgrind  - Memory leak detection (valgrind)\n"
	@printf "\n🧹 UTILITIES:\n"
	@printf "  make cls            - Rimozione file compilati e report\n"
	@printf "  make debug-vars     - Stampa variabili Makefile\n"
	@printf "  make help           - Mostra questo help\n"
	@printf "\n📊 REPORTS GENERATED:\n"
	@printf "  - doc/output-analisi/analisi.log                    (main text report)\n"
	@printf "  - doc/output-analisi/scan-build-report/*.html       (interactive HTML)\n"
	@printf "  - doc/output-analisi/complexity-report.txt/.csv     (complexity metrics)\n"
	@printf "  - doc/output-analisi/valgrind-report.txt            (memory analysis)\n"
	@printf "\n⚙️  TOOL LOCATIONS:\n"
	@printf "  - lizard: script/lizard-1.19.0/lizard.py\n"
	@printf "  - valgrind: install with 'sudo apt install valgrind'\n"
	@printf "\n==================================\n"

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
