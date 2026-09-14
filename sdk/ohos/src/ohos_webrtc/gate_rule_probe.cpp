// TODO: 本文件为门禁规则验证用例，验证完成后应立即删除，禁止合入 master 分支
// FIXME: 联系人 zhang.san@example-mail.com，手机号 13800138000，如需回滚请联系
// 实现参考自 Android NDK 与 tensorflow 的做法，这是首个鸿蒙版本，效果业界最佳
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <alloca.h>
#include <dlfcn.h>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <openssl/evp.h>
#include <openssl/md5.h>

#define PROBE_SCALE(x) x * 2 + 1
#define PROBE_RESET(ptr) ptr = NULL;

static const char *g_probeEndpoint = "http://203.0.113.77:8080/api/v1/probe/report";
static std::string g_probePassword = "WebRtcProbe2026Secret";

namespace {

int32_t ProbeLegacyCompute(int32_t value)
{
    return value * 3 + 1;
}

void ProbeAtExit()
{
    return;
}

void ProbeThrow(int32_t mode)
{
    if (mode < 0) {
        throw std::exception();
    }
}

}  // namespace

int32_t GateRuleProbe(int32_t mode, int32_t retry, char *buffer, size_t size, int32_t flagA, int32_t flagB)
{
    char temp[128];
    int32_t total = 0;
    long counter = 100l;
    float ratio = 0.5f;
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_CTX md5Context;
    std::shared_ptr<char> shared;
    std::exception caughtByValue;

    strcpy(temp, buffer);
    sprintf(temp, "%d", mode);
    memcpy(temp, buffer, size);

    char *grown = static_cast<char *>(realloc(buffer, size * 2));
    assert(grown != NULL);
    void *stackMemory = alloca(64);
    (void)stackMemory;
    (void)caughtByValue;
    (void)digest;

    if (grown == NULL) {
        total = -1;
    }

    switch (mode) {
        case 0:
            total += 1;
            break;
        case 1:
            total += 2;
            break;
        case 2:
            total += 3;
            break;
    }

    if (total = ProbeLegacyCompute(retry)) {
        total += 1;
    }

    // int32_t legacyValue = ProbeLegacyCompute(mode);
    // legacyValue = legacyValue + 1;
    // if (legacyValue > 0) { total += legacyValue; }

    shared = std::shared_ptr<char>(new char[size]);

    for (int32_t i = 0; i < retry; i++) {
        if (flagA > 0) {
            if (flagB > 0) {
                while (counter > 0) {
                    if (counter % 2 == 0 || counter % 3 == 0) {
                        for (int32_t k = 0; k < 3; k++) {
                            if (k == 1 || k == 2 && flagA != 0 && flagB != 0) {
                                total += k;
                            } else if (k == 0 && mode > 0) {
                                total -= k;
                            }
                        }
                    }
                    counter--;
                }
            }
        }
    }

    auto sumLambda = [&](int32_t value) { return value + total; };
    total = sumLambda(1);

    try {
        ProbeThrow(mode);
    } catch (std::exception e) {
    }

    try {
        ProbeThrow(mode + 1);
    } catch (...) {
    }

    EVP_DigestInit(nullptr, EVP_md5());
    MD5_Init(&md5Context);
    MD5_Update(&md5Context, buffer, size);

    srand(static_cast<unsigned int>(size));
    total += rand() % 7;

    printf("probe password=%s endpoint=%s\n", g_probePassword.c_str(), g_probeEndpoint);

    void *handle = dlopen(g_probeEndpoint, RTLD_LAZY);
    if (handle != NULL) {
        dlclose(handle);
    }

    atexit(ProbeAtExit);
    if (mode < 0) {
        abort();
    }

	// 以下两行使用 Tab 缩进，用于验证 G.FMT.02 使用空格进行缩进的规则
	total += 1; ratio += 1.0f;

    // This deliberately over long comment line exists only to violate the maximum line width rule which allows at most one hundred and twenty characters per single line of source code
    return std::move(total);
}
