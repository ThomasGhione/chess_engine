// Reference reader (pure std, no GPU) for the quantised.bin files written by
// trainer_deep.rs (the big net) and trainer_small.rs (the small net).
//
// It is the ORACLE: the C++ must agree with this file, not the other way
// round, and is written after reading it.
//
// Both nets are (768x4kb_hm -> H)x2 -> pairwise -> H -> L1 -> 1, with 8
// output buckets applied to l1 and l2, QA=255 QB=64 SCALE=400, and differ only
// in (H, L1):
//
//   big   (trainer_deep.rs)   H = 1024, L1 = 16   file 6,425,664 B
//   small (trainer_small.rs)  H = 128,  L1 = 8    file   795,456 B
//                             (L1 = 4,             file   791,104 B, also read)
//
// The layout is recognised from the file size. The inputs are those of the
// one-layer net (4 mirrored king buckets, 8 output buckets on popcount): that
// part is copied from sanity.rs on purpose, so a disagreement between the two
// readers isolates the new part.
//
// LAYOUT (little-endian, in save_format order, padded to 64 B with "bullet"):
//
//   l0w [4*768][H]     i16    QA
//   l0b [H]            i16    QA
//   l1w [8][L1][H]     i8     QB    (transposed: each bucket contiguous)
//   l1b [8][L1]        f32    real
//   l2w [8][1][L1]     f32    real
//   l2b [8]            f32    real
//
// ARITHMETIC, which is the delicate part:
//
//   acc[i]              i16, QA scale
//   c[i]  = clamp(acc[i], 0, QA)                    QA scale, fits in u8
//   p[j]  = c[j] * c[j + H/2] / QA                  QA scale, fits in u8
//                       ^ the pairwise product: H values per perspective
//                         become H/2, and the division by QA brings them back
//                         to u8.
//   hl1   = [p_stm (H/2), p_ntm (H/2)]              H values, QA scale
//   z[o]  = (SUM hl1[i]*l1w[o][i]) / (QA*QB) + l1b[o]     real
//   a[o]  = clamp(z[o], 0, 1)^2                     SCReLU in float
//   y     = SUM a[o]*l2w[o] + l2b                   linear
//   cp    = y * SCALE
//
// Usage: cargo run -r --bin sanity_deep -- <quantised.bin> [fen]...
//        cargo run -r --bin sanity_deep -- <quantised.bin> - < fens.txt
//          (one FEN per line, prints "cp<TAB>fen": diff it against
//           ./chess small-eval or the engine's evaluations)

use std::io::BufRead;

const INPUT_BUCKETS: usize = 4;
const OUTPUT_BUCKETS: usize = 8;
const QA: i32 = 255;
const QB: i32 = 64;
const SCALE: f32 = 400.0;

// (H, L1) of the layouts this reader knows.
const LAYOUTS: [(usize, usize); 3] = [(1024, 16), (128, 4), (128, 8)];

// Keep in sync with trainer_deep.rs / trainer_small.rs BUCKET_LAYOUT and
// nnue/network.hpp.
#[rustfmt::skip]
const BUCKET_LAYOUT: [usize; 32] = [
    0, 0, 1, 1,
    2, 2, 2, 2,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
];

fn king_bucket(ksq: usize) -> usize {
    const FILE_FOLD: [usize; 8] = [0, 1, 2, 3, 3, 2, 1, 0];
    BUCKET_LAYOUT[(ksq / 8) * 4 + FILE_FOLD[ksq % 8]]
}

pub struct Network {
    pub hidden: usize,
    pub l1_size: usize,
    pub l0w: Vec<i16>,  // [4*768 * H]
    pub l0b: Vec<i16>,  // [H]
    pub l1w: Vec<i8>,   // [OB * L1 * H]
    pub l1b: Vec<f32>,  // [OB * L1]
    pub l2w: Vec<f32>,  // [OB * L1]
    pub l2b: Vec<f32>,  // [OB]
}

fn payload(hidden: usize, l1_size: usize) -> usize {
    INPUT_BUCKETS * 768 * hidden * 2
        + hidden * 2
        + OUTPUT_BUCKETS * l1_size * hidden
        + OUTPUT_BUCKETS * l1_size * 4 * 2
        + OUTPUT_BUCKETS * 4
}

pub fn load(path: &str) -> Network {
    let bytes = std::fs::read(path).unwrap_or_else(|e| panic!("cannot read {path}: {e}"));
    let (hidden, l1_size) = *LAYOUTS
        .iter()
        .find(|&&(h, l1)| payload(h, l1).div_ceil(64) * 64 == bytes.len())
        .unwrap_or_else(|| panic!("{} bytes: not a known layout {LAYOUTS:?}", bytes.len()));
    let total = payload(hidden, l1_size);
    // bullet fills the tail with "bullet" repeated: anything else means the
    // layout has drifted.
    assert!(
        bytes[total..].iter().zip(b"bullet".iter().cycle()).all(|(a, b)| a == b),
        "unexpected padding tail - layout drift?"
    );

    let mut off = 0usize;
    let i16s = |n: usize, off: &mut usize| -> Vec<i16> {
        let v = bytes[*off..*off + n * 2]
            .chunks_exact(2)
            .map(|c| i16::from_le_bytes([c[0], c[1]]))
            .collect();
        *off += n * 2;
        v
    };
    let l0w = i16s(INPUT_BUCKETS * 768 * hidden, &mut off);
    let l0b = i16s(hidden, &mut off);

    let l1w_len = OUTPUT_BUCKETS * l1_size * hidden;
    let l1w: Vec<i8> = bytes[off..off + l1w_len].iter().map(|&b| b as i8).collect();
    off += l1w_len;

    let f32s = |n: usize, off: &mut usize| -> Vec<f32> {
        let v = bytes[*off..*off + n * 4]
            .chunks_exact(4)
            .map(|c| f32::from_le_bytes([c[0], c[1], c[2], c[3]]))
            .collect();
        *off += n * 4;
        v
    };
    let l1b = f32s(OUTPUT_BUCKETS * l1_size, &mut off);
    let l2w = f32s(OUTPUT_BUCKETS * l1_size, &mut off);
    let l2b = f32s(OUTPUT_BUCKETS, &mut off);
    assert_eq!(off, total, "parsing did not consume exactly the payload");

    Network { hidden, l1_size, l0w, l0b, l1w, l1b, l2w, l2b }
}

fn screlu_f32(x: f32) -> f32 {
    let y = x.clamp(0.0, 1.0);
    y * y
}

/// Both perspectives' accumulators and the output bucket, from the FEN.
/// As in sanity.rs: the part the architectures share.
pub fn accumulate(net: &Network, fen: &str) -> (Vec<i16>, Vec<i16>, usize) {
    let hidden = net.hidden;
    let mut fields = fen.split_whitespace();
    let board = fields.next().expect("empty FEN");
    let black_to_move = fields.next() == Some("b");

    let mut pieces: Vec<(usize, bool, usize)> = Vec::with_capacity(32);
    let (mut wk, mut bk) = (usize::MAX, usize::MAX);
    for (rank_from_top, row) in board.split('/').enumerate() {
        let rank = 7 - rank_from_top;
        let mut file = 0usize;
        for c in row.chars() {
            if let Some(skip) = c.to_digit(10) {
                file += skip as usize;
                continue;
            }
            let piece_type = match c.to_ascii_lowercase() {
                'p' => 0, 'n' => 1, 'b' => 2, 'r' => 3, 'q' => 4, 'k' => 5,
                other => panic!("bad FEN piece '{other}'"),
            };
            let is_black_piece = c.is_ascii_lowercase();
            let sq = rank * 8 + file;
            if piece_type == 5 {
                if is_black_piece { bk = sq } else { wk = sq }
            }
            pieces.push((piece_type, is_black_piece, sq));
            file += 1;
        }
    }
    assert!(wk != usize::MAX && bk != usize::MAX, "FEN without both kings");

    let params = |from_black_view: bool| {
        let own = if from_black_view { bk ^ 56 } else { wk };
        let flip = if own % 8 > 3 { 7 } else { 0 };
        (768 * king_bucket(own), flip)
    };
    let (us_base, us_flip) = params(black_to_move);
    let (them_base, them_flip) = params(!black_to_move);

    let mut acc_us = net.l0b.clone();
    let mut acc_them = net.l0b.clone();
    for &(piece_type, is_black_piece, sq) in &pieces {
        for (acc, from_black_view, base, flip) in [
            (&mut acc_us, black_to_move, us_base, us_flip),
            (&mut acc_them, !black_to_move, them_base, them_flip),
        ] {
            let is_opp = is_black_piece != from_black_view;
            let sq_view = if from_black_view { sq ^ 56 } else { sq };
            let feat768 = usize::from(is_opp) * 384 + piece_type * 64 + sq_view;
            let feature = base + (feat768 ^ flip);
            for (a, w) in acc.iter_mut().zip(&net.l0w[feature * hidden..(feature + 1) * hidden]) {
                *a = a.wrapping_add(*w);
            }
        }
    }

    let out_bucket = (pieces.len() - 2) / 32usize.div_ceil(OUTPUT_BUCKETS);
    (acc_us, acc_them, out_bucket)
}

/// CReLU + pairwise product: H i16 values -> H/2 values in [0, QA].
fn pairwise(acc: &[i16], out: &mut [i32]) {
    let half = acc.len() / 2;
    for j in 0..half {
        let a = i32::from(acc[j]).clamp(0, QA);
        let b = i32::from(acc[j + half]).clamp(0, QA);
        out[j] = a * b / QA;
    }
}

pub fn evaluate(net: &Network, fen: &str) -> i32 {
    let (hidden, l1_size) = (net.hidden, net.l1_size);
    let (acc_us, acc_them, ob) = accumulate(net, fen);

    let half = hidden / 2;
    let mut hl1 = vec![0i32; hidden];
    pairwise(&acc_us, &mut hl1[..half]);
    pairwise(&acc_them, &mut hl1[half..]);

    // l1: [L1][H] per bucket, transposed at save time.
    let w1 = &net.l1w[ob * l1_size * hidden..(ob + 1) * l1_size * hidden];
    let b1 = &net.l1b[ob * l1_size..(ob + 1) * l1_size];
    let mut a1 = vec![0f32; l1_size];
    for o in 0..l1_size {
        let row = &w1[o * hidden..(o + 1) * hidden];
        let mut sum = 0i32;
        for (h, w) in hl1.iter().zip(row) {
            sum += h * i32::from(*w);
        }
        a1[o] = screlu_f32(sum as f32 / (QA * QB) as f32 + b1[o]);
    }

    let w2 = &net.l2w[ob * l1_size..(ob + 1) * l1_size];
    let mut y = net.l2b[ob];
    for (a, w) in a1.iter().zip(w2) {
        y += a * w;
    }
    (y * SCALE).round() as i32
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let path = args.get(1).map(String::as_str).unwrap_or("quantised.bin");
    let net = load(path);

    if args.get(2).map(String::as_str) == Some("-") {
        for line in std::io::stdin().lock().lines() {
            let fen = line.expect("stdin");
            if !fen.trim().is_empty() {
                println!("{}\t{fen}", evaluate(&net, &fen));
            }
        }
        return;
    }

    println!("{path}: layout OK ({} -> {} -> 1, {} B payload)",
             net.hidden, net.l1_size, payload(net.hidden, net.l1_size));

    let default_fens = [
        ("startpos (w)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"),
        ("startpos (b)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b - - 0 1"),
        ("wN e4 extra (w)", "rnbqkbnr/pppppppp/8/8/4N3/8/PPPPPPPP/RNBQKBNR w - - 0 1"),
        ("bN e5 extra (b) [must equal previous]", "rnbqkbnr/pppppppp/8/4n3/8/8/PPPPPPPP/RNBQKBNR b - - 0 1"),
        ("stm up a queen (w)", "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"),
        ("stm down a queen (w)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w - - 0 1"),
        ("middlegame ~24 pieces (w)", "r1bq1rk1/pp3ppp/2n1pn2/3p4/3P4/2NBPN2/PP3PPP/R1BQ1RK1 w - - 0 1"),
        ("KRPvKR (w)", "8/8/4k3/8/2r5/2K5/4P3/3R4 w - - 0 1"),
        ("KQvK (w)", "8/8/8/4k3/8/8/8/KQ6 w - - 0 1"),
        ("both castled short (w)", "r4rk1/ppp2ppp/2n1bn2/2bpp3/4P3/2NP1N2/PPP1BPPP/R1BQ1RK1 w - - 0 1"),
        ("white long castle vs e8 king (w)", "r3kb1r/ppp2ppp/2n1bn2/3qp3/8/2NP1N2/PPPBQPPP/2KR3R w kq - 0 1"),
        ("kings on 2nd rank (w)", "8/1k3ppp/1p6/p1p5/P1P5/1P4P1/1K3P1P/8 w - - 0 1"),
        ("active kings endgame (w)", "8/8/3k4/8/2p5/2P1K3/8/8 w - - 0 1"),
    ];

    if args.len() > 2 {
        for fen in &args[2..] {
            println!("{fen}: {} cp (stm)", evaluate(&net, fen));
        }
    } else {
        for (label, fen) in default_fens {
            println!("{label}: {} cp (stm)", evaluate(&net, fen));
        }
    }
}
