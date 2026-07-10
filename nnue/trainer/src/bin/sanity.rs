// Standalone (pure-std, no GPU) reader for bullet's quantised.bin — the
// REFERENCE IMPLEMENTATION for HydraY's engine-side loader/inference
// (NNUE_PLAN.md Fase 3). Keep the C++ code byte-for-byte consistent with this.
//
// Net: (768 -> HIDDEN)x2 -> OB, dual perspective, SCReLU, QA=255 QB=64
// SCALE=400, OB=8 material-count output buckets:
//   bucket = (popcount(occupancy) - 2) / ceil(32/OB)   (bullet MaterialCount)
// File layout (little-endian i16, padded to a multiple of 64 B):
//   l0w: 768 columns x HIDDEN rows  (feature -> accumulator weights, QA)
//   l0b: HIDDEN                     (accumulator bias, QA)
//   l1w: OB rows x 2*HIDDEN         (output weights, TRANSPOSED in the save
//                                    format: bucket b's stm half then ntm
//                                    half contiguous at b*2*HIDDEN.., QB)
//   l1b: OB                         (output bias, QA*QB)
//
// Feature index from perspective X: isOpp(piece,X)*384 + type*64 + sqFromX
// with type P=0..K=5, sq LERF (a1=0), sqFromX = sq^56 when X is Black.
//
// Usage: cargo run -r --bin sanity -- <quantised.bin> [fen]...

const HIDDEN: usize = 512;
const OUTPUT_BUCKETS: usize = 8;
const QA: i32 = 255;
const QB: i32 = 64;
const SCALE: i32 = 400;

struct Network {
    feature_weights: Vec<i16>, // [768 * HIDDEN], column f at f*HIDDEN..
    feature_bias: Vec<i16>,    // [HIDDEN]
    output_weights: Vec<i16>,  // [OB * 2 * HIDDEN], bucket b at b*2*HIDDEN..
    output_bias: Vec<i16>,     // [OB]
}

fn load(path: &str) -> Network {
    let bytes = std::fs::read(path).unwrap_or_else(|e| panic!("cannot read {path}: {e}"));
    let expected_i16s = 768 * HIDDEN + HIDDEN + OUTPUT_BUCKETS * 2 * HIDDEN + OUTPUT_BUCKETS;
    let expected_bytes = expected_i16s * 2;
    assert!(
        bytes.len() >= expected_bytes && bytes.len() % 64 == 0,
        "size mismatch: got {} bytes, expected {} (+pad to 64)",
        bytes.len(),
        expected_bytes
    );
    // bullet pads the file to 64 bytes with the repeating ASCII string
    // "bullet" — anything else in the tail means the layout drifted.
    assert!(
        bytes[expected_bytes..]
            .iter()
            .zip(b"bullet".iter().cycle())
            .all(|(a, b)| a == b),
        "unexpected padding tail — layout drift?"
    );

    let mut it = bytes
        .chunks_exact(2)
        .map(|c| i16::from_le_bytes([c[0], c[1]]));
    let mut take = |n: usize| -> Vec<i16> { it.by_ref().take(n).collect() };

    let feature_weights = take(768 * HIDDEN);
    let feature_bias = take(HIDDEN);
    let output_weights = take(OUTPUT_BUCKETS * 2 * HIDDEN);
    let output_bias = take(OUTPUT_BUCKETS);

    Network { feature_weights, feature_bias, output_weights, output_bias }
}

fn screlu(x: i16) -> i32 {
    let y = i32::from(x).clamp(0, QA);
    y * y
}

// Returns the stm-relative eval in centipawns.
fn evaluate(net: &Network, fen: &str) -> i32 {
    let mut fields = fen.split_whitespace();
    let board = fields.next().expect("empty FEN");
    let black_to_move = fields.next() == Some("b");

    let mut acc_us = net.feature_bias.clone();
    let mut acc_them = net.feature_bias.clone();
    let mut piece_count = 0usize;

    for (rank_from_top, row) in board.split('/').enumerate() {
        let rank = 7 - rank_from_top; // LERF rank
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
            piece_count += 1;

            for (acc, from_black_view) in
                [(&mut acc_us, black_to_move), (&mut acc_them, !black_to_move)]
            {
                let is_opp = is_black_piece != from_black_view;
                let sq_view = if from_black_view { sq ^ 56 } else { sq };
                let feature = usize::from(is_opp) * 384 + piece_type * 64 + sq_view;
                for (a, w) in acc
                    .iter_mut()
                    .zip(&net.feature_weights[feature * HIDDEN..(feature + 1) * HIDDEN])
                {
                    *a += *w;
                }
            }
            file += 1;
        }
    }

    // bullet MaterialCount<OB>: bucket from total piece count (incl. kings).
    let bucket = (piece_count - 2) / 32usize.div_ceil(OUTPUT_BUCKETS);
    let w = &net.output_weights[bucket * 2 * HIDDEN..(bucket + 1) * 2 * HIDDEN];

    let mut output = 0i32;
    for (a, wi) in acc_us.iter().zip(&w[..HIDDEN]) {
        output += screlu(*a) * i32::from(*wi);
    }
    for (a, wi) in acc_them.iter().zip(&w[HIDDEN..]) {
        output += screlu(*a) * i32::from(*wi);
    }
    output /= QA;
    output += i32::from(net.output_bias[bucket]);
    output * SCALE / (QA * QB)
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let path = args.get(1).map(String::as_str).unwrap_or("quantised.bin");
    let net = load(path);
    println!("{path}: layout OK");

    let default_fens = [
        ("startpos (w)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"),
        ("startpos (b)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b - - 0 1"),
        ("wN e4 extra (w)", "rnbqkbnr/pppppppp/8/8/4N3/8/PPPPPPPP/RNBQKBNR w - - 0 1"),
        ("bN e5 extra (b) [must equal previous]", "rnbqkbnr/pppppppp/8/4n3/8/8/PPPPPPPP/RNBQKBNR b - - 0 1"),
        ("stm up a queen (w)", "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"),
        ("stm down a queen (w)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w - - 0 1"),
        // Reduced-material refs: exercise different output buckets (the six
        // above all sit in the top material bucket). Keep in sync with
        // nnue/selftest.cpp.
        ("middlegame ~24 pieces (w)", "r1bq1rk1/pp3ppp/2n1pn2/3p4/3P4/2NBPN2/PP3PPP/R1BQ1RK1 w - - 0 1"),
        ("KRPvKR (w)", "8/8/4k3/8/2r5/2K5/4P3/3R4 w - - 0 1"),
        ("KQvK (w)", "8/8/8/4k3/8/8/8/KQ6 w - - 0 1"),
    ];

    let extra: Vec<(String, String)> = args[2..]
        .iter()
        .map(|f| (f.clone(), f.clone()))
        .collect();

    for (label, fen) in default_fens
        .iter()
        .map(|(l, f)| (l.to_string(), f.to_string()))
        .chain(extra)
    {
        println!("{label}: {} cp (stm)", evaluate(&net, &fen));
    }
}
