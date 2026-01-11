#!/usr/bin/env python3
"""
Script per confrontare le prestazioni tra due log di partite.
Estrae i tempi di ricerca e calcola le differenze (speedup/slowdown).

Usage:
    python compare_timings.py partita.log partita2.log
    python compare_timings.py  # usa file predefiniti
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple


def extract_timings(log_file: Path) -> List[Tuple[float, int]]:
    """
    Estrae tempi di ricerca e nodi visitati dal file di log.
    
    Returns:
        Lista di tuple (tempo_ms, nodi)
    """
    timings = []
    
    with open(log_file, 'r', encoding='utf-8') as f:
        time_ms = None
        for line in f:
            # Pattern: [DEBUG] Engine search: 128.641ms.
            time_match = re.search(r'\[DEBUG\] Engine search: ([\d.]+)ms', line)
            if time_match:
                time_ms = float(time_match.group(1))
            
            # Pattern: [DEBUG] Nodes visited: 143746
            nodes_match = re.search(r'\[DEBUG\] Nodes visited: (\d+)', line)
            if nodes_match and time_ms is not None:
                nodes = int(nodes_match.group(1))
                timings.append((time_ms, nodes))
                time_ms = None  # Reset per la prossima coppia
    
    return timings


def compare_timings(file1: Path, file2: Path):
    """
    Confronta i timing tra due file di log e stampa statistiche.
    """
    timings1 = extract_timings(file1)
    timings2 = extract_timings(file2)
    
    if len(timings1) != len(timings2):
        print(f"⚠️  Warning: diverso numero di mosse ({len(timings1)} vs {len(timings2)})")
        min_len = min(len(timings1), len(timings2))
        timings1 = timings1[:min_len]
        timings2 = timings2[:min_len]
    
    print(f"{'Move':<6} {'Before (ms)':<14} {'After (ms)':<14} {'Diff (ms)':<12} {'Speedup':<10} {'Nodes Δ':<10}")
    print("=" * 80)
    
    total_time1 = 0.0
    total_time2 = 0.0
    improvements = 0
    regressions = 0
    
    for i, ((t1, n1), (t2, n2)) in enumerate(zip(timings1, timings2), start=1):
        diff = t1 - t2
        speedup = (t1 / t2) if t2 > 0 else float('inf')
        nodes_diff = n2 - n1
        
        total_time1 += t1
        total_time2 += t2
        
        # Simbolo per visualizzare miglioramento/peggioramento
        if diff > 0:
            symbol = "✓"
            improvements += 1
        elif diff < 0:
            symbol = "✗"
            regressions += 1
        else:
            symbol = "="
        
        print(f"{i:<6} {t1:<14.3f} {t2:<14.3f} {diff:>+10.3f}  {symbol} {speedup:<10.3f}x {nodes_diff:>+9}")
    
    print("=" * 80)
    print(f"{'TOTAL':<6} {total_time1:<14.3f} {total_time2:<14.3f} {total_time1 - total_time2:>+10.3f}    "
          f"{(total_time1 / total_time2) if total_time2 > 0 else 0:<10.3f}x")
    print()
    
    # Statistiche riassuntive
    avg_speedup = total_time1 / total_time2 if total_time2 > 0 else 0
    percent_improvement = ((total_time1 - total_time2) / total_time1 * 100) if total_time1 > 0 else 0
    
    print(f"📊 Summary:")
    print(f"   • Moves compared: {len(timings1)}")
    print(f"   • Improvements: {improvements} moves ✓")
    print(f"   • Regressions: {regressions} moves ✗")
    print(f"   • Average speedup: {avg_speedup:.3f}x")
    print(f"   • Performance gain: {percent_improvement:+.2f}%")
    
    if avg_speedup > 1.0:
        print(f"\n🚀 Overall: FASTER by {percent_improvement:.2f}%")
    elif avg_speedup < 1.0:
        print(f"\n🐌 Overall: SLOWER by {-percent_improvement:.2f}%")
    else:
        print(f"\n⚖️  Overall: NO CHANGE")


def main():
    # Default files
    default_file1 = Path("partita.log")
    default_file2 = Path("partita2.log")
    
    if len(sys.argv) == 3:
        file1 = Path(sys.argv[1])
        file2 = Path(sys.argv[2])
    elif len(sys.argv) == 1:
        file1 = default_file1
        file2 = default_file2
    else:
        print("Usage: python compare_timings.py [file1.log] [file2.log]")
        print("       python compare_timings.py  # usa partita.log e partita2.log")
        sys.exit(1)
    
    if not file1.exists():
        print(f"❌ Error: {file1} not found")
        sys.exit(1)
    
    if not file2.exists():
        print(f"❌ Error: {file2} not found")
        sys.exit(1)
    
    print(f"Comparing: {file1} vs {file2}")
    print()
    
    compare_timings(file1, file2)


if __name__ == "__main__":
    main()
