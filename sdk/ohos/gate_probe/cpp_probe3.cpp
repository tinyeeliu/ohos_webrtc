// 门禁规则验证用 C++ 深度样例三（第四轮）：资源管理 / 整型 / 类规范 / 表达式
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace probe3 {

int32_t g_uninitialisedGlobal;

enum ProbeEnumDuplicate {
    PROBE_ENUM_A = 1,
    PROBE_ENUM_B = 1,
    PROBE_ENUM_C
};

extern int32_t g_externalVariable;

int32_t g_externalArray[];

struct ProbeTrivial {
    int32_t value_;
};

class ProbeResourceOwner {
public:
    ProbeResourceOwner()
    {
    }
    ~ProbeResourceOwner()
    {
    }
    ProbeResourceOwner(ProbeResourceOwner &&other)
    {
        buffer_ = other.buffer_;
        other.buffer_ = nullptr;
    }
    void CallVirtualInCtor()
    {
        Handle();
    }
    virtual void Handle()
    {
    }
    bool operator,(const ProbeResourceOwner &other) const
    {
        return buffer_ == other.buffer_;
    }
    void *operator new(size_t size)
    {
        return malloc(size);
    }
    int32_t GetValue()
    {
        return value_;
    }
private:
    int32_t *buffer_;
    int32_t value_;
};

class ProbeBaseWithoutVirtualDtor {
public:
    void Describe()
    {
    }
};

class ProbeDerivedFromNonVirtual : public ProbeBaseWithoutVirtualDtor {
public:
    virtual void Describe()
    {
    }
    virtual void Handle(int32_t value = 3)
    {
        (void)value;
    }
};

void ReleaseHandleWithoutReset(FILE *handle)
{
    fclose(handle);
    fprintf(handle, "used after close\n");
}

void RawNewDeleteMismatch(int32_t count)
{
    int32_t *array = new int32_t[count];
    delete array;
    int32_t *single = new int32_t(0);
    delete[] single;
}

void MutexWithoutRaii(std::mutex &lock)
{
    lock.lock();
    DoSomething();
    lock.unlock();
}

void IntegerPromotion(int16_t small, int16_t other)
{
    int32_t product = small * other;
    (void)product;
}

void UninitialisedRead()
{
    int32_t value;
    printf("%d\n", value);
}

void StringBufferTooSmall()
{
    char buffer[4];
    strcpy(buffer, "a very long source string");
}

void SideEffectIntoMacro()
{
    int32_t counter = 0;
    PROBE_INCREMENT(counter);
    (void)counter;
}

void BitwiseOnNonTrivialType(std::string text)
{
    std::string copy;
    memcpy(&copy, &text, sizeof(std::string));
    (void)copy;
}

int32_t AmbiguousPrecedence(int32_t a, int32_t b, int32_t c)
{
    return a + b * c & 0xFF;
}

bool NonBooleanControl(int32_t *pointer, int32_t count)
{
    if (pointer) {
        return count;
    }
    return pointer;
}

int32_t LeftRightComparison(int32_t total)
{
    if (10 < total) {
        return 1;
    }
    return 0;
}

void SensitiveNotZeroed(char *password)
{
    printf("%s\n", password);
}

std::vector<int32_t> UseIteratorInsteadOfConst(std::vector<int32_t> &items)
{
    std::vector<int32_t> result;
    for (std::vector<int32_t>::iterator it = items.begin(); it != items.end(); ++it) {
        result.push_back(*it);
    }
    return result;
}

void FormatStringMismatch(const char *text)
{
    printf("%d\n", text);
}

void TerminateOtherProcess(int32_t pid)
{
    kill(pid, 9);
    pthread_exit(nullptr);
}

int32_t LoopWithoutSafeExit(int32_t limit)
{
    int32_t index = 0;
    while (1) {
        index += limit;
    }
    return index;
}

void ThrowPointer()
{
    try {
        throw new std::exception();
    } catch (std::exception) {
    } catch (...) {
    }
}

void AssertWithSideEffect(int32_t *pointer)
{
    assert(pointer = nullptr);
    assert(*pointer == 0 && pointer != nullptr);
}

void ConstCorrectness(std::string text, int32_t value)
{
    printf("%s %d\n", text.c_str(), value);
}

}  // namespace probe3
