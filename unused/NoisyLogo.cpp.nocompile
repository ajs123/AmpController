#include "NoisyLogo.h"

NoisyLogo::NoisyLogo() {}

const unsigned char * NoisyLogo::getNoisyLogo() { return noisyLogo; }
const unsigned char * NoisyLogo::getCleanLogo() { return logo_bits; }

int limit(int num, int low, int high) { return max( min(num, high), low); }

// Hard-coded for a 128x64 image
void NoisyLogo::makeNoise(int_fast16_t brightness, int_fast16_t centrality) {
    // The nested loops aren't needed for generating a uniform field
    // but will be if we add any spatial organization
    int bright = limit(brightness, 0, 100);
    int baseThreshold = (RAND_MAX / 100) * limit(brightness, 0, 100);
    int centFactor = (baseThreshold / 100) * limit(centrality, -100, 100) / 32;
    uint_fast16_t i = 0;
    for (int_fast16_t y = 0; y < 64; y++) {
        int threshold = centFactor >= 0 ? 
            limit(baseThreshold - centFactor * abs(32 - y), 0, RAND_MAX) :
            limit(baseThreshold + centFactor * (32 - abs(y - 32)), 0, RAND_MAX) ;
        for (int_fast16_t xb = 0; xb < 16; xb++) {
            uint8_t byte = 0;
            for (uint8_t mask = 0b00000001; mask !=0; mask <<= 1) {
                if (threshold > rand()) byte |= mask; 
            }
            noisyLogo[i] = byte; 
            i++;
        }
    }
}

// Having the proportion higher at the periphery should give a sense of 
// the image coalescing toward the center as it clarifies.

// Hard-coded for a 128x64 image
void NoisyLogo::doStep(int_fast16_t proportion, int_fast16_t centrality) {
    int prop = limit(proportion, 0, 100);
    int baseThreshold = (RAND_MAX / 100) * limit(prop, 0, 100);
    int centFactor = (baseThreshold / 100) * limit(centrality, -100, 100) / 32;
    uint_fast16_t i = 0;
    for (int_fast16_t y = 0; y < 64; y++) {
        int threshold = centFactor >= 0 ? 
            limit(baseThreshold - centFactor * abs(32 - y), 0, RAND_MAX) :
            limit(baseThreshold + centFactor * (32 - abs(y - 32)), 0, RAND_MAX) ;
        for (uint_fast16_t xb = 0; xb < 16; xb++) {
            uint8_t noisyByte = noisyLogo[i];
            uint8_t cleanByte = logo_bits[i];
            uint8_t newByte = 0;
            for (uint8_t mask = 0b00000001; mask !=0; mask <<= 1) {
                newByte |= (threshold > rand()) ? cleanByte & mask : noisyByte & mask ; 
            } 
            noisyLogo[i] = newByte;
            i++; 
        }
    }
}