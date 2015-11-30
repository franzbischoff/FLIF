#pragma once

#ifdef STATS
#include <stdio.h>
#endif

#include <stdint.h>
#include <assert.h>
#include "../config.h"

/* RAC configuration for 24-bit RAC */
class RacConfig24 {
public:
    typedef uint32_t data_t;
    static const data_t MAX_RANGE_BITS = 24;
    static const data_t MIN_RANGE_BITS = 16;
    static const data_t MIN_RANGE = (1UL << MIN_RANGE_BITS);
    static const data_t BASE_RANGE = (1UL << MAX_RANGE_BITS);

    static inline data_t chance_12bit_chance(int b12, data_t range) {
        assert(b12 > 0);
        assert((b12 >> 12) == 0);
        // We want to compute (range * b12 + 0x800) >> 12.
        // Unfortunately, this can overflow the 32-bit data type, so split range
        // in its lower and upper 12 bits, and compute separately.
        return ((((range & 0xFFF) * b12 + 0x800) >> 12) + ((range >> 12) * b12));
    }
};

template <typename Config, typename IO> class RacInput {
public:
    typedef typename Config::data_t rac_t;
protected:
    IO& io;
private:
#ifdef STATS
    uint64_t samples;
#endif
    rac_t range;
    rac_t low;
private:
    int read_catch_eof() {
        int c = io.getc();
        if(c == io.EOS) return 0;
        return c;
    }
    void inline input() {
        while (range <= Config::MIN_RANGE) {
            low <<= 8;
            range <<= 8;
            low |= read_catch_eof();
        }
    }
    bool inline get(rac_t chance) {
#ifdef STATS
        samples++;
#endif
        assert(chance > 0);
        assert(chance < range);
        if (low >= range - chance) {
            low -= range - chance;
            range = chance;
            input();
            return 1;
        } else {
            range -= chance;
            input();
            return 0;
        }
    }
public:
    RacInput(IO& ioin) : io(ioin), range(Config::BASE_RANGE), low(0) {
#ifdef STATS
        samples = 0;
#endif
        rac_t r = Config::BASE_RANGE;
        while (r > 1) {
            low <<= 8;
            low |= read_catch_eof();
            r >>= 8;
        }
    }

#ifdef STATS
    ~RacInput() {
        fprintf(stderr, "Total samples read from range coder: %llu\n", (unsigned long long)samples);
    }
#endif

    bool inline read_12bit_chance(uint16_t b12) {
        return get(Config::chance_12bit_chance(b12, range));
    }

    bool inline read_bit() {
        return get(range >> 1);
    }
};


template <typename IO> class RacInput24 : public RacInput<RacConfig24, IO> {
public:
    RacInput24(IO& io) : RacInput<RacConfig24, IO>(io) { }
};

#ifdef HAS_ENCODER
#include "rac_enc.hpp"
#endif
