#include <jni.h>
#include <link.h>
#include <elf.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>

namespace {
constexpr uintptr_t kHeaderVA = 0xa51000;
constexpr uintptr_t kStateVA = 0xa4fa00;
constexpr size_t kStateSize = 256;
constexpr uint32_t kSealed = 0x80000000u;
constexpr char kEnvironment[] = "SSVR_WAVE_COMPOSITOR_VRS_V1";

bool isRuntime(const dl_phdr_info* info) {
    const char* path = info->dlpi_name;
    if (!path) return false;
    const char* slash = strrchr(path, '/');
    return strcmp(slash ? slash + 1 : path, "libwvr_runtime.so") == 0;
}
struct Inventory { unsigned matches = 0; std::string path; };
int collectRuntime(dl_phdr_info* info, size_t, void* data) {
    if (isRuntime(info)) {
        auto& result = *static_cast<Inventory*>(data);
        if (++result.matches == 1) result.path = info->dlpi_name;
    }
    return 0;
}
Inventory inventory() {
    Inventory result;
    dl_iterate_phdr(collectRuntime, &result);
    return result;
}
bool contains(const dl_phdr_info* info, uintptr_t va, size_t size, unsigned required, unsigned forbidden) {
    for (ElfW(Half) i = 0; i < info->dlpi_phnum; ++i) {
        const auto& p = info->dlpi_phdr[i];
        if (p.p_type != PT_LOAD || (p.p_flags & required) != required || (p.p_flags & forbidden)) continue;
        if (va >= p.p_vaddr && size <= p.p_memsz && va - p.p_vaddr <= p.p_memsz - size) return true;
    }
    return false;
}
bool validRuntime(const dl_phdr_info* info, const char* path) {
    const uintptr_t base = static_cast<uintptr_t>(info->dlpi_addr);
    if (!path || strcmp(info->dlpi_name, path) != 0 || base > UINTPTR_MAX - kHeaderVA) return false;
    if (!contains(info, kHeaderVA, 24, PF_R | PF_X, PF_W) ||
        !contains(info, kStateVA, kStateSize, PF_R | PF_W, PF_X)) return false;
    const auto* header = reinterpret_cast<const uint8_t*>(base + kHeaderVA);
    uint32_t abi, size;
    uint64_t stateVA;
    memcpy(&abi, header + 8, sizeof abi);
    memcpy(&size, header + 12, sizeof size);
    memcpy(&stateVA, header + 16, sizeof stateVA);
    return memcmp(header, "SSVRVRS1", 8) == 0 && abi == 2 && size == kStateSize && stateVA == kStateVA;
}
uint32_t* word(const dl_phdr_info* info, size_t offset) {
    return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(info->dlpi_addr) + kStateVA + offset);
}
uint32_t load32(const dl_phdr_info* info, size_t offset) { return __atomic_load_n(word(info, offset), __ATOMIC_ACQUIRE); }
uint64_t load64(const dl_phdr_info* info, size_t offset) {
    return __atomic_load_n(reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(info->dlpi_addr) + kStateVA + offset), __ATOMIC_ACQUIRE);
}
class UtfChars {
 public:
    UtfChars(JNIEnv* e, jstring s) : env(e), str(s), chars(s ? e->GetStringUTFChars(s, nullptr) : nullptr) {}
    ~UtfChars() { if (chars) env->ReleaseStringUTFChars(str, chars); }
    JNIEnv* env; jstring str; const char* chars;
};

// Android bionic holds its recursive dynamic-linker mutex throughout callbacks,
// including against dlclose. The nested inventory proves uniqueness while that
// same mutex remains held; no loader-owned pointer escapes the outer callback.
// See platform/bionic/linker/dlfcn.cpp: __loader_dl_iterate_phdr and __loader_dlclose.
struct Access {
    const char* path;
    int mode; // 0 snapshot; otherwise policy mode
    int result = 2;
    jlong values[13] = {};
};
int accessRuntime(dl_phdr_info* info, size_t, void* data) {
    auto& access = *static_cast<Access*>(data);
    if (!isRuntime(info) || !access.path || strcmp(info->dlpi_name, access.path) != 0) return 0;
    const auto all = inventory();
    if (all.matches != 1 || !validRuntime(info, access.path)) return 1;
    if (access.mode) {
        uint32_t* policy = word(info, 0);
        uint32_t old = __atomic_load_n(policy, __ATOMIC_ACQUIRE);
        do {
            if (old & kSealed) { access.result = 1; return 1; }
        } while (!__atomic_compare_exchange_n(policy, &old, static_cast<uint32_t>(access.mode), false,
                                               __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
        access.result = 0;
        return 1;
    }
    // Writers serialize on +0x48, mark +0x4c odd, publish all observations,
    // then release-publish the next even sequence. Correlation identities are
    // only compared, never exposed or dereferenced.
    const auto sequenceBefore = load32(info, 0x4c);
    const auto policyBefore = load32(info, 0);
    const auto constructorsBefore = load32(info, 0x10);
    const auto rendererBefore = load64(info, 0x30);
    const auto drawBefore = load64(info, 0x38);
    const auto unsupportedBefore = load64(info, 0x40);
    jlong values[] = {policyBefore, constructorsBefore, static_cast<jlong>(load64(info, 0x20)),
                      load32(info, 0x14), load32(info, 0x18), load32(info, 0x1c), load32(info, 0x28),
                      load32(info, 4), load32(info, 8), load32(info, 0xc), 0, 0, 0};
    const auto drawAfter = load64(info, 0x38);
    const auto unsupportedAfter = load64(info, 0x40);
    const auto rendererAfter = load64(info, 0x30);
    const auto constructorsAfter = load32(info, 0x10);
    const auto policyAfter = load32(info, 0);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    const auto sequenceAfter = load32(info, 0x4c);
    const bool stable = !(sequenceBefore & 1u) && sequenceBefore == sequenceAfter && policyBefore == policyAfter &&
                        rendererBefore != 0 && rendererBefore == rendererAfter &&
                        constructorsBefore == constructorsAfter;
    values[10] = stable;
    values[11] = stable && drawBefore == rendererBefore && drawAfter == rendererBefore;
    values[12] = stable && unsupportedBefore == rendererBefore && unsupportedAfter == rendererBefore;
    if (!stable) { values[3] = 0; values[4] = 0; values[6] = 0; }
    memcpy(access.values, values, sizeof values);
    access.result = 0;
    return 1;
}
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_sandboxvr_wave_CompositorSettings_nativeRuntimePath(JNIEnv* env, jclass) {
    auto r = inventory();
    if (r.matches == 0) return nullptr;
    return env->NewStringUTF(r.matches == 1 ? r.path.c_str() : "");
}
extern "C" JNIEXPORT jint JNICALL
Java_com_sandboxvr_wave_CompositorSettings_nativeConfigure(JNIEnv* env, jclass, jint mode, jstring validatedPath) {
    if (mode != 1 && mode != 2) return 3;
    UtfChars path(env, validatedPath);
    if (validatedPath && !path.chars) return 3;
    if (!validatedPath) {
        if (inventory().matches) return 4;
        if (setenv(kEnvironment, mode == 1 ? "Off" : "DeviceDefault", 1) != 0) return 3;
        return inventory().matches ? 4 : 0;
    }
    Access access{path.chars, mode};
    dl_iterate_phdr(accessRuntime, &access);
    if (access.result != 0) return access.result;
    return setenv(kEnvironment, mode == 1 ? "Off" : "DeviceDefault", 1) == 0 ? 0 : 3;
}
extern "C" JNIEXPORT jlongArray JNICALL
Java_com_sandboxvr_wave_CompositorSettings_nativeSnapshot(JNIEnv* env, jclass, jstring validatedPath) {
    UtfChars path(env, validatedPath);
    if (!path.chars) return nullptr;
    Access access{path.chars, 0};
    dl_iterate_phdr(accessRuntime, &access);
    if (access.result != 0) return nullptr;
    jlongArray out = env->NewLongArray(sizeof(access.values) / sizeof(access.values[0]));
    if (out) env->SetLongArrayRegion(out, 0, sizeof(access.values) / sizeof(access.values[0]), access.values);
    return out;
}
