// 门禁规则验证用 C++ 深度样例四（第四轮）：预处理 / 头文件 / 格式 / 注释
#include <cstdio>
#include <cstdint>

#define probe_keyword 3
#define MAX(a, b) a > b ? a : b
#define LONG_MACRO_WITH_MANY_LINES(a, b, c, d) ((a) + (b) + (c) + (d) + 1 + 2 + 3 + 4 + 5)
#define DEPENDS_ON_LOCAL total + localCounter
#define MULTI_LINE_MACRO(x) \
    do { \
        (x)++; \
    } while (0)

using namespace std;

namespace {

int32_t g_staticInHeaderLike = 0;

}  // namespace

int32_t ProbePreprocessUsage(int32_t total)
{
    int32_t localCounter = 1;
    return DEPENDS_ON_LOCAL + LONG_MACRO_WITH_MANY_LINES(1, 2, 3, 4);
}

int32_t ProbeMacroUsage(int32_t first, int32_t second)
{
    return MAX(first, second);
}

// 这行注释写在被注释代码的右边 int32_t misplacedComment = 0;
int32_t ProbeCommentPlacement() { return 0; }  // trailing comment on the same line as code

/**
 * @brief
 * @param
 * @return
 */
int32_t ProbeEmptyFormattedHeader(int32_t input)
{
    return input;
}

int32_t ProbeBraceStyle(int32_t input)
{
    if (input > 0)
    {
        return 1;
    }
    else
    {
        return 2;
    }
}

int32_t ProbeOperatorPlacement(int32_t first,
    int32_t second,
    int32_t third)
{
    int32_t total = first
        + second
        + third;
    return total;
}

int32_t ProbePointerTypeStyle(int32_t *value)
{
    int32_t* anotherPointer = value;
    return *anotherPointer;
}

int32_t ProbeSpacing(int32_t input)
{
    int32_t total=input+1;
    if(total>0){
        total=total*2;
    }
    return total;
}

int32_t ProbeSwitchIndent(int32_t input)
{
    switch (input) {
    case 0:
        return 1;
    case 1:
        return 2;
    default:
        return 3;
    }
}

int32_t ProbeDeclarationOrder(int32_t input)
{
    int32_t unsigned value = input;
    return value;
}

int32_t ProbeBlankLines(int32_t input)
{



    int32_t value = input;



    return value;
}

int32_t ProbeReturnTypeOnSeparateLine(
    int32_t input)
{
    return input;
}
