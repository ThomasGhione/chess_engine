namespace engine {

#ifndef ENGINE_POPLSB_DEFINED
#define ENGINE_POPLSB_DEFINED
// Pops and returns index of least-significant 1 bit.
inline int popLSB(uint64_t& bb) noexcept {
    const int idx = __builtin_ctzll(bb);
    bb &= (bb - 1);
    return idx;
}
#endif

} // namespace engine
