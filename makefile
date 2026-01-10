# Piccola nota: @Comando non scrive il comando ma solo l'output

SHELL := /bin/bash

# Flag per avere l'esecuzione parallela
NUMBER_OF_CORES = $(nproc)
MAKEFLAGS += -j$(NUMBER_OF_CORES)

# Variabili compilatore
CXX = g++
TEST_FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -DDEBUG -fopenmp -march=native -flto=8 -fext-numeric-literals -g
PRODFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O2 -DDEBUG -fopenmp -march=native -flto=8 -pg

# Tool di analisi statica locali
CPPCHECK = script/cppcheck-2.19.0/cppcheck

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
.PHONY: prod parallel_prod debug test performance all-tests analyze analyze-setup analyze-cppcheck analyze-clang-tidy analyze-iwyu analyze-scan-build analyze-gcc-analyzer analyze-cppclean analyze-lizard analyze-summary complexity test-valgrind cls cls-compile-files help

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
analyze: analyze-setup
	@$(MAKE) analyze-cppcheck
	@$(MAKE) analyze-clang-tidy
	@$(MAKE) analyze-iwyu
	@$(MAKE) analyze-scan-build
	@$(MAKE) analyze-lizard
	@$(MAKE) analyze-summary

analyze-setup:
	@printf "\n=== Complete Static Analysis Suite ===\n"
	@printf "⚠️  This will take some minutes...\n"
	@printf "Analyzing %s files (.cpp + .hpp)...\n\n" $(words $(ALL_ANALYSIS_FILES))
	@mkdir -p doc/output-analisi
	@rm -f doc/output-analisi/analisi.log
	@rm -rf doc/output-analisi/scan-build-report
	@rm -f doc/output-analisi/complexity-report.csv
	@printf "=== STATIC ANALYSIS REPORT ===\n" > doc/output-analisi/analisi.log
	@printf "Date: %s\n" "$$(date '+%Y-%m-%d %H:%M:%S')" >> doc/output-analisi/analisi.log
	@printf "Files analyzed: %s\n\n" $(words $(ALL_ANALYSIS_FILES)) >> doc/output-analisi/analisi.log
	@bear -- $(MAKE) prod > /dev/null 2>&1
	@printf "[0/5] Generating compilation database\n"

analyze-cppcheck:
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "1. CPPCHECK ANALYSIS\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@$(CPPCHECK) $(ALL_ANALYSIS_FILES) --enable=all --suppress=missingIncludeSystem --quiet >> doc/output-analisi/analisi.log 2>&1
	@printf "[1/7] Running cppcheck terminated\n"

analyze-clang-tidy:
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "2. CLANG-TIDY ANALYSIS\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@clang-tidy -p . $(MAIN_SRC) $(ALL_MODULE_SRCS) $(ALL_MODULE_HDRS) >> doc/output-analisi/analisi.log 2>&1 || echo "clang-tidy exited with error code $$?" >> doc/output-analisi/analisi.log
	@printf "[2/7] Running clang-tidy terminated\n"

analyze-iwyu:
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "3. INCLUDE-WHAT-YOU-USE ANALYSIS\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@for file in $(MAIN_SRC) $(ALL_MODULE_SRCS); do \
		printf "Checking $$file...\n" >> doc/output-analisi/analisi.log; \
		include-what-you-use -std=c++23 -fopenmp -DDEBUG -march=native $$file >> doc/output-analisi/analisi.log 2>&1 || true; \
	done 2>/dev/null
	@printf "[3/7] Running include-what-you-use terminated\n"

analyze-scan-build:
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "4. SCAN-BUILD (CLANG STATIC ANALYZER)\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@printf "Path-sensitive dataflow analysis in progress...\n" >> doc/output-analisi/analisi.log
	@$(MAKE) cls-compile-files > /dev/null
	@scan-build -o doc/output-analisi/scan-build-report --status-bugs -v -analyzer-config aggressive-binary-operation-simplification=true $(MAKE) prod >> doc/output-analisi/analisi.log 2>&1 || echo "scan-build exited with error code $$?" >> doc/output-analisi/analisi.log
	@printf "\n--- Scan-build text summary ---\n" >> doc/output-analisi/analisi.log
	@if [ -d doc/output-analisi/scan-build-report ] && [ -n "$$(ls -A doc/output-analisi/scan-build-report 2>/dev/null)" ]; then \
		printf "HTML Report location: doc/output-analisi/scan-build-report/\n" >> doc/output-analisi/analisi.log; \
		find doc/output-analisi/scan-build-report -name "*.html" -type f | head -1 | xargs -I {} printf "Main report: {}\n" >> doc/output-analisi/analisi.log 2>&1; \
		printf "\nBugs found by scan-build:\n" >> doc/output-analisi/analisi.log; \
		if ! find doc/output-analisi/scan-build-report -name "*.html" -exec grep -h "<!-- BUGTYPE" {} \; 2>/dev/null | sort | uniq -c >> doc/output-analisi/analisi.log; then \
			printf "No bugs found or unable to parse HTML reports\n" >> doc/output-analisi/analisi.log; \
		fi; \
	else \
		printf "No bugs detected by scan-build!\n" >> doc/output-analisi/analisi.log; \
	fi
	@printf "[4/7] Running scan-build terminated\n"

analyze-gcc-analyzer:
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "5. GCC STATIC ANALYZER\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@$(MAKE) cls-compile-files > /dev/null
	@$(CXX) -fanalyzer -fsyntax-only $(PRODFLAGS) $(MAIN_SRC) $(ALL_MODULE_SRCS) >> doc/output-analisi/analisi.log 2>&1 || echo "GCC analyzer exited with error code $$?" >> doc/output-analisi/analisi.log
	@printf "[5/7] Running GCC analyzer terminated\n"

analyze-cppclean:
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "6. CPPCLEAN (DEAD CODE & DEPENDENCIES)\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@cppclean . >> doc/output-analisi/analisi.log 2>&1 || echo "cppclean exited with error code $$?" >> doc/output-analisi/analisi.log
	@printf "[6/7] Running cppclean terminated\n"

analyze-lizard:
	@printf "\n========================================\n" >> doc/output-analisi/analisi.log
	@printf "7. LIZARD COMPLEXITY ANALYSIS\n" >> doc/output-analisi/analisi.log
	@printf "========================================\n\n" >> doc/output-analisi/analisi.log
	@if [ -f script/lizard-1.19.0/lizard.py ]; then \
		python3 script/lizard-1.19.0/lizard.py -l cpp -w -L 60 -C 15 --csv . > doc/output-analisi/complexity-report.csv 2>&1 || true; \
		python3 script/lizard-1.19.0/lizard.py -l cpp -w -L 60 -C 15 . >> doc/output-analisi/analisi.log 2>&1 || true; \
		printf "[7/7] Running lizard terminated\n"; \
	else \
		printf "Lizard not found - skipping\n" >> doc/output-analisi/analisi.log; \
		printf "[7/7] Running lizard skipped\n"; \
	fi

analyze-summary:
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
	@printf "\n[7/7] Generating LLM prompt...\n\n"
	@printf "=== LLM PROMPT (copy below) ===\n\n"
	@printf "Analizza il report (doc/output-analisi/analisi.log) di analisi statica di un chess engine in C++23.\n\n"
	@printf "**Obiettivo**: Fornisci un riassunto in formato md conciso degli errori rilevati. Inserisci il file dentro: 'doc/testi-llm'\n\n"
	@printf "**Output richiesto**:\n"
	@printf "1. Raggruppa errori identici in una sola voce\n"
	@printf "2. Per ogni errore indica: fonte (cppcheck/clang-tidy/iwyu/scan-build/gcc-analyzer/cppclean), descrizione, numero occorrenze\n"
	@printf "3. Suggerisci fix se evidenti\n"
	@printf "4. Indica il numero di righe in cui l'errore e' presente\n\n"
	@printf "\n\n=== END PROMPT ===\n"

# Pulizia solo dei file oggetto e binari
cls-compile-files:
	rm -f $(ALL_OBJS) $(TEST_OBJS) $(TEST_MAIN_OBJ) $(PERF_OBJS) $(PERF_MAIN_OBJ) $(NAME_APP) $(TEST_APP) $(PERF_APP)

# Pulizia dei file temporanei
cls: cls-compile-files
	@printf "\nCleaning..."
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
	@printf "  make analyze        - Analisi completa (7 tool) ⏱️ 5min\n"
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
