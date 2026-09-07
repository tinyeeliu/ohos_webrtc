// 门禁规则验证用 C++ 补充样例（第二轮），验证完成后应删除
using namespace std;
#include <stdio.h>
#include <cstring>
#include <cassert>

extern "C" {
#include <stdlib.h>
}

#define PROBE_SIDE_EFFECT(x) (x++)
#define PROBE_WRAP_STRCPY(dest, src) strcpy_s(dest, strlen(src) + 1, src)

class ProbeBase {
public:
    ProbeBase()
    {
    }
    ~ProbeBase()
    {
    }
    virtual void Run()
    {
    }
};

class ProbeDerived : public ProbeBase {
public:
    ProbeDerived(int32_t seed)
    {
        seed_ = seed;
    }
    ProbeDerived(const ProbeDerived &other)
    {
        seed_ = other.seed_;
    }
    void Run()
    {
        int32_t seed_ = 0;
        (void)seed_;
    }
private:
    int32_t seed_;
};

/**
 * @brief
 * @param
 * @return
 */
int32_t ProbeCompute(int32_t total, int32_t parts) throw(int32_t)
{
    int32_t quotient = total / parts;
    char buffer[4];
    strcpy(buffer, "abcdefgh");
    assert(total > 0 && parts > 0);
    PROBE_SIDE_EFFECT(total);
    PROBE_WRAP_STRCPY(buffer, "x");
    if (quotient > 0) { quotient = 1; } else { quotient = 2; }
    printf("%d\n", buffer);
    char *tmp = tmpnam(nullptr);
    (void)tmp;
    return quotient;
}
