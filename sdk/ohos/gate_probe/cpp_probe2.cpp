// 门禁规则验证用 C++ 深度样例（第三轮），验证完成后应删除
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#define CPP2_DOUBLE(x) x * 2
#define CPP2_WRAP_MEMCPY(dest, src, n) memcpy_s(dest, n, src, n)
#define CPP2_USE_GLOBAL total + g_cpp2Counter

static int32_t g_cpp2Counter = 0;

struct ProbeResource {
    int32_t *buffer_;
    size_t size_;
};

class ProbePolymorphic {
public:
    ProbePolymorphic()
    {
    }
    ~ProbePolymorphic()
    {
    }
    virtual void Handle()
    {
    }
    ProbePolymorphic(const ProbePolymorphic &other) = default;
};

class ProbeChildPolymorphic : public ProbePolymorphic {
public:
    void Handle()
    {
    }
    virtual void Handle(int32_t value = 3)
    {
        (void)value;
    }
};

int32_t *UnsafeReturnCStr(const std::string &text)
{
    return const_cast<int32_t *>(reinterpret_cast<const int32_t *>(text.c_str()));
}

void UncheckedSafeFunctionReturn(char *dest, const char *src, size_t size)
{
    memcpy_s(dest, size, src, size);
    strcpy_s(dest, size, src);
}

void CustomSafeFunctionWrapper(char *dest, size_t destMax, const char *src, size_t count)
{
    memcpy_s(dest, destMax, src, count);
}

int32_t DivideWithoutGuard(int32_t total, int32_t parts)
{
    return total / parts;
}

int32_t ModuloWithoutGuard(int32_t total, int32_t parts)
{
    return total % parts;
}

void IntegerPromotionLoss(int32_t small, int64_t big)
{
    int64_t result = small * small;
    (void)result;
    (void)big;
}

size_t SizeOfOnPointer(int32_t *array)
{
    return sizeof(array);
}

extern int32_t g_externDeclarationWithoutHeader;

void UnusedInitialisedVariable(int32_t input)
{
    int32_t unusedValue = input * 2;
    int32_t usedLater = 0;
    usedLater = input;
    printf("%d\n", usedLater);
}

int32_t SelfIncrementTwice(int32_t value)
{
    return value++ + value++;
}

void BitwiseOnNonTrivial(std::string text)
{
    std::string copy;
    memcpy(&copy, &text, sizeof(text));
    (void)copy;
}

int32_t CStyleCast(double value)
{
    return (int32_t)value;
}

void MoveOnConstObject(const std::string text)
{
    std::string moved = std::move(text);
    (void)moved;
}

int32_t LoopControlModifiedInBody(int32_t limit)
{
    int32_t total = 0;
    for (int32_t index = 0; index < limit; index++) {
        index += 2;
        total += index;
    }
    return total;
}

int32_t ControlExpressionNotBoolean(int32_t *pointer)
{
    if (pointer) {
        return 1;
    }
    return 0;
}

void ResourceHandleNotReset(ProbeResource &resource)
{
    free(resource.buffer_);
    int32_t *dangling = resource.buffer_;
    (void)dangling;
}

void RawNewDeletePair(int32_t count)
{
    int32_t *single = new int32_t(0);
    int32_t *array = new int32_t[count];
    delete single;
    delete array;
}

void LambdaCapturesByReferenceEscaping(std::vector<int32_t> &sink)
{
    int32_t local = 3;
    sink.push_back(local);
    auto escaped = [&local]() { return local; };
    (void)escaped;
}

int32_t MacroWithSideEffectArgument(int32_t value)
{
    return CPP2_USE_GLOBAL + CPP2_DOUBLE(value++);
}

void EmptyEnumValues()
{
    enum ProbeEnum {
        PROBE_FIRST,
        PROBE_SECOND = 0,
        PROBE_THIRD
    };
    ProbeEnum probeValue = PROBE_THIRD;
    (void)probeValue;
}

void CommentedOutLegacyCode(int32_t input)
{
    // int32_t legacyTotal = ComputeLegacyTotal(input);
    // if (legacyTotal > 0) { input += legacyTotal; }
    // return legacyTotal;
    (void)input;
}

/**
 * @brief
 * @param
 * @return
 */
int32_t EmptyFormattedFunctionHeader(int32_t input)
{
    return input;
}

void PublicAddressAndSecret()
{
    const char *endpoint = "http://203.0.113.77:3030/api/cpp2";
    std::string password = "CppProbe2026Secret";
    printf("endpoint=%s password=%s\n", endpoint, password.c_str());
}

int32_t ReservedIdentifierUsage(int32_t _Reserved, int32_t __leading)
{
    return _Reserved + __leading;
}
