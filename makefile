SHELL := /bin/bash
ROOT_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

# ============================================================================
# DIRECTORY STRUCTURE
# ============================================================================
OUTPUT_DIR   := $(ROOT_DIR)/output
TESTS_DIR    := $(ROOT_DIR)/tests
SCRIPT_DIR   := $(ROOT_DIR)/script
DOC_DIR      := $(ROOT_DIR)/doc
ANALYSIS_DIR := $(DOC_DIR)/analysis-output

# ============================================================================
# COMPILER & FLAGS
# ============================================================================
CXX = g++
WIN_CXX = x86_64-w64-mingw32-g++

# Common C++ standard and warnings
COMMON_FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -fopenmp -march=native

# Test flags: O3 optimization + debug symbols + LTO
TEST_FLAGS = $(COMMON_FLAGS) -O3 -DDEBUG -flto=8 -fext-numeric-literals -g

# Production flags: balanced optimization + LTO + function sections
PRODFLAGS = $(COMMON_FLAGS) -mtune=native -flto=auto -fno-math-errno \
            -fno-trapping-math -funroll-loops -ffunction-sections -fdata-sections -O3

# Windows cross-compile flags
WIN_PRODFLAGS = $(COMMON_FLAGS) -O3 -DDEBUG -static -static-libgcc -static-libstdc++ -flto=4 -fext-numeric-literals

# ============================================================================
# BUILD TOOLS & ANALYSIS
# ============================================================================
CPPCHECK             := $(SCRIPT_DIR)/cppcheck-2.19.0/cppcheck
COMPILATION_DB       := $(ROOT_DIR)/compile_commands.json
LIZARD               := $(SCRIPT_DIR)/lizard-1.19.0/lizard.py
GPROF2DOT            := $(SCRIPT_DIR)/gprof2dot.py
LLM_ANALYSIS_PROMPT  := $(DOC_DIR)/static-analysis-summary-prompt.txt

# Output reports
ANALYSIS_LOG         := $(ROOT_DIR)/analysis.log
COMPLEXITY_REPORT    := $(ROOT_DIR)/complexity-report.csv
VALGRIND_REPORT      := $(ROOT_DIR)/valgrind-report.txt
SCAN_BUILD_DIR       := $(ROOT_DIR)/scan-build-report
GPROF_OUTPUT         := $(ROOT_DIR)/gmon.out
PROFILE_IMAGE        := $(ROOT_DIR)/output.png

# ============================================================================
# SOURCE FILES: Helper function for wildcards
# ============================================================================
SRC_WILDCARD = $(wildcard $(ROOT_DIR)/$(1)/*.cpp) $(wildcard $(ROOT_DIR)/$(1)/*/*.cpp)
HDR_WILDCARD = $(wildcard $(ROOT_DIR)/$(1)/*.hpp) $(wildcard $(ROOT_DIR)/$(1)/*/*.hpp)

# Main + Module sources
MAIN_SRC      = $(ROOT_DIR)/main.cpp
ENGINE_SRCS   = $(call SRC_WILDCARD,engine)
DRIVER_SRCS   = $(call SRC_WILDCARD,driver)
BOARD_SRCS    = $(call SRC_WILDCARD,board)
UCI_SRCS      = $(call SRC_WILDCARD,uci)
ALL_MODULE_SRCS = $(ENGINE_SRCS) $(DRIVER_SRCS) $(BOARD_SRCS) $(UCI_SRCS)

# Module headers
ENGINE_HDRS  = $(call HDR_WILDCARD,engine)
DRIVER_HDRS  = $(call HDR_WILDCARD,driver)
BOARD_HDRS   = $(call HDR_WILDCARD,board)
UCI_HDRS     = $(call HDR_WILDCARD,uci)
ALL_MODULE_HDRS = $(ENGINE_HDRS) $(DRIVER_HDRS) $(BOARD_HDRS) $(UCI_HDRS)

# Analysis files (main code only, excluding tests)
ALL_ANALYSIS_FILES = $(MAIN_SRC) $(ALL_MODULE_SRCS) $(ALL_MODULE_HDRS)

# ============================================================================
# TEST SOURCES
# ============================================================================
TEST_MAIN_SRC       = $(TESTS_DIR)/mainTest.cpp
TEST_MODULE_SRCS    = $(call SRC_WILDCARD,engine/test) $(call SRC_WILDCARD,driver/test) $(call SRC_WILDCARD,board/test)
PERF_MAIN_SRC       = $(TESTS_DIR)/mainPerformanceTest.cpp
PERF_MODULE_SRCS    = $(call SRC_WILDCARD,engine/test/performance-test)
TT_HP_BENCH_SRC     = $(TESTS_DIR)/tt_hugepage_bench.cpp

# ============================================================================
# OBJECT FILES: Helper function for .o paths
# ============================================================================
OBJ_FROM_SRC = $(patsubst $(ROOT_DIR)/%.cpp,$(OUTPUT_DIR)/%.o,$(1))

# Main + Module objects
MAIN_OBJ     = $(call OBJ_FROM_SRC,$(MAIN_SRC))
MODULE_OBJS  = $(call OBJ_FROM_SRC,$(ALL_MODULE_SRCS))
ALL_OBJS     = $(MAIN_OBJ) $(MODULE_OBJS)

# Test objects
TEST_MAIN_OBJ  = $(call OBJ_FROM_SRC,$(TEST_MAIN_SRC))
TEST_OBJS      = $(call OBJ_FROM_SRC,$(TEST_MODULE_SRCS))
PERF_MAIN_OBJ  = $(call OBJ_FROM_SRC,$(PERF_MAIN_SRC))
PERF_OBJS      = $(call OBJ_FROM_SRC,$(PERF_MODULE_SRCS))
TT_HP_BENCH_OBJ = $(call OBJ_FROM_SRC,$(TT_HP_BENCH_SRC))

# ============================================================================
# OUTPUT BINARIES
# ============================================================================
NAME_APP        = $(ROOT_DIR)/chess
NAME_APP_WIN    = $(ROOT_DIR)/chess.exe
TEST_APP        = $(TESTS_DIR)/test
PERF_APP        = $(TESTS_DIR)/perf
TT_HP_BENCH_APP = $(TESTS_DIR)/tt_hugepage_bench
GENERATED_BINS  = $(NAME_APP) $(NAME_APP_WIN) $(TEST_APP) $(PERF_APP) $(TT_HP_BENCH_APP)

# ============================================================================
# DEPENDENCY FILES
# ============================================================================
DEPFILES = $(ALL_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(TEST_MAIN_OBJ:.o=.d) \
           $(PERF_OBJS:.o=.d) $(PERF_MAIN_OBJ:.o=.d) $(TT_HP_BENCH_OBJ:.o=.d)

# ============================================================================
# PARALLEL BUILD
# ============================================================================
NUMBER_OF_CORES := $(shell nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
ifeq ($(MAKELEVEL),0)
ifeq ($(filter -j%,$(MAKEFLAGS)),)
MAKEFLAGS += -j$(NUMBER_OF_CORES)
endif
endif

# ============================================================================
# BUILD TARGETS
# ============================================================================
.PHONY: all prod prod_windows prod_sequential debug test perf tt-huge-bench all-tests \
        analyze analyze-setup analyze-cppcheck analyze-clang-tidy analyze-iwyu \
        analyze-scan-build analyze-gcc-analyzer analyze-cppclean analyze-lizard \
        analyze-summary complexity test-valgrind cls cls-compile-files get-image help debug-vars

# Default target
all: prod

# Main build targets
chess prod: $(NAME_APP)
	@printf "\n✓ Build completed: $(NAME_APP)\n\n"

prod_windows:
	@printf "✓ Compiling Windows binary...\n"
	$(WIN_CXX) $(WIN_PRODFLAGS) $(MAIN_SRC) $(ALL_MODULE_SRCS) -o $(NAME_APP_WIN)
	@printf "✓ Build completed: $(NAME_APP_WIN)\n\n"

prod_sequential:
	$(CXX) $(PRODFLAGS) $(MAIN_SRC) $(ALL_MODULE_SRCS) -o $(NAME_APP)
	@printf "✓ Build completed: $(NAME_APP)\n\n"

debug: PRODFLAGS += -DDEBUG -g -pg -O1
debug: $(NAME_APP)
	@printf "✓ Debug build completed: $(NAME_APP)\n\n"

# Test targets
test: $(TEST_APP)
	@printf "✓ Test binary built: $(TEST_APP)\n\n"

perf: $(PERF_APP)
	@printf "✓ Performance-test binary built: $(PERF_APP)\n\n"

tt-huge-bench: $(TT_HP_BENCH_APP)
	@printf "✓ TT huge-page benchmark built: $(TT_HP_BENCH_APP)\n\n"

all-tests: test perf
	@printf "\n=== Running functional tests ===\n"
	$(TEST_APP)
	@printf "\n=== Running performance tests ===\n"
	$(PERF_APP)
	@printf "\n✓ All tests completed\n\n"

# ============================================================================
# COMPILATION RULES
# ============================================================================
# Main executable linking
$(NAME_APP): $(ALL_OBJS)
	@printf "Linking $(NAME_APP)...\n"
	$(CXX) $(PRODFLAGS) $(ALL_OBJS) -o $(NAME_APP)

# Object file compilation
$(OUTPUT_DIR)/%.o: $(ROOT_DIR)/%.cpp
	@printf "Compiling $<...\n"
	@mkdir -p $(dir $@)
	$(CXX) $(PRODFLAGS) -MMD -MP -c $< -o $@

# Test executable linking
$(TEST_APP): $(MODULE_OBJS) $(TEST_OBJS) $(TEST_MAIN_OBJ)
	@printf "Linking test executable...\n"
	$(CXX) $(TEST_FLAGS) $(MODULE_OBJS) $(TEST_OBJS) $(TEST_MAIN_OBJ) -o $(TEST_APP)

# Performance test executable linking
$(PERF_APP): $(MODULE_OBJS) $(PERF_OBJS) $(PERF_MAIN_OBJ)
	@printf "Linking performance test executable...\n"
	$(CXX) $(TEST_FLAGS) $(MODULE_OBJS) $(PERF_OBJS) $(PERF_MAIN_OBJ) -o $(PERF_APP)

# TT huge-page benchmark linking
$(TT_HP_BENCH_APP): $(MODULE_OBJS) $(TT_HP_BENCH_OBJ)
	@printf "Linking TT huge-page benchmark...\n"
	$(CXX) $(PRODFLAGS) $(MODULE_OBJS) $(TT_HP_BENCH_OBJ) -o $(TT_HP_BENCH_APP)

# ============================================================================
# STATIC ANALYSIS
# ============================================================================
analyze: analyze-setup
	@$(MAKE) -C $(ROOT_DIR) analyze-cppcheck
	@$(MAKE) -C $(ROOT_DIR) analyze-clang-tidy
	@$(MAKE) -C $(ROOT_DIR) analyze-iwyu
	@$(MAKE) -C $(ROOT_DIR) analyze-scan-build
	@$(MAKE) -C $(ROOT_DIR) analyze-gcc-analyzer
	@$(MAKE) -C $(ROOT_DIR) analyze-cppclean
	@$(MAKE) -C $(ROOT_DIR) analyze-lizard
	@$(MAKE) -C $(ROOT_DIR) analyze-summary

analyze-setup:
	@printf "\n╔════════════════════════════════════════╗\n"
	@printf "║  Complete Static Analysis Suite      ║\n"
	@printf "╚════════════════════════════════════════╝\n"
	@printf "This will take ~5 minutes...\n"
	@printf "Analyzing %s files (.cpp + .hpp)...\n\n" $(words $(ALL_ANALYSIS_FILES))
	@mkdir -p $(ANALYSIS_DIR)
	@rm -f $(ANALYSIS_LOG)
	@rm -rf $(SCAN_BUILD_DIR)
	@rm -f $(COMPLEXITY_REPORT)
	@printf "=== STATIC ANALYSIS REPORT ===\n" > $(ANALYSIS_LOG)
	@printf "Date: %s\n" "$$(date '+%Y-%m-%d %H:%M:%S')" >> $(ANALYSIS_LOG)
	@printf "Files analyzed: %s\n\n" $(words $(ALL_ANALYSIS_FILES)) >> $(ANALYSIS_LOG)
	@if command -v bear >/dev/null 2>&1; then \
		cd $(ROOT_DIR) && bear -- $(MAKE) prod > /dev/null 2>&1; \
	else \
		printf "⚠ bear not found: skipping compile_commands.json\n"; \
	fi
	@printf "[0/7] Generating compilation database\n"

# Individual analysis tools
define ANALYZE_TOOL
@printf "\n========================================\n" >> $(ANALYSIS_LOG)
@printf "$(1)\n" >> $(ANALYSIS_LOG)
@printf "========================================\n\n" >> $(ANALYSIS_LOG)
endef

analyze-cppcheck:
	$(call ANALYZE_TOOL,1. CPPCHECK ANALYSIS)
	@$(CPPCHECK) $(ALL_ANALYSIS_FILES) --check-level=exhaustive --enable=all \
		--suppress=missingIncludeSystem --inline-suppr --std=c++23 --quiet >> $(ANALYSIS_LOG) 2>&1
	@printf "[1/7] Running cppcheck terminated\n"

analyze-clang-tidy:
	$(call ANALYZE_TOOL,2. CLANG-TIDY ANALYSIS)
	@clang-tidy -p $(COMPILATION_DB) $(MAIN_SRC) $(ALL_MODULE_SRCS) $(ALL_MODULE_HDRS) >> $(ANALYSIS_LOG) 2>&1 || true
	@printf "[2/7] Running clang-tidy terminated\n"

analyze-iwyu:
	$(call ANALYZE_TOOL,3. INCLUDE-WHAT-YOU-USE ANALYSIS)
	@for file in $(MAIN_SRC) $(ALL_MODULE_SRCS); do \
		printf "Checking $$file...\n" >> $(ANALYSIS_LOG); \
		include-what-you-use -std=c++23 -fopenmp -DDEBUG -march=native $$file >> $(ANALYSIS_LOG) 2>&1 || true; \
	done 2>/dev/null
	@printf "[3/7] Running include-what-you-use terminated\n"

analyze-scan-build:
	$(call ANALYZE_TOOL,4. SCAN-BUILD (CLANG STATIC ANALYZER))
	@printf "Path-sensitive dataflow analysis in progress...\n" >> $(ANALYSIS_LOG)
	@$(MAKE) -C $(ROOT_DIR) cls-compile-files > /dev/null
	@scan-build -o $(SCAN_BUILD_DIR) --status-bugs -v \
		-analyzer-config aggressive-binary-operation-simplification=true \
		$(MAKE) -C $(ROOT_DIR) prod >> $(ANALYSIS_LOG) 2>&1 || true
	@printf "\n--- Scan-build summary ---\n" >> $(ANALYSIS_LOG)
	@if [ -d $(SCAN_BUILD_DIR) ] && [ -n "$$(ls -A $(SCAN_BUILD_DIR) 2>/dev/null)" ]; then \
		printf "HTML Report: $(SCAN_BUILD_DIR)/\n" >> $(ANALYSIS_LOG); \
		find $(SCAN_BUILD_DIR) -name "*.html" -type f | head -1 | xargs -I {} printf "Main: {}\n" >> $(ANALYSIS_LOG) 2>&1; \
	else \
		printf "✓ No bugs detected by scan-build!\n" >> $(ANALYSIS_LOG); \
	fi
	@printf "[4/7] Running scan-build terminated\n"

analyze-gcc-analyzer:
	$(call ANALYZE_TOOL,5. GCC STATIC ANALYZER)
	@$(MAKE) -C $(ROOT_DIR) cls-compile-files > /dev/null
	@$(CXX) -fanalyzer -fsyntax-only $(PRODFLAGS) $(MAIN_SRC) $(ALL_MODULE_SRCS) >> $(ANALYSIS_LOG) 2>&1 || true
	@printf "[5/7] Running GCC analyzer terminated\n"

analyze-cppclean:
	$(call ANALYZE_TOOL,6. CPPCLEAN (DEAD CODE & DEPENDENCIES))
	@cd $(ROOT_DIR) && cppclean . >> $(ANALYSIS_LOG) 2>&1 || true
	@printf "[6/7] Running cppclean terminated\n"

analyze-lizard:
	$(call ANALYZE_TOOL,7. LIZARD COMPLEXITY ANALYSIS)
	@if [ -f $(LIZARD) ]; then \
		python3 $(LIZARD) -l cpp -w -L 60 -C 15 --csv $(ROOT_DIR) > $(COMPLEXITY_REPORT) 2>&1 || true; \
		python3 $(LIZARD) -l cpp -w -L 60 -C 15 $(ROOT_DIR) >> $(ANALYSIS_LOG) 2>&1 || true; \
		printf "[7/7] Running lizard terminated\n"; \
	else \
		printf "⚠ Lizard not found - skipping\n" >> $(ANALYSIS_LOG); \
		printf "[7/7] Running lizard skipped\n"; \
	fi

analyze-summary:
	@printf "\n========================================\n" >> $(ANALYSIS_LOG)
	@printf "ANALYSIS FINISHED\n" >> $(ANALYSIS_LOG)
	@printf "========================================\n" >> $(ANALYSIS_LOG)
	@printf "\n╔════════════════════════════════════════╗\n"
	@printf "║  ✓ Analysis Finished!                ║\n"
	@printf "╚════════════════════════════════════════╝\n\n"
	@printf "Reports:\n"
	@printf "  1. $(ANALYSIS_LOG)\n"
	@printf "  2. $(SCAN_BUILD_DIR)/*/index.html\n"
	@if [ -f $(COMPLEXITY_REPORT) ]; then \
		printf "  3. $(COMPLEXITY_REPORT)\n"; \
	fi
	@printf "\nLLM Prompt:\n"
	@if [ -f $(LLM_ANALYSIS_PROMPT) ]; then \
		cat $(LLM_ANALYSIS_PROMPT); \
	else \
		printf "⚠ Prompt template not found\n"; \
	fi

complexity: analyze-lizard

# ============================================================================
# RUNTIME TESTING & UTILITIES
# ============================================================================
test-valgrind: $(TEST_APP)
	@mkdir -p $(ANALYSIS_DIR)
	@if ! command -v valgrind >/dev/null 2>&1; then \
		printf "❌ valgrind not found. Install: sudo apt install valgrind\n"; \
		exit 1; \
	fi
	@printf "Running valgrind on $(TEST_APP)...\n"
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--error-exitcode=1 $(TEST_APP) > $(VALGRIND_REPORT) 2>&1
	@printf "✓ Report: $(VALGRIND_REPORT)\n\n"

cls-compile-files:
	rm -rf $(OUTPUT_DIR)
	rm -f $(GENERATED_BINS)

cls: cls-compile-files
	@printf "Cleaning...\n"
	rm -f $(DOC_DIR)/main-doc.{aux,log,pdf,toc}
	rm -f $(COMPILATION_DB)
	rm -rf $(GPROF_OUTPUT)
	@printf "✓ Clean completed\n\n"

get-image:
	gprof $(NAME_APP) $(GPROF_OUTPUT) | python3 $(GPROF2DOT) -s -w | dot -Tpng -Gdpi=200 -o $(PROFILE_IMAGE)
	@printf "✓ Profile image: $(PROFILE_IMAGE)\n\n"

# ============================================================================
# DEPENDENCY TRACKING & HELP
# ============================================================================
-include $(DEPFILES)

help:
	@printf "\n╔════════════════════════════════════════╗\n"
	@printf "║  Chess Engine Build System           ║\n"
	@printf "╚════════════════════════════════════════╝\n\n"
	@printf "BUILD TARGETS:\n"
	@printf "  prod              Optimized production build\n"
	@printf "  prod_windows      Cross-compile for Windows\n"
	@printf "  prod_sequential   Sequential (no parallel jobs)\n"
	@printf "  debug             Build with debug symbols + gprof\n"
	@printf "  test              Build functional tests\n"
	@printf "  perf              Build performance tests\n"
	@printf "  tt-huge-bench     Build TT huge-page benchmark\n"
	@printf "  all-tests         Run all tests\n\n"
	@printf "STATIC ANALYSIS:\n"
	@printf "  analyze           Full suite (cppcheck, clang-tidy, scan-build, etc.)\n"
	@printf "  complexity        Code complexity metrics (lizard)\n\n"
	@printf "RUNTIME TESTING:\n"
	@printf "  test-valgrind     Memory leak detection\n\n"
	@printf "UTILITIES:\n"
	@printf "  cls               Clean compiled files\n"
	@printf "  get-image         Generate gprof call graph\n"
	@printf "  debug-vars        Show makefile variables\n"
	@printf "  help              Show this message\n\n"
	@printf "REPORTS:\n"
	@printf "  $(ANALYSIS_LOG)\n"
	@printf "  $(SCAN_BUILD_DIR)/\n"
	@printf "  $(COMPLEXITY_REPORT)\n"
	@printf "  $(VALGRIND_REPORT)\n\n"

debug-vars:
	@printf "\nCOMPILER: $(CXX)\n"
	@printf "CORES: $(NUMBER_OF_CORES)\n"
	@printf "TEST_FLAGS: $(TEST_FLAGS)\n"
	@printf "PRODFLAGS: $(PRODFLAGS)\n"
	@printf "\nMODULE_SRCS: $(words $(ALL_MODULE_SRCS)) files\n"
	@printf "MODULE_OBJS: $(words $(MODULE_OBJS)) objects\n"
	@printf "TOTAL_ANALYSIS_FILES: $(words $(ALL_ANALYSIS_FILES))\n\n"
