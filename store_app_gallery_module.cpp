#include "store_app_gallery/store_app_gallery_platform.h"
#include "store_app_gallery/store_app_gallery_services.h"

#include "store/store_service.h"

#include "core/app/engine.h"
#include "core/app/module.h"
#include "core/app/module_context.h"

#include "core/foundation/diagnostics/log.h"

namespace nxm::store_app_gallery {
namespace {

const nx::log::Category log_store_app_gallery =
    nx::log::category("store_app_gallery");

// store.achievements/store.cloud_saves/store.presence are deliberately
// absent - HMS IAP Kit has none of those subsystems (HUAWEI Game Service is
// a separate, unrelated product for that), the same "simply doesn't provide
// it" shape `store_google_play_module.cpp` already established.
constexpr nxe::ModuleService PROVIDED_SERVICES[] = {
    {.id = store::kCoreService, .version = {1, 0, 0}},
    {.id = store::kIapService, .version = {1, 0, 0}},
};

class StoreAppGalleryModule final : public nxe::Module {
public:
  StoreAppGalleryModule() : m_core(m_platform), m_iap(m_platform) {}

  [[nodiscard]] nxe::ModuleDescriptor descriptor() const noexcept override {
    nxe::ModuleDescriptor out{};
    out.id = "store_app_gallery";
    out.version = {1, 0, 0};
    out.provided_services = PROVIDED_SERVICES;
    out.platforms = nxe::ModulePlatform::Android;
    return out;
  }

  bool on_register(nxe::ModuleContext &ctx) override {
    nxe::ServiceRegistrar registrar = ctx.service_registrar();
    store::StoreCore &core = m_core;
    store::StoreIap &iap = m_iap;
    return registrar.provide(store::kCoreService, PROVIDED_SERVICES[0].version, core) &&
           registrar.provide(store::kIapService, PROVIDED_SERVICES[1].version, iap);
  }

  bool on_attach(nxe::ModuleContext &) override {
    // No pump system registered here, unlike the desktop backends - every
    // HMS IAP call resolves through the Java shim's own callbacks,
    // dispatched by the Android runtime itself (see
    // store_app_gallery_platform.h), not from anything this module needs to
    // poll each frame. There's also no per-project runtime config file: IAP
    // Kit takes no developer-supplied credentials at runtime beyond
    // agconnect-services.json, a build-time artifact consumed entirely by
    // the AGConnect Gradle plugin before this module's C++ ever runs.
    if (m_platform.initialize())
      nx::logi(log_store_app_gallery, "attached, checking HUAWEI IAP environment");
    else
      nx::logi(log_store_app_gallery, "no Android activity available; staying idle");
    return true;
  }

  void on_detach(nxe::ModuleContext &) override { m_platform.shutdown(); }

private:
  HuaweiPlatform m_platform;
  HuaweiCore m_core;
  HuaweiIap m_iap;
};

} // namespace
} // namespace nxm::store_app_gallery

NX_DECLARE_MODULE(store_app_gallery, nxm::store_app_gallery::StoreAppGalleryModule)
