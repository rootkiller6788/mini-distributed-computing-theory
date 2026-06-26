#include <stdint.h>
uint64_t lcg_random_next(uint64_t* state) {
    *state = (*state * 6364136223846793005ULL + 1442695040888963407ULL);
    return *state;
}
double lcg_random_double(uint64_t* state) {
    return (double)(lcg_random_next(state) & 0xFFFFFFFFULL) / 4294967296.0;
}
