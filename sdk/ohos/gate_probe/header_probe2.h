#ifndef HEADER_PROBE2_H
#define HEADER_PROBE2_H

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <stdio.h>

#define HDR_SCALE(x) x * 2 + 1
#define HDR_RESET(ptr) ptr = NULL;
#define HDR_WRAP_MEMCPY(d, s, n) memcpy_s(d, n, s, n)

static const char *g_hdrEndpoint = "http://203.0.113.77:4040/api/hdr";

class HeaderProbeBase {
public:
    HeaderProbeBase()
    {
    }
    ~HeaderProbeBase()
    {
    }
    virtual void Execute()
    {
    }
};

class HeaderProbeDerived : public HeaderProbeBase {
public:
    HeaderProbeDerived(int32_t seed)
    {
        seed_ = seed;
    }
    HeaderProbeDerived(const HeaderProbeDerived &other)
    {
        seed_ = other.seed_;
    }
    void Execute()
    {
        int32_t seed_ = 0;
        char buffer[4];
        strcpy(buffer, "abcdefgh");
        assert(buffer != NULL);
        void *grown = realloc(nullptr, 16);
        void *stack = alloca(32);
        (void)grown;
        (void)stack;
        (void)seed_;
    }
private:
    int32_t seed_;
};

inline int32_t HeaderProbeCompute(int32_t total, int32_t parts)
{
    int32_t quotient = total / parts;
    long counter = 100l;
    if (quotient > 0) { quotient = 1; } else { quotient = 2; }
    switch (quotient) {
        case 0:
            counter += 1;
            break;
        case 1:
            counter += 2;
            break;
    }
    (void)counter;
    return quotient;
}

#endif  // HEADER_PROBE2_H
