// HydraY small net trainer.
//
// Architecture: (768x4kb_hm -> 128)x2 -> pairwise -> 128 -> L1 -> 1, with 8
// output buckets on l1 and l2. trainer_deep.rs at a fraction of the width:
// the same 4 mirrored king buckets, the same pairwise CReLU product, the same
// quantisation (l0 i16 at QA=255, l1 i8 at QB=64, float tail) and SCALE, so
// the engine reads it with the big net's arithmetic (nnue/network_small.hpp)
// and its evals share the big net's centipawn scale. The reference reader for
// the file is sanity_deep.rs, which recognises the layout by its size.
//
// The net only evaluates unbalanced positions, so it is meant to be trained on
// those alone: filter the data with datafilter.rs first (the Colab notebook
// colab_small.ipynb does). It does not change anything here.
//
// FILE LAYOUT (little-endian, save_format order, padded to 64 B):
//
//   l0w [4*768][128]   i16  QA   (factoriser folded in)
//   l0b [128]          i16  QA
//   l1w [8][L1][128]   i8   QB   (transposed: each bucket contiguous)
//   l1b [8][L1]        f32
//   l2w [8][1][L1]     f32       (transposed)
//   l2b [8]            f32
//
// L1_SIZE must match nnue/network_small.hpp, and sanity_deep.rs must list the
// (128, L1_SIZE) layout.
//
// Usage, staged exactly like trainer.rs (see there for why STAGE_END must sit
// right in front of cargo, and why stage ends land on multiples of save_rate):
//   cargo run -r --bin trainer_small --features cuda -- <data.bin> [superbatches]
//                                                        [net_id] [start_sb] [resume_ckpt]

use bullet_lib::{
    game::{
        inputs::{get_num_buckets, ChessBucketsMirrored},
        outputs::MaterialCount,
    },
    nn::{
        optimiser::{AdamW, AdamWParams},
        InitSettings, Shape,
    },
    trainer::{
        save::SavedFormat,
        schedule::{lr, wdl, TrainingSchedule, TrainingSteps},
        settings::LocalSettings,
    },
    value::{loader, ValueTrainerBuilder},
};

const HIDDEN_SIZE: usize = 128;
const L1_SIZE: usize = 8;
const OUTPUT_BUCKETS: usize = 8;
const SCALE: i32 = 400;
const QA: i16 = 255;
const QB: i16 = 64;

// Keep in sync with nnue/network.hpp KING_BUCKET_MAP and sanity_deep.rs.
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
const INPUT_BUCKETS: usize = get_num_buckets(&BUCKET_LAYOUT);

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let data_path = args.get(1).cloned().expect("usage: trainer_small <data.bin> [superbatches] [net_id] [start_sb] [resume_ckpt]");
    let superbatches: usize = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(40);
    let net_id = args.get(3).cloned().unwrap_or_else(|| "hydray-small".to_string());
    let start_superbatch: usize = args.get(4).and_then(|s| s.parse().ok()).unwrap_or(1);
    let resume_from = args.get(5).cloned();
    let stage_end: usize = std::env::var("STAGE_END")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(superbatches);

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(AdamW)
        .inputs(ChessBucketsMirrored::new(BUCKET_LAYOUT))
        .output_buckets(MaterialCount::<OUTPUT_BUCKETS>)
        .save_format(&[
            SavedFormat::id("l0w")
                .transform(|store, weights| {
                    let factoriser = store.get("l0f").values.f32().repeat(INPUT_BUCKETS);
                    weights.into_iter().zip(factoriser).map(|(a, b)| a + b).collect()
                })
                .round()
                .quantise::<i16>(QA),
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            // bullet fails the save if a weight does not fit i8 at QB, rather
            // than truncating it.
            SavedFormat::id("l1w").transpose().round().quantise::<i8>(QB),
            SavedFormat::id("l1b"),
            SavedFormat::id("l2w").transpose(),
            SavedFormat::id("l2b"),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs, output_buckets| {
            let l0f = builder.new_weights("l0f", Shape::new(HIDDEN_SIZE, 768), InitSettings::Zeroed);
            let mut l0 = builder.new_affine("l0", 768 * INPUT_BUCKETS, HIDDEN_SIZE);
            l0.init_with_effective_input_size(32);
            l0.weights = l0.weights + l0f.repeat(INPUT_BUCKETS);

            let l1 = builder.new_affine("l1", HIDDEN_SIZE, OUTPUT_BUCKETS * L1_SIZE);
            let l2 = builder.new_affine("l2", L1_SIZE, OUTPUT_BUCKETS);

            // Pairwise: CReLU on the two halves of the feature transformer,
            // multiplied. HIDDEN_SIZE / 2 per perspective, so l1 reads
            // HIDDEN_SIZE values.
            let ft = |input, start, end| l0.slice(start, end).forward(input).crelu();
            let stm_hidden = ft(stm_inputs, 0, HIDDEN_SIZE / 2) * ft(stm_inputs, HIDDEN_SIZE / 2, HIDDEN_SIZE);
            let ntm_hidden = ft(ntm_inputs, 0, HIDDEN_SIZE / 2) * ft(ntm_inputs, HIDDEN_SIZE / 2, HIDDEN_SIZE);

            let hl1 = stm_hidden.concat(ntm_hidden);
            let hl2 = l1.forward(hl1).select(output_buckets).screlu();
            l2.forward(hl2).select(output_buckets)
        });

    // The saved l0 weight is l0w + l0f: clipping both to +-0.99 keeps the sum
    // inside the +-1.98 that fits i16 at QA=255.
    let stricter = AdamWParams { max_weight: 0.99, min_weight: -0.99, ..Default::default() };
    trainer.optimiser.set_params_for_weight("l0w", stricter);
    trainer.optimiser.set_params_for_weight("l0f", stricter);

    let schedule = TrainingSchedule {
        net_id,
        eval_scale: SCALE as f32,
        steps: TrainingSteps {
            batch_size: 16_384,
            batches_per_superbatch: 6104, // ~100M samples per superbatch
            start_superbatch,
            end_superbatch: stage_end,
        },
        wdl_scheduler: wdl::ConstantWDL { value: 0.3 },
        lr_scheduler: lr::StepLR {
            start: 0.001,
            gamma: 0.1,
            step: (superbatches / 2).max(1),
        },
        save_rate: superbatches.clamp(1, 10),
    };

    let settings = LocalSettings {
        threads: 4,
        test_set: None, // not implemented by bullet at the pinned rev: see trainer.rs
        output_directory: "checkpoints",
        batch_queue_size: 64,
    };

    let data_loader = loader::DirectSequentialDataLoader::new(&[data_path.as_str()]);

    // Before run(): load_from_checkpoint restores the optimiser state too.
    if let Some(ckpt) = &resume_from {
        println!("resuming from {ckpt} at superbatch {start_superbatch}");
        trainer.load_from_checkpoint(ckpt);
    }

    trainer.run(&schedule, &settings, &data_loader);

    println!("=== sanity evals (stm-relative) ===");
    for (label, fen) in [
        ("startpos (w)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"),
        ("stm up a queen (w)", "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"),
        ("stm down a queen (w)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w - - 0 1"),
        ("stm down a queen (b) [mirror of previous]", "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b - - 0 1"),
        ("KRvK (w)", "8/8/8/4k3/8/8/8/KR6 w - - 0 1"),
        ("KQvKR (w)", "8/8/4k3/8/2r5/8/8/KQ6 w - - 0 1"),
    ] {
        println!("{label}: {:.1}", trainer.eval(fen));
    }
}
