// Provides a logo that emerges from random noise
#pragma once

#include <U8g2lib.h>
#include "logo.h"

class NoisyLogo {
    public:
        NoisyLogo();

        /**
         * @brief Get the noisy logo. 
         * @return const char* 
         */
        const unsigned char * getNoisyLogo();

        /**
         * @brief Get the clean logo
         * @return const char* 
         */
        const unsigned char * getCleanLogo();

        /**
         * @brief Rebuild the noisy logo.
         * 
         * @param brightness percentage between 0 and 100, where 50 = half of the pixels are lit
         * @param centrality percentage between -100 (bias to periphery) to 100 (bias to center)
         */
        void makeNoise(int_fast16_t brigthness, int_fast16_t centrality);

        /**
         * @brief Migrate the noisy logo toward the base image.
         * 
         * @param proportion percentage between 0 (no movement toward the base) and 100 (copy the base)
         * @param centrality percentage between -100 (operate only at the periphery) to 100 (only at the center)
         */
        void doStep(int_fast16_t proportion, int_fast16_t centrality);

    private:
        unsigned char noisyLogo[logo_bytes] {0};
};

