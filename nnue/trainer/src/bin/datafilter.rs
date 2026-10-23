// Keeps the bulletformat records (32 B each) of unbalanced positions: the
// training set of the small net, which only evaluates such positions.
//
// A record passes when every given threshold holds:
//   --min-material N   |material(stm) - material(opp)| >= N, with the engine's
//                      PIECE_VALUES (engine/search/search_constants.hpp):
//                      P 100, N 344, B 359, R 502, Q 960
//   --min-score N      |score| >= N, the record's stm-relative label in cp
// With no threshold every valid record passes. All-zero records (a write cut
// short) are dropped either way.
//
// Streams stdin to stdout, so it sits behind a decompressor:
//   zstd -dc part.bin.zst | datafilter --min-material 500 > small.bin
// The kept records stay in input order: a shuffled input stays shuffled.
// The statistics go to stderr.
//
// Pure std, no GPU:
//   cargo run -r --bin datafilter -- --min-material 500 < in.bin > out.bin

use std::io::{BufReader, BufWriter, Read, Write};

const REC: usize = 32;
const PIECE_VALUES: [i32; 6] = [100, 344, 359, 502, 960, 0]; // P N B R Q K

fn imbalance(rec: &[u8]) -> i32 {
    let occ = u64::from_le_bytes(rec[..8].try_into().unwrap());
    let mut diff = 0i32;
    for idx in 0..occ.count_ones() as usize {
        let nibble = (rec[8 + idx / 2] >> (4 * (idx & 1))) & 0xF;
        let value = PIECE_VALUES[usize::from(nibble & 7)];
        diff += if nibble & 8 == 0 { value } else { -value };
    }
    diff.abs()
}

fn main() {
    let mut min_material: Option<i32> = None;
    let mut min_score: Option<i32> = None;
    let mut args = std::env::args().skip(1);
    while let Some(flag) = args.next() {
        let mut value = || -> i32 {
            args.next().and_then(|v| v.parse().ok()).unwrap_or_else(|| panic!("{flag} needs a number"))
        };
        match flag.as_str() {
            "--min-material" => min_material = Some(value()),
            "--min-score" => min_score = Some(value()),
            _ => panic!("usage: datafilter [--min-material N] [--min-score N] < in.bin > out.bin"),
        }
    }

    let mut input = BufReader::with_capacity(1 << 24, std::io::stdin().lock());
    let mut output = BufWriter::with_capacity(1 << 24, std::io::stdout().lock());
    // |imbalance| histogram of the valid records, in steps of 100.
    let mut hist = [0u64; 21];
    let (mut total, mut zeroed, mut kept) = (0u64, 0u64, 0u64);
    let mut buf = vec![0u8; REC * 65536];
    let mut filled = 0usize;
    loop {
        let n = input.read(&mut buf[filled..]).expect("read stdin");
        filled += n;
        let whole = filled / REC * REC;
        for rec in buf[..whole].chunks_exact(REC) {
            total += 1;
            if rec[..8] == [0u8; 8] {
                zeroed += 1;
                continue;
            }
            let material = imbalance(rec);
            hist[(material / 100).min(20) as usize] += 1;
            let score = i32::from(i16::from_le_bytes([rec[24], rec[25]])).abs();
            if min_material.is_some_and(|m| material < m) || min_score.is_some_and(|s| score < s) {
                continue;
            }
            output.write_all(rec).expect("write stdout");
            kept += 1;
        }
        buf.copy_within(whole..filled, 0);
        filled -= whole;
        if n == 0 {
            break;
        }
    }
    output.flush().expect("flush stdout");
    assert!(filled == 0, "input ends with a partial record ({filled} B): truncated file?");

    let valid = total - zeroed;
    eprintln!("records {total}, all-zero {zeroed}, kept {kept} ({:.2}% of valid)",
              100.0 * kept as f64 / valid.max(1) as f64);
    eprint!("|material imbalance| of the valid records, cumulative from the top:");
    let mut above = 0u64;
    for (i, c) in hist.iter().enumerate().rev() {
        above += c;
        if i % 2 == 1 || i == 20 {
            continue;
        }
        eprint!(" >={}: {:.1}%", i * 100, 100.0 * above as f64 / valid.max(1) as f64);
    }
    eprintln!();
}
