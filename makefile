SHELL := /bin/bash

NUMBER_OF_CORES := $(shell nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
ifeq ($(MAKELEVEL),0)
ifeq ($(filter -j%,$(MAKEFLAGS)),)
MAKEFLAGS += -j$(NUMBER_OF_CORES)
endif
endif

# Variabili compilatore
CXX = g++
TEST_FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O3 -DDEBUG -fopenmp -march=native -flto=8 -fext-numeric-literals -g

# -O3: Aggressive inlining + vectorization (better than -O2 and safer than -Ofast)
# -march=native: Use all CPU features (SSE4.2, AVX2, BMI2, POPCNT)
# -mtune=native: Optimize instruction scheduling for your CPU
# -flto=auto: Let compiler decide LTO threads (usually better than fixed number)
# -fno-math-errno: Don't set errno on math functions (safe for chess)
# -fno-trapping-math: Assume floating point ops don't trap (safe, we don't use FP exceptions)
# -funroll-loops: Unroll hot loops (good for bitboard operations)
# Balanced production flags (safety + speed)
PRODFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -fopenmp -march=native -mtune=native \
		-flto=auto -fno-math-errno -fno-trapping-math -funroll-loops \
		-ffunction-sections -fdata-sections -O3 

# Uncomment if codebase is exception-free

# Cross-compiler per Windows (installare mingw-w64)
WIN_CXX = x86_64-w64-mingw32-g++
WIN_PRODFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O3 -DDEBUG -static -static-libgcc -static-libstdc++ -fopenmp -flto=4 -fext-numeric-literals

# Tool di analisi statica locali
CPPCHECK = script/cppcheck-2.19.0/cppcheck
COMPILATION_DB = script/compile_commands.json

# Nome file finali
NAME_APP = chess
TEST_APP = tests/test
PERF_APP = tests/perf
TT_HP_BENCH_APP = tests/tt_hugepage_bench
NAME_APP_WIN = chess.exe
OUTPUT_DIR = output
GENERATED_BINS = $(NAME_APP) $(NAME_APP_WIN) $(TEST_APP) $(PERF_APP) $(TT_HP_BENCH_APP)

# File paths by module
MAIN_SRC = main.cpp
ENGINE_SRCS = $(wildcard ./engine/*.cpp) $(wildcard ./engine/eval/*.cpp) $(wildcard ./engine/eval/*/*.cpp) $(wildcard ./engine/search/*.cpp) $(wildcard ./engine/movegen/*.cpp)
COORDS_SRCS = $(wildcard ./coords/*.cpp)
DRIVER_SRCS = $(wildcard ./driver/*.cpp)
PIECE_SRCS = $(wildcard ./piece/*.cpp)
GAMESTATUS_SRCS = $(wildcard ./gamestatus/*.cpp)
BOARD_SRCS = $(wildcard ./board/*.cpp)
UCI_SRCS = $(wildcard ./uci/*.cpp)

# Module source paths in a single variable
ALL_MODULE_SRCS = $(ENGINE_SRCS) $(COORDS_SRCS) $(PRINTER_SRCS) $(DRIVER_SRCS) \
                  $(PIECE_SRCS) $(GAMESTATUS_SRCS) $(BOARD_SRCS) $(UCI_SRCS)

# Header file paths (excluding stockfish and test utils)
ENGINE_HDRS = $(wildcard ./engine/*.hpp) $(wildcard ./engine/eval/*.hpp) $(wildcard ./engine/search/*.hpp) $(wildcard ./engine/movegen/*.hpp)
COORDS_HDRS = $(wildcard ./coords/*.hpp)
DRIVER_HDRS = $(wildcard ./driver/*.hpp)
PIECE_HDRS = $(wildcard ./piece/*.hpp)
GAMESTATUS_HDRS = $(wildcard ./gamestatus/*.hpp)
BOARD_HDRS = $(wildcard ./board/*.hpp)
UCI_HDRS = $(wildcard ./uci/*.hpp)

# All header paths
ALL_MODULE_HDRS = $(ENGINE_HDRS) $(COORDS_HDRS) $(PRINTER_HDRS) $(DRIVER_HDRS) \
                  $(PIECE_HDRS) $(GAMESTATUS_HDRS) $(BOARD_HDRS) $(UCI_HDRS)

# All files to analyze (.cpp and .hpp)
ALL_ANALYSIS_FILES = $(MAIN_SRC) $(ENGINE_SRCS) $(COORDS_SRCS) $(PRINTER_SRCS) $(DRIVER_SRCS) \
                  	$(PIECE_SRCS) $(GAMESTATUS_SRCS) $(BOARD_SRCS) \
			$(ENGINE_HDRS) $(COORDS_HDRS) $(PRINTER_HDRS) $(DRIVER_HDRS) \
			$(PIECE_HDRS) $(GAMESTATUS_HDRS) $(BOARD_HDRS)


# Helper to generate .o paths inside output/
MAIN_OBJ = $(OUTPUT_DIR)/$(MAIN_SRC:.cpp=.o)
MODULE_OBJS = $(addprefix $(OUTPUT_DIR)/,$(ALL_MODULE_SRCS:.cpp=.o))
ALL_OBJS = $(MAIN_OBJ) $(MODULE_OBJS)

# Test file paths
TEST_MAIN_SRC = tests/mainTest.cpp
TEST_ENGINE_SRCS = $(wildcard ./engine/test/*.cpp)
TEST_COORDS_SRCS = $(wildcard ./coords/test/*.cpp)
TEST_DRIVER_SRCS = $(wildcard ./driver/test/*.cpp)
TEST_PIECE_SRCS = $(wildcard ./piece/test/*.cpp)
TEST_GAMESTATUS_SRCS = $(wildcard ./gamestatus/test/*.cpp)
TEST_BOARD_SRCS = $(wildcard ./board/test/*.cpp)

# Test file paths in a single variable
ALL_TEST_MODULE_SRCS = $(TEST_ENGINE_SRCS) $(TEST_COORDS_SRCS) $(TEST_PRINTER_SRCS) \
											 $(TEST_DRIVER_SRCS) $(TEST_PIECE_SRCS) $(TEST_GAMESTATUS_SRCS) \
											 $(TEST_BOARD_SRCS)

# Test .o file paths in output/
TEST_MAIN_OBJ = $(OUTPUT_DIR)/$(TEST_MAIN_SRC:.cpp=.o)
TEST_OBJS = $(addprefix $(OUTPUT_DIR)/,$(ALL_TEST_MODULE_SRCS:.cpp=.o))

# Performance-test file paths
PERF_MAIN_SRC = tests/mainPerformanceTest.cpp
PERF_ENGINE_SRCS = $(wildcard ./engine/test/performance-test/*.cpp)

# Performance-test paths in a single variable
ALL_PERF_MODULE_SRCS = $(PERF_ENGINE_SRCS)

# Performance-test .o paths in output/
PERF_MAIN_OBJ = $(OUTPUT_DIR)/$(PERF_MAIN_SRC:.cpp=.o)
PERF_OBJS = $(addprefix $(OUTPUT_DIR)/,$(ALL_PERF_MODULE_SRCS:.cpp=.o))

# Huge-page TT benchmark
TT_HP_BENCH_SRC = tests/tt_hugepage_bench.cpp
TT_HP_BENCH_OBJ = $(OUTPUT_DIR)/$(TT_HP_BENCH_SRC:.cpp=.o)

# Auto-generated dependency files (header dependencies)
DEPFILES = $(ALL_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(TEST_MAIN_OBJ:.o=.d) $(PERF_OBJS:.o=.d) $(PERF_MAIN_OBJ:.o=.d) $(TT_HP_BENCH_OBJ:.o=.d)

# Main targets
.PHONY: prod prod_windows parallel_prod debug test perf tt-huge-bench all-tests analyze analyze-setup analyze-cppcheck analyze-clang-tidy analyze-iwyu analyze-scan-build analyze-gcc-analyzer analyze-cppclean analyze-lizard analyze-summary complexity test-valgrind cls cls-compile-files help

# Default target for plain `make`
all: PRODFLAGS += -DDEBUG -fomit-frame-pointer -O3
all: prod

# Parallel production build
# Note: faster after the first build but keeps .o files
prod: $(NAME_APP)
	@printf "\n✅ Build completato: $(NAME_APP)\n\n"

# Explicit alias for parallel build
parallel_prod: prod

# Windows production build (cross-compile from Linux)
prod_windows:
	@printf "\nCompiling Windows binary..."
	$(WIN_CXX) $(WIN_PRODFLAGS) $(MAIN_SRC) $(ALL_MODULE_SRCS) -o $(NAME_APP_WIN)
	@printf "\n✅ Build completato: $(NAME_APP_WIN)\n\n"

# Sequential production build
# Note: no slower, but does not generate .o files
prod_sequential:
	$(CXX) $(PRODFLAGS) $(MAIN_SRC) $(ALL_MODULE_SRCS) -o $(NAME_APP)
	@printf "\n✅ Build completato: $(NAME_APP)\n\n"

# Build command for debug executable
# Adds -g to include debug symbols
debug: PRODFLAGS += -DDEBUG -g -pg -O1
debug: $(NAME_APP)
	@printf "\n✅ Build debug completato: $(NAME_APP)\n\n"

# Build command for test executable
test: $(TEST_APP)
	@printf "\n✅ Test compilato: $(TEST_APP)\n\n"

# Build command for performance-test executable
perf: $(PERF_APP)
	@printf "\n✅ Performance test compilato: $(PERF_APP)\n\n"

# Build command for huge-page TT benchmark
tt-huge-bench: $(TT_HP_BENCH_APP)
	@printf "\n✅ TT huge-page benchmark compilato: $(TT_HP_BENCH_APP)\n\n"

# Command to run all tests (functional + performance)
all-tests: test perf
	@printf "\n=== Running functional tests ===\n"
	./$(TEST_APP)
	@printf "\n=== Running performance tests ===\n"
	./$(PERF_APP)
	@printf "\n✅ All tests completed\n\n"

# Final executable generation: 'chess'
$(NAME_APP): $(ALL_OBJS)
	@printf "\nLinking $(NAME_APP)..."
	$(CXX) $(PRODFLAGS) $(ALL_OBJS) -o $(NAME_APP)

# .o file generation
$(OUTPUT_DIR)/%.o: %.cpp
	@printf "\nCompiling $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(PRODFLAGS) -MMD -MP -c $< -o $@

# Final executable generation: 'outputTest'
$(TEST_APP): $(MODULE_OBJS) $(TEST_OBJS) $(TEST_MAIN_OBJ)
	@printf "\nLinking test $(TEST_APP)..."
	$(CXX) $(TEST_FLAGS) $(MODULE_OBJS) $(TEST_OBJS) $(TEST_MAIN_OBJ) -o $(TEST_APP)

# Final executable generation: 'outputPerformance'
$(PERF_APP): $(MODULE_OBJS) $(PERF_OBJS) $(PERF_MAIN_OBJ)
	@printf "\nLinking performance test $(PERF_APP)..."
	$(CXX) $(TEST_FLAGS) $(MODULE_OBJS) $(PERF_OBJS) $(PERF_MAIN_OBJ) -o $(PERF_APP)

# Final executable generation: TT huge-page benchmark
$(TT_HP_BENCH_APP): $(MODULE_OBJS) $(TT_HP_BENCH_OBJ)
	@printf "\nLinking TT huge-page benchmark $(TT_HP_BENCH_APP)..."
	$(CXX) $(PRODFLAGS) $(MODULE_OBJS) $(TT_HP_BENCH_OBJ) -o $(TT_HP_BENCH_APP)

# Analisi completa del codice
analyze: analyze-setup
	@$(MAKE) analyze-cppcheck
	@$(MAKE) analyze-clang-tidy
	@$(MAKE) analyze-iwyu
	@$(MAKE) analyze-scan-build
	@$(MAKE) analyze-gcc-analyzer
	@$(MAKE) analyze-cppclean
	@$(MAKE) analyze-lizard
	@$(MAKE) analyze-summary

analyze-setup:
	@printf "\n=== Complete Static Analysis Suite ===\n"
	@printf "⚠️  This will take some minutes...\n"
	@printf "Analyzing %s files (.cpp + .hpp)...\n\n" $(words $(ALL_ANALYSIS_FILES))
	@mkdir -p doc/output-analisi
	@rm -f analisi.log
	@rm -rf scan-build-report
	@rm -f complexity-report.csv
	@printf "=== STATIC ANALYSIS REPORT ===\n" > analisi.log
	@printf "Date: %s\n" "$$(date '+%Y-%m-%d %H:%M:%S')" >> analisi.log
	@printf "Files analyzed: %s\n\n" $(words $(ALL_ANALYSIS_FILES)) >> analisi.log
	@if command -v bear >/dev/null 2>&1; then \
		bear -- $(MAKE) prod > /dev/null 2>&1; \
	else \
		printf "⚠️  bear non trovato: skip compile_commands.json generation\n"; \
	fi
	@printf "[0/7] Generating compilation database\n"

analyze-cppcheck:
	@printf "\n========================================\n" >> analisi.log
	@printf "1. CPPCHECK ANALYSIS\n" >> analisi.log
	@printf "========================================\n\n" >> analisi.log
	@$(CPPCHECK) $(ALL_ANALYSIS_FILES) --check-level=exhaustive --enable=all --suppress=missingIncludeSystem --inline-suppr --std=c++23 --quiet >> analisi.log 2>&1
	@printf "[1/7] Running cppcheck terminated\n"

analyze-clang-tidy:
	@printf "\n========================================\n" >> analisi.log
	@printf "2. CLANG-TIDY ANALYSIS\n" >> analisi.log
	@printf "========================================\n\n" >> analisi.log
	@clang-tidy -p $(COMPILATION_DB) $(MAIN_SRC) $(ALL_MODULE_SRCS) $(ALL_MODULE_HDRS) >> analisi.log 2>&1 || echo "clang-tidy exited with error code $$?" >> analisi.log
	@printf "[2/7] Running clang-tidy terminated\n"

analyze-iwyu:
	@printf "\n========================================\n" >> analisi.log
	@printf "3. INCLUDE-WHAT-YOU-USE ANALYSIS\n" >> analisi.log
	@printf "========================================\n\n" >> analisi.log
	@for file in $(MAIN_SRC) $(ALL_MODULE_SRCS); do \
		printf "Checking $$file...\n" >> analisi.log; \
		include-what-you-use -std=c++23 -fopenmp -DDEBUG -march=native $$file >> analisi.log 2>&1 || true; \
	done 2>/dev/null
	@printf "[3/7] Running include-what-you-use terminated\n"

analyze-scan-build:
	@printf "\n========================================\n" >> analisi.log
	@printf "4. SCAN-BUILD (CLANG STATIC ANALYZER)\n" >> analisi.log
	@printf "========================================\n\n" >> analisi.log
	@printf "Path-sensitive dataflow analysis in progress...\n" >> analisi.log
	@$(MAKE) cls-compile-files > /dev/null
	@scan-build -o scan-build-report --status-bugs -v -analyzer-config aggressive-binary-operation-simplification=true $(MAKE) prod >> analisi.log 2>&1 || echo "scan-build exited with error code $$?" >>  analisi.log
	@printf "\n--- Scan-build text summary ---\n" >> analisi.log
	@if [ -d scan-build-report ] && [ -n "$$(ls -A scan-build-report 2>/dev/null)" ]; then \
		printf "HTML Report location: scan-build-report/\n" >> analisi.log; \
		find scan-build-report -name "*.html" -type f | head -1 | xargs -I {} printf "Main report: {}\n" >> analisi.log 2>&1; \
		printf "\nBugs found by scan-build:\n" >> analisi.log; \
		if ! find scan-build-report -name "*.html" -exec grep -h "<!-- BUGTYPE" {} \; 2>/dev/null | sort | uniq -c >> analisi.log; then \
			printf "No bugs found or unable to parse HTML reports\n" >> analisi.log; \
		fi; \
	else \
		printf "No bugs detected by scan-build!\n" >> analisi.log; \
	fi
	@printf "[4/7] Running scan-build terminated\n"

analyze-gcc-analyzer:
	@printf "\n========================================\n" >> analisi.log
	@printf "5. GCC STATIC ANALYZER\n" >> analisi.log
	@printf "========================================\n\n" >> analisi.log
	@$(MAKE) cls-compile-files > /dev/null
	@$(CXX) -fanalyzer -fsyntax-only $(PRODFLAGS) $(MAIN_SRC) $(ALL_MODULE_SRCS) >> analisi.log 2>&1 || echo "GCC analyzer exited with error code $$?" >> analisi.log
	@printf "[5/7] Running GCC analyzer terminated\n"

analyze-cppclean:
	@printf "\n========================================\n" >> analisi.log
	@printf "6. CPPCLEAN (DEAD CODE & DEPENDENCIES)\n" >> analisi.log
	@printf "========================================\n\n" >> analisi.log
	@cppclean . >> analisi.log 2>&1 || echo "cppclean exited with error code $$?" >> analisi.log
	@printf "[6/7] Running cppclean terminated\n"

analyze-lizard:
	@printf "\n========================================\n" >> analisi.log
	@printf "7. LIZARD COMPLEXITY ANALYSIS\n" >> analisi.log
	@printf "========================================\n\n" >> analisi.log
	@if [ -f script/lizard-1.19.0/lizard.py ]; then \
		python3 script/lizard-1.19.0/lizard.py -l cpp -w -L 60 -C 15 --csv . > complexity-report.csv 2>&1 || true; \
		python3 script/lizard-1.19.0/lizard.py -l cpp -w -L 60 -C 15 . >> analisi.log 2>&1 || true; \
		printf "[7/7] Running lizard terminated\n"; \
	else \
		printf "Lizard not found - skipping\n" >> analisi.log; \
		printf "[7/7] Running lizard skipped\n"; \
	fi

analyze-summary:
	@printf "\n========================================\n" >> analisi.log
	@printf "COMPLETE ANALYSIS FINISHED\n" >> analisi.log
	@printf "========================================\n" >> analisi.log
	@printf "\n========================================\n"
	@printf "✅ Complete analysis finished!\n"
	@printf "========================================\n"
	@printf "\n📊 Reports generated:\n"
	@printf "   1. analisi.log (main report)\n"
	@printf "   2. scan-build-report/*/index.html (interactive HTML)\n"
	@if [ -f complexity-report.csv ]; then \
		printf "   3. complexity-report.csv (spreadsheet)\n"; \
	fi
	@printf "\n[7/7] Generating LLM prompt...\n\n"
	@printf "=== LLM PROMPT (copy below) ===\n\n"
	@cat doc/prompt-llm-riassunto-analisi-statica.txt
	@printf "\n\n=== END PROMPT ===\n"

# Alias per analisi complessità
complexity: analyze-lizard

# Memory leak detection con report salvato su file
test-valgrind: $(TEST_APP)
	@mkdir -p doc/output-analisi
	@if ! command -v valgrind >/dev/null 2>&1; then \
		printf "❌ valgrind non trovato. Installa con: sudo apt install valgrind\n"; \
		exit 1; \
	fi
	@printf "\nRunning valgrind on $(TEST_APP)...\n"
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--error-exitcode=1 ./$(TEST_APP) > valgrind-report.txt 2>&1
	@printf "✅ Valgrind report: valgrind-report.txt\n\n"

# Include dependency files generated by -MMD -MP
-include $(DEPFILES)

# Clean object files and binaries only
cls-compile-files:
	rm -rf $(OUTPUT_DIR)
	rm -f $(GENERATED_BINS)

# Clean temporary files
cls: cls-compile-files
	@printf "\nCleaning..."
	rm -f doc/main-doc.{aux,log,pdf,toc}
	rm -f compile_commands.json
	#rm -rf doc/output-analisi
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
	@printf "  make prod_windows   - Cross-compilazione Windows (richiede mingw-w64)\n"
	@printf "  make debug          - Compilazione con debug symbols\n"
	@printf "  make test           - Compilazione e test funzionali\n"
	@printf "  make perf           - Compilazione e performance test\n"
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
	@printf "  - analisi.log                    (main text report)\n"
	@printf "  - scan-build-report/*.html       (interactive HTML)\n"
	@printf "  - complexity-report.txt/.csv     (complexity metrics)\n"
	@printf "  - valgrind-report.txt            (memory analysis)\n"
	@printf "\n⚙️  TOOL LOCATIONS:\n"
	@printf "  - lizard: script/lizard-1.19.0/lizard.py\n"
	@printf "  - valgrind: install with 'sudo apt install valgrind'\n"
	@printf "\n==================================\n"

# Print variables
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
