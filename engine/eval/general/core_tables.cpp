#include "../evaluator.hpp"

namespace engine {

const std::array<uint64_t, 8> Evaluator::FILE_MASKS = Evaluator::initFileMasks();
const std::array<uint64_t, 8> Evaluator::ADJACENT_FILES_ONLY = Evaluator::initAdjacentFilesOnly();
const std::array<uint64_t, 8> Evaluator::ADJACENT_AND_FILE_MASKS = Evaluator::initAdjacentAndFileMasks();
const std::array<uint64_t, 64> Evaluator::KING_PROXIMITY_MASKS = Evaluator::initKingProximityMasks();

const std::array<uint64_t, 64> Evaluator::WHITE_FORWARD_FILL = Evaluator::initWhiteForwardFill();
const std::array<uint64_t, 64> Evaluator::BLACK_FORWARD_FILL = Evaluator::initBlackForwardFill();

} // namespace engine
