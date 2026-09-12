#include "store_app_gallery/store_app_gallery_services.h"

#include "core/foundation/platform/android_jni.h"

#include "core/foundation/diagnostics/log.h"
#include "core/foundation/strings/format.h"

#include <utility>

namespace nxm::store_app_gallery {
namespace {

const nx::log::Category log_store_app_gallery =
    nx::log::category("store_app_gallery");

/// Looks up and calls a static void method on the Java shim by name/
/// signature, forwarding whatever jvalue args the caller already built - the
/// same helper `store_google_play_services.cpp` already established, so
/// it's centralized once here rather than repeating the FindClass/
/// GetStaticMethodID/exception-clear dance per call site.
void call_shim_static_void(JNIEnv *const env, const char *const name,
                           const char *const signature, jvalue *const args) {
  const jclass shim = find_iap_shim_class(env);
  if (shim == nullptr)
    return;
  const jmethodID method = nx::android::static_method(env, shim, name, signature);
  if (method != nullptr) {
    env->CallStaticVoidMethodA(shim, method, args);
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  env->DeleteLocalRef(shim);
}

} // namespace

// -- HuaweiCore -----------------------------------------------------------

HuaweiCore *HuaweiCore::s_instance = nullptr;

HuaweiCore::HuaweiCore(HuaweiPlatform &platform) noexcept : m_platform(platform) {
  s_instance = this;
}

HuaweiCore::~HuaweiCore() {
  if (s_instance == this)
    s_instance = nullptr;
}

bool HuaweiCore::is_owned(const nx::string_view dlc_id) const {
  if (!m_platform.ready())
    return false;
  if (dlc_id.empty())
    return true;
  for (const nx::string &id : m_owned_dlc_ids)
    if (id.view() == dlc_id)
      return true;
  return false;
}

void HuaweiCore::refresh_ownership(const nx::string_view) {
  if (!m_platform.ready())
    return;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return;
  call_shim_static_void(env.get(), "obtainOwnedPurchases", "()V", nullptr);
}

void HuaweiCore::on_owned_purchases_queried(const jboolean success,
                                            const jobjectArray product_ids) {
  if (success != JNI_TRUE)
    return;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return;
  m_owned_dlc_ids = nx::android::to_nx_string_vector(env.get(), product_ids);
}

void HuaweiCore::dispatch_owned_purchases_queried(const jboolean success,
                                                  const jobjectArray product_ids) {
  if (s_instance != nullptr)
    s_instance->on_owned_purchases_queried(success, product_ids);
}

// -- HuaweiIap --------------------------------------------------------------

HuaweiIap *HuaweiIap::s_instance = nullptr;

HuaweiIap::HuaweiIap(HuaweiPlatform &platform) noexcept : m_platform(platform) {
  s_instance = this;
}

HuaweiIap::~HuaweiIap() {
  if (s_instance == this)
    s_instance = nullptr;
}

bool HuaweiIap::purchase(const nx::string_view product_id) {
  if (!m_platform.ready())
    return false;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return false;
  const jstring id = nx::android::to_jstring(env.get(), product_id);
  if (id == nullptr)
    return false;

  m_purchase_pending = true;
  m_purchase_error = nx::string{};

  jvalue args[2];
  args[0].l = m_platform.activity();
  args[1].l = id;
  call_shim_static_void(env.get(), "purchase",
                        "(Landroid/app/Activity;Ljava/lang/String;)V", args);
  env->DeleteLocalRef(id);
  return true;
}

void HuaweiIap::refresh_products(const nx::vector<nx::string> &product_ids) {
  if (!m_platform.ready())
    return;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return;
  const jobjectArray ids = nx::android::to_jstring_array(env.get(), product_ids);
  if (ids == nullptr)
    return;
  jvalue args[1];
  args[0].l = ids;
  call_shim_static_void(env.get(), "obtainProductDetails", "([Ljava/lang/String;)V",
                        args);
  env->DeleteLocalRef(ids);
}

void HuaweiIap::on_product_details_response(
    const jboolean success, const jobjectArray product_ids,
    const jobjectArray titles, const jobjectArray formatted_prices) {
  if (success != JNI_TRUE)
    return;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return;
  const nx::vector<nx::string> ids = nx::android::to_nx_string_vector(env.get(), product_ids);
  const nx::vector<nx::string> names = nx::android::to_nx_string_vector(env.get(), titles);
  const nx::vector<nx::string> prices =
      nx::android::to_nx_string_vector(env.get(), formatted_prices);

  nx::vector<store::StoreProduct> products;
  products.reserve(ids.size());
  for (usize i = 0; i < ids.size(); ++i) {
    store::StoreProduct product;
    product.id = ids[i];
    product.title = i < names.size() ? names[i] : nx::string{};
    product.price_display = i < prices.size() ? prices[i] : nx::string{};
    products.push_back(std::move(product));
  }
  m_products = std::move(products);
}

void HuaweiIap::on_purchase_result(const jint return_code, const jstring) {
  m_purchase_pending = false;
  // OrderStatusCode.ORDER_STATE_SUCCESS == 0 (com.huawei.hms.iap.entity.
  // OrderStatusCode) - not worth pulling in a mirrored C++ enum just for
  // this one comparison, the same call store_google_play's Result-code
  // checks already make against a bare int.
  if (return_code == 0)
    m_purchase_error = nx::string{};
  else
    m_purchase_error = nx::format("HUAWEI IAP error {}", return_code);
}

void HuaweiIap::dispatch_product_details_response(
    const jboolean success, const jobjectArray product_ids,
    const jobjectArray titles, const jobjectArray formatted_prices) {
  if (s_instance != nullptr)
    s_instance->on_product_details_response(success, product_ids, titles,
                                            formatted_prices);
}

void HuaweiIap::dispatch_purchase_result(const jint return_code,
                                         const jstring product_id) {
  if (s_instance != nullptr)
    s_instance->on_purchase_result(return_code, product_id);
}

}

extern "C" JNIEXPORT void JNICALL
Java_com_nx2d_runtime_NxHuaweiIap_nativeOnOwnedPurchasesQueried(
    JNIEnv *, jclass, const jboolean success, const jobjectArray product_ids) {
  nxm::store_app_gallery::HuaweiCore::dispatch_owned_purchases_queried(success,
                                                                       product_ids);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nx2d_runtime_NxHuaweiIap_nativeOnProductDetailsResponse(
    JNIEnv *, jclass, const jboolean success, const jobjectArray product_ids,
    const jobjectArray titles, const jobjectArray formatted_prices) {
  nxm::store_app_gallery::HuaweiIap::dispatch_product_details_response(
      success, product_ids, titles, formatted_prices);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nx2d_runtime_NxHuaweiIap_nativeOnPurchaseResult(
    JNIEnv *, jclass, const jint return_code, const jstring product_id) {
  nxm::store_app_gallery::HuaweiIap::dispatch_purchase_result(return_code,
                                                               product_id);
}
