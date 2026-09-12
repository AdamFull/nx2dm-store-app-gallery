#pragma once

#include "core/foundation/core/foundation.h"
#include "core/foundation/strings/utf8_string.h"

#include <jni.h>

namespace nxm::store_app_gallery {

/// Looks up `com.nx2d.runtime.NxHuaweiIap` - shared by the platform and both
/// services, each of which calls a different one of its static methods.
/// Returns a local ref (caller's to `DeleteLocalRef`), or nullptr with any
/// pending exception already cleared.
[[nodiscard]] jclass find_iap_shim_class(JNIEnv *env);

/// Owns the JNI handle to the Java-side static facade
/// `com.nx2d.runtime.NxHuaweiIap` (contributed by this module's own
/// `modules/store_app_gallery/android/java` tree - see
/// `android/app/build.gradle.kts`'s per-module Java source-dir loop; never
/// referenced from `NxActivity.java`).
///
/// HMS IAP Kit, like Google Play Billing, has **no native/NDK API** - this
/// class (and `store_app_gallery_services.h`) is pure JNI plumbing: call
/// into `NxHuaweiIap`'s static methods via `nx::android::JniScope`, and
/// receive results back through a handful of JNI-exported C++ functions Java
/// calls directly (`Java_com_nx2d_runtime_NxHuaweiIap_nativeOnXxx`, resolved
/// by the JVM's own symbol-name convention against the already-loaded
/// `libnx2d.so`), the same shape `store_google_play_platform.h` already
/// established as the first Java-calls-C++ direction in this codebase.
///
/// Unlike Play Billing, HMS IAP genuinely needs `Activity.onActivityResult()`
/// - both `isEnvReady()`'s HUAWEI-ID sign-in resolution and
/// `createPurchaseIntent()`'s checkout page - so `NxHuaweiIap.kt` registers
/// itself with `NxActivity.registerActivityResultHandler()`; nothing on the
/// C++ side needs to know about that plumbing.
///
/// Exactly one `HuaweiPlatform` (and one `HuaweiCore`/`HuaweiIap`) is ever
/// alive in a process, the same invariant `order_modules()` already enforces
/// for "only one store backend active" - each JNI export function below
/// dispatches through a static "current instance" pointer, mirroring
/// `store_google_play`'s own pattern.
class HuaweiPlatform {
public:
  ~HuaweiPlatform();

  bool initialize();
  void shutdown();

  /// True once `isEnvReady()` has succeeded - the signed-in HUAWEI ID (if
  /// any) is in a region HUAWEI IAP supports. False both before the first
  /// check completes and whenever it fails (no HMS Core, no sign-in, an
  /// unsupported region, ...).
  [[nodiscard]] bool ready() const noexcept { return m_ready; }

  [[nodiscard]] JavaVM *vm() const noexcept { return m_vm; }
  /// A global ref on the Android `Activity` SDL created this process with -
  /// HMS IAP needs one throughout (unlike Play Billing, which only needs an
  /// `Activity` for `purchase()`): `isEnvReady()`'s and
  /// `createPurchaseIntent()`'s resolutions both call
  /// `Status.startResolutionForResult(activity, ...)`.
  [[nodiscard]] jobject activity() const noexcept { return m_activity; }

  /// Dispatch target for this module's one `nativeOnXxx` JNI export owned by
  /// the platform itself - public because a plain `extern "C"` function, not
  /// a member, is what the JVM actually calls.
  static void dispatch_env_ready(jboolean ready);

private:
  void on_env_ready(jboolean ready);

  JavaVM *m_vm = nullptr;
  jobject m_activity = nullptr;
  bool m_ready = false;

  static HuaweiPlatform *s_instance;
};

}
