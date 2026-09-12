#include "store_app_gallery/store_app_gallery_platform.h"

#include "core/foundation/platform/android_jni.h"

#include "core/foundation/diagnostics/log.h"

#include <SDL3/SDL_system.h>

namespace nxm::store_app_gallery {
namespace {

const nx::log::Category log_store_app_gallery =
    nx::log::category("store_app_gallery");

constexpr const char *SHIM_CLASS = "com/nx2d/runtime/NxHuaweiIap";

} // namespace

jclass find_iap_shim_class(JNIEnv *const env) {
  return nx::android::find_class(env, SHIM_CLASS);
}

HuaweiPlatform *HuaweiPlatform::s_instance = nullptr;

HuaweiPlatform::~HuaweiPlatform() { shutdown(); }

bool HuaweiPlatform::initialize() {
  JNIEnv *const env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
  jobject const activity =
      env != nullptr ? static_cast<jobject>(SDL_GetAndroidActivity()) : nullptr;
  if (env == nullptr || activity == nullptr) {
    nx::logw(log_store_app_gallery, "no Android activity available");
    return false;
  }
  if (env->GetJavaVM(&m_vm) != JNI_OK || m_vm == nullptr) {
    nx::logw(log_store_app_gallery, "JNI_GetJavaVM failed");
    return false;
  }
  m_activity = env->NewGlobalRef(activity);
  env->DeleteLocalRef(activity);
  if (m_activity == nullptr) {
    nx::logw(log_store_app_gallery, "failed to hold a global ref on the activity");
    return false;
  }

  const jclass shim = find_iap_shim_class(env);
  if (shim == nullptr) {
    nx::logw(log_store_app_gallery,
              "NxHuaweiIap.class not found - was the module enabled when "
              "the APK was built?");
    return false;
  }
  const jmethodID connect = nx::android::static_method(
      env, shim, "connect", "(Landroid/app/Activity;)V");
  if (connect == nullptr) {
    env->DeleteLocalRef(shim);
    return false;
  }

  s_instance = this;
  env->CallStaticVoidMethod(shim, connect, m_activity);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  env->DeleteLocalRef(shim);
  return true;
}

void HuaweiPlatform::shutdown() {
  if (m_vm == nullptr)
    return;
  const nx::android::JniScope env(m_vm);
  if (env && m_activity != nullptr) {
    const jclass shim = find_iap_shim_class(env.get());
    if (shim != nullptr) {
      const jmethodID disconnect =
          nx::android::static_method(env.get(), shim, "disconnect", "()V");
      if (disconnect != nullptr)
        env->CallStaticVoidMethod(shim, disconnect);
      if (env->ExceptionCheck())
        env->ExceptionClear();
      env->DeleteLocalRef(shim);
    }
    env->DeleteGlobalRef(m_activity);
  }
  m_activity = nullptr;
  m_vm = nullptr;
  m_ready = false;
  if (s_instance == this)
    s_instance = nullptr;
}

void HuaweiPlatform::on_env_ready(const jboolean ready) {
  m_ready = ready == JNI_TRUE;
  if (m_ready)
    nx::logi(log_store_app_gallery, "HUAWEI IAP environment ready");
  else
    nx::logi(log_store_app_gallery,
              "HUAWEI IAP environment not ready - staying idle");
}

void HuaweiPlatform::dispatch_env_ready(const jboolean ready) {
  if (s_instance != nullptr)
    s_instance->on_env_ready(ready);
}

}

extern "C" JNIEXPORT void JNICALL
Java_com_nx2d_runtime_NxHuaweiIap_nativeOnEnvReady(JNIEnv *, jclass,
                                                    const jboolean ready) {
  nxm::store_app_gallery::HuaweiPlatform::dispatch_env_ready(ready);
}
