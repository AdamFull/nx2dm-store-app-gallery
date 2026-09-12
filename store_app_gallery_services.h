#pragma once

#include "store_app_gallery/store_app_gallery_platform.h"

#include "store/store_service.h"

#include <jni.h>

namespace nxm::store_app_gallery {

/// Two of the five neutral services (store_service.h), backed by HMS IAP
/// Kit - store.achievements/store.cloud_saves/store.presence are never
/// registered: IAP Kit has none of those subsystems (HUAWEI Game Service is
/// a separate, unrelated product for that), the same "simply doesn't
/// provide it" shape `store_google_play_services.h` already established.
///
/// Every call here is asynchronous via the Java shim's callbacks (see
/// store_app_gallery_platform.h for the JNI mechanics) - both classes
/// therefore hold a small cache populated by their own query's JNI-exported
/// callback, with the neutral interface's synchronous methods reading
/// whatever is cached so far, the same eventually-consistent shape
/// `store_google_play_services.h` already established.

class HuaweiCore final : public store::StoreCore {
public:
  explicit HuaweiCore(HuaweiPlatform &platform) noexcept;
  ~HuaweiCore() override;

  /// HMS IAP Kit has no "own the base game" concept at all - AppGallery
  /// already gates who can install/run the APK, so a ready environment
  /// implies base ownership. A non-empty @p dlc_id checks the cache
  /// refresh_ownership() populates instead.
  [[nodiscard]] bool is_owned(nx::string_view dlc_id = {}) const override;
  [[nodiscard]] nx::vector<nx::string> owned_dlc_ids() const override {
    return m_owned_dlc_ids;
  }
  [[nodiscard]] nx::string_view store_name() const noexcept override {
    return "app_gallery";
  }

  /// Fires `NxHuaweiIap.obtainOwnedPurchases()`, refreshing owned_dlc_ids()
  /// with every currently-PURCHASED non-consumable product id.
  void refresh_ownership();

  static void dispatch_owned_purchases_queried(jboolean success,
                                               jobjectArray product_ids);

private:
  void on_owned_purchases_queried(jboolean success, jobjectArray product_ids);

  HuaweiPlatform &m_platform;
  nx::vector<nx::string> m_owned_dlc_ids;

  static HuaweiCore *s_instance;
};

class HuaweiIap final : public store::StoreIap {
public:
  explicit HuaweiIap(HuaweiPlatform &platform) noexcept;
  ~HuaweiIap() override;

  [[nodiscard]] nx::vector<store::StoreProduct> products() const override {
    return m_products;
  }
  bool purchase(nx::string_view product_id) override;
  [[nodiscard]] bool purchase_pending() const override { return m_purchase_pending; }
  [[nodiscard]] nx::string_view purchase_error() const override {
    return m_purchase_error.view();
  }

  /// Fires `NxHuaweiIap.obtainProductDetails()` for exactly the ids given -
  /// like Play Billing, IAP Kit has no "list everything" query, the game
  /// must know its own product ids up front.
  void refresh_products(const nx::vector<nx::string> &product_ids);

  static void dispatch_product_details_response(jboolean success,
                                                 jobjectArray product_ids,
                                                 jobjectArray titles,
                                                 jobjectArray formatted_prices);
  static void dispatch_purchase_result(jint return_code, jstring product_id);

private:
  void on_product_details_response(jboolean success, jobjectArray product_ids,
                                   jobjectArray titles,
                                   jobjectArray formatted_prices);
  void on_purchase_result(jint return_code, jstring product_id);

  HuaweiPlatform &m_platform;
  nx::vector<store::StoreProduct> m_products;
  bool m_purchase_pending = false;
  nx::string m_purchase_error;

  static HuaweiIap *s_instance;
};

}
