// xash3dpp — Android AAsset platform extensions
// Legacy reference: filesystem/android.c
// This file is compiled only when XASH_ANDROID is defined.

#if defined(XASH_ANDROID)

#include <xash3dpp/private/filesystem/platform/os_io.hpp>
#include <xash3dpp/private/filesystem/os_fd.hpp>

// Android NDK headers — confined to this translation unit.
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include <sys/syscall.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <string>

namespace xash::filesystem::platform {

// ---------------------------------------------------------------------------
// AssetManagerHandle — full definition (forward-declared in os_io.hpp)
// ---------------------------------------------------------------------------

struct AssetManagerHandle {
    AAssetManager* mgr          = nullptr;
    bool           engine       = false;
    std::string    package_name;
};

// ---------------------------------------------------------------------------
// Module-level JNI state
// ---------------------------------------------------------------------------

namespace {

struct JniState {
    JNIEnv*   env               = nullptr;
    jobject   activity          = nullptr;
    jclass    activity_class    = nullptr;
    jmethodID get_package_name    = nullptr;   // "()Ljava/lang/String;"
    jmethodID get_calling_package = nullptr;   // "()Ljava/lang/String;"
    jmethodID get_assets_list     = nullptr;   // "(ZLjava/lang/String;)[Ljava/lang/String;"
    jmethodID get_assets          = nullptr;   // "(Z)Landroid/content/res/AssetManager;"
};

JniState g_jni;

// Cached handles: [0] = engine APK, [1] = app APK.
AssetManagerHandle g_handles[2];

// Per-slot init flags — ensure each handle is populated exactly once.
std::once_flag g_init_flags[2];

} // anonymous namespace

// ---------------------------------------------------------------------------
// android_init_jni
// ---------------------------------------------------------------------------

void android_init_jni(JNIEnv* env, jobject activity, jclass activity_class) noexcept {
    assert(g_jni.env == nullptr && "android_init_jni called more than once");
    g_jni.env            = env;
    g_jni.activity       = activity;
    g_jni.activity_class = activity_class;

    g_jni.get_package_name    = env->GetMethodID(activity_class, "getPackageName",
                                                 "()Ljava/lang/String;");
    g_jni.get_calling_package = env->GetMethodID(activity_class, "getCallingPackage",
                                                 "()Ljava/lang/String;");
    g_jni.get_assets_list     = env->GetMethodID(activity_class, "getAssetsList",
                                                 "(ZLjava/lang/String;)[Ljava/lang/String;");
    g_jni.get_assets          = env->GetMethodID(activity_class, "getAssets",
                                                 "(Z)Landroid/content/res/AssetManager;");
}

// ---------------------------------------------------------------------------
// get_asset_manager
// ---------------------------------------------------------------------------

AssetManagerHandle* get_asset_manager(bool engine_package) noexcept {
    if (!g_jni.env || !g_jni.get_assets) return nullptr;

    const int idx = engine_package ? 0 : 1;
    std::call_once(g_init_flags[idx], [idx, engine_package]() noexcept {
        if (!g_jni.env || !g_jni.get_assets) return;
        AssetManagerHandle& h = g_handles[idx];

        const jboolean engine_jbool = engine_package ? JNI_TRUE : JNI_FALSE;
        jobject mgr_obj = g_jni.env->CallObjectMethod(
            g_jni.activity, g_jni.get_assets, engine_jbool);
        if (!mgr_obj) return;

        h.mgr    = AAssetManager_fromJava(g_jni.env, mgr_obj);
        h.engine = engine_package;

        // Retrieve package name for diagnostics.
        const jmethodID name_mid = engine_package ? g_jni.get_package_name
                                                  : g_jni.get_calling_package;
        if (name_mid) {
            jstring pkg = static_cast<jstring>(
                g_jni.env->CallObjectMethod(g_jni.activity, name_mid));
            if (pkg) {
                const char* c = g_jni.env->GetStringUTFChars(pkg, nullptr);
                if (c) {
                    h.package_name = c;
                    g_jni.env->ReleaseStringUTFChars(pkg, c);
                }
                g_jni.env->DeleteLocalRef(pkg);
            }
        }

        g_jni.env->DeleteLocalRef(mgr_obj);
    });
    return g_handles[idx].mgr ? &g_handles[idx] : nullptr;
}

// ---------------------------------------------------------------------------
// list_assets
// ---------------------------------------------------------------------------

std::vector<std::string> list_assets(AssetManagerHandle* mgr,
                                     std::string_view path) noexcept {
    if (!mgr || !g_jni.env || !g_jni.get_assets_list) return {};

    const std::string path_str(path);
    jstring path_jstr = g_jni.env->NewStringUTF(path_str.c_str());
    if (!path_jstr) return {};

    const jboolean engine_jbool = mgr->engine ? JNI_TRUE : JNI_FALSE;
    jobjectArray arr = static_cast<jobjectArray>(
        g_jni.env->CallObjectMethod(g_jni.activity, g_jni.get_assets_list,
                                    engine_jbool, path_jstr));
    g_jni.env->DeleteLocalRef(path_jstr);
    if (!arr) return {};

    const jsize count = g_jni.env->GetArrayLength(arr);
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));

    for (jsize i = 0; i < count; ++i) {
        jstring entry = static_cast<jstring>(
            g_jni.env->GetObjectArrayElement(arr, i));
        if (!entry) continue;
        const char* c = g_jni.env->GetStringUTFChars(entry, nullptr);
        if (c) {
            result.emplace_back(c);
            g_jni.env->ReleaseStringUTFChars(entry, c);
        }
        g_jni.env->DeleteLocalRef(entry);
    }

    g_jni.env->DeleteLocalRef(arr);
    return result;
}

// ---------------------------------------------------------------------------
// asset_exists
// ---------------------------------------------------------------------------

bool asset_exists(AssetManagerHandle* mgr, std::string_view path) noexcept {
    if (!mgr || !mgr->mgr) return false;
    const std::string path_str(path);
    AAsset* a = AAssetManager_open(mgr->mgr, path_str.c_str(), AASSET_MODE_UNKNOWN);
    if (!a) return false;
    AAsset_close(a);
    return true;
}

// ---------------------------------------------------------------------------
// open_asset
// ---------------------------------------------------------------------------

OsFd open_asset(AssetManagerHandle* mgr, std::string_view path) noexcept {
    if (!mgr || !mgr->mgr) return OsFd{};

    const std::string path_str(path);
    AAsset* asset = AAssetManager_open(mgr->mgr, path_str.c_str(), AASSET_MODE_BUFFER);
    if (!asset) return OsFd{};

    const auto size = static_cast<int64_t>(AAsset_getLength64(asset));

    // Create an anonymous in-memory fd so the caller gets a seekable stream.
    const int fd_raw = static_cast<int>(
        ::syscall(SYS_memfd_create, path_str.c_str(), 0x1u /* MFD_CLOEXEC */));
    if (fd_raw < 0) {
        AAsset_close(asset);
        return OsFd{};
    }

    bool ok = false;
    const void* buf = AAsset_getBuffer(asset);
    if (buf) {
        // Fast path: asset is already in the NDK's internal buffer.
        ok = (::write(fd_raw, buf, static_cast<std::size_t>(size))
              == static_cast<ssize_t>(size));
    } else {
        // Slow path: read in chunks.
        uint8_t tmp[65536];
        int64_t remaining = size;
        ok = true;
        while (remaining > 0) {
            const int chunk = static_cast<int>(
                std::min<int64_t>(remaining, static_cast<int64_t>(sizeof(tmp))));
            const int n = AAsset_read(asset, tmp, static_cast<std::size_t>(chunk));
            if (n <= 0) { ok = false; break; }
            if (::write(fd_raw, tmp, static_cast<std::size_t>(n))
                    != static_cast<ssize_t>(n)) {
                ok = false;
                break;
            }
            remaining -= n;
        }
    }

    AAsset_close(asset);

    if (!ok) {
        ::close(fd_raw);
        return OsFd{};
    }

    // Rewind to position 0 so the caller can read from the beginning.
    ::lseek(fd_raw, 0, SEEK_SET);
    return OsFd{fd_raw};
}

} // namespace xash::filesystem::platform

#endif // XASH_ANDROID
