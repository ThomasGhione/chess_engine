#include "../engine/engine.hpp"
#include "../board/piece.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

struct BenchConfig {
    int depth = 8;
    int repeats = 2;
    bool perFen = false;
};

bool parsePositiveInt(const char* text, int& outValue) {
    if (text == nullptr || *text == '\0') return false;
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed <= 0 || parsed > 1000) return false;
    outValue = static_cast<int>(parsed);
    return true;
}

bool parseArgs(int argc, char** argv, BenchConfig& cfg) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--depth" && (i + 1) < argc) {
            if (!parsePositiveInt(argv[++i], cfg.depth)) return false;
            continue;
        }
        if (arg == "--repeats" && (i + 1) < argc) {
            if (!parsePositiveInt(argv[++i], cfg.repeats)) return false;
            continue;
        }
        if (arg == "--per-fen") {
            cfg.perFen = true;
            continue;
        }
        return false;
    }
    return true;
}

void printUsage(const char* exeName) {
    std::cerr << "Usage: " << exeName << " [--depth N] [--repeats N] [--per-fen]\n";
}

} // namespace

int main(int argc, char** argv) {
    BenchConfig cfg;
    if (!parseArgs(argc, argv, cfg)) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // Ensure attack tables are initialized outside timing.
    pieces::initMagicBitboards();

    constexpr std::array<const char*, 16> FENS = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkbnr/pppp1ppp/2n5/4p3/2P1P3/2N5/PP1P1PPP/R1BQKBNR w KQkq - 0 3",
        "rnbq1rk1/pp2bppp/3ppn2/2p5/2P1P3/2N2N2/PP1P1PPP/R1BQ1RK1 w - - 2 8",
        "r2q1rk1/pp2bppp/2np1n2/2p1p3/2P1P3/2NP1N2/PP2BPPP/R1BQ1RK1 w - - 0 9",
        "r1bq1rk1/pp3ppp/2np1n2/2p1p3/2P1P3/1PNP1N2/PB2BPPP/R2Q1RK1 w - - 3 10",
        "r3k2r/pppq1ppp/2npbn2/3Np3/2P1P3/2N1B3/PP2QPPP/R3KB1R w KQkq - 2 10",
        "2rq1rk1/1b2bppp/p2ppn2/1p6/3NP3/1BN1B3/PPQ2PPP/2RR2K1 w - - 0 17",
        "r3k2r/6pp/pp1bbp2/2p5/Rn2P3/1N3P1P/1P1N1BP1/2R3K1 w kq - 4 20",
        "r6r/3k2pp/pBR1bp2/8/1p2Pb2/1N3P1P/1P1N2P1/6K1 w - - 3 23",
        "2r3k1/5ppp/p2bpn2/1p6/3P4/1P3NP1/PB3PBP/2R3K1 w - - 0 25",
        "8/2p5/3p2k1/2P1p1p1/4P3/3P1K2/8/8 w - - 0 40",
        "4rrk1/pp3ppp/2n1bn2/2bp4/3P4/2P1PN2/PP1NBPPP/R2QR1K1 w - - 4 13",
        "2r2rk1/pp3ppp/2n1bn2/3p4/3P4/2P1PN2/PP1N1PPP/2RR2K1 w - - 0 16",
        "3r2k1/pp1n1ppp/2p1pn2/3p4/3P4/2P1PN2/PP1N1PPP/3R2K1 w - - 0 20",
        "6k1/1p3pp1/p1p1p2p/3nP3/3P1P2/2P3P1/PP4KP/3r4 w - - 0 33",
        "8/5pk1/3p1np1/2pPp3/2P1P3/2N3P1/5PKP/8 w - - 3 45"
    };

    engine::Engine engine;

    uint64_t totalNodes = 0;
    uint64_t totalTimeNs = 0;
    uint64_t totalSearches = 0;
    std::string firstMoveUci;

    for (int r = 0; r < cfg.repeats; ++r) {
        for (std::size_t i = 0; i < FENS.size(); ++i) {
            engine.board = chess::Board(FENS[i]);
            engine.bestMove = chess::Board::Move{};
            engine.moveHistory.clear();
            engine.updateGameResult();

            const auto start = std::chrono::steady_clock::now();
            const chess::Board::Move bestMove = engine.searchUCI(engine::time::Limits{.maxDepth = static_cast<int64_t>(cfg.depth)});
            const auto stop = std::chrono::steady_clock::now();
            const uint64_t elapsedNs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
            const uint64_t nodes = engine.searchRuntime.nodesSearched;

            if (firstMoveUci.empty()) firstMoveUci = bestMove.toUCIString();

            totalNodes += nodes;
            totalTimeNs += elapsedNs;
            ++totalSearches;

            if (cfg.perFen) {
                const double ms = static_cast<double>(elapsedNs) / 1e6;
                const double nps = (elapsedNs > 0)
                    ? (static_cast<double>(nodes) * 1e9 / static_cast<double>(elapsedNs))
                    : 0.0;
                std::cout << "FEN[" << i << "] rep=" << r
                          << " nodes=" << nodes
                          << " time_ms=" << ms
                          << " nps=" << nps
                          << " bestmove=" << bestMove.toUCIString() << '\n';
            }
        }
    }

    const double totalMs = static_cast<double>(totalTimeNs) / 1e6;
    const double avgNodes = (totalSearches > 0) ? static_cast<double>(totalNodes) / static_cast<double>(totalSearches) : 0.0;
    const double avgMs = (totalSearches > 0) ? totalMs / static_cast<double>(totalSearches) : 0.0;
    const double nps = (totalTimeNs > 0)
        ? (static_cast<double>(totalNodes) * 1e9 / static_cast<double>(totalTimeNs))
        : 0.0;

    std::cout << "SUMMARY depth=" << cfg.depth
              << " repeats=" << cfg.repeats
              << " fen_count=" << FENS.size()
              << " searches=" << totalSearches
              << " total_nodes=" << totalNodes
              << " total_ms=" << totalMs
              << " avg_nodes=" << avgNodes
              << " avg_ms=" << avgMs
              << " nps=" << nps
              << " first_bestmove=" << (firstMoveUci.empty() ? "0000" : firstMoveUci)
              << '\n';

    return EXIT_SUCCESS;
}
