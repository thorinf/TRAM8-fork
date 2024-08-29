#include "random.h"

inline uint16_t updateLfsr(uint16_t *lfsr) {
    uint16_t bit = ((*lfsr >> 0) ^ (*lfsr >> 2) ^ (*lfsr >> 3) ^ (*lfsr >> 5)) & 1;
    *lfsr = (*lfsr >> 1) | (bit << 15);
    return *lfsr;
}

inline uint16_t updateLfsrAlt(uint16_t *lfsr) {
    uint16_t bit = ((*lfsr >> 1) ^ (*lfsr >> 3) ^ (*lfsr >> 4) ^ (*lfsr >> 8)) & 1;
    *lfsr = (*lfsr >> 1) | (bit << 15);
    return *lfsr;
}