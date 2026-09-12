package com.nx2d.runtime

import android.app.Activity
import android.content.Intent
import android.content.IntentSender
import android.os.Handler
import android.os.Looper
import com.huawei.hms.api.ConnectionResult
import com.huawei.hms.api.HuaweiApiAvailability
import com.huawei.hms.iap.Iap
import com.huawei.hms.iap.IapApiException
import com.huawei.hms.iap.IapClient
import com.huawei.hms.iap.entity.InAppPurchaseData
import com.huawei.hms.iap.entity.OrderStatusCode
import com.huawei.hms.iap.entity.OwnedPurchasesReq
import com.huawei.hms.iap.entity.ProductInfoReq
import com.huawei.hms.iap.entity.PurchaseIntentReq
import com.huawei.hms.iap.entity.PurchaseResultInfo

// Deliberately NOT referenced anywhere in NxActivity.java (unlike NxHaptics)
// - this object only exists in the compiled app when store_app_gallery is
// enabled (see android/app/build.gradle.kts's per-module Kotlin/Java
// source-dir loop and its conditional HMS IAP + AGConnect wiring), the same
// contract NxGooglePlayBilling.kt already established. It self-registers
// with NxActivity.registerActivityResultHandler() the first time connect()
// runs instead - HMS IAP genuinely needs onActivityResult (unlike Play
// Billing), for two distinct resolutions: signing the user into a HUAWEI ID
// (isEnvReady()) and the checkout page itself (createPurchaseIntent()).
object NxHuaweiIap : NxActivity.ActivityResultHandler {
    // Arbitrary but unique across every request code this app's Activity
    // sees - SDLActivity and NxHaptics never issue activity results at all,
    // so there is nothing else in this codebase to collide with.
    private const val REQ_CODE_LOGIN = 65_401
    private const val REQ_CODE_PURCHASE = 65_402

    private val mainHandler = Handler(Looper.getMainLooper())
    private var client: IapClient? = null

    // Every product here is treated as a durable, non-consumable entitlement
    // (store.core's DLC-ownership model), the same honest scope limit
    // NxGooglePlayBilling.kt already documents for its own backend - a game
    // that needs consumable currency-style products isn't served by this
    // backend's generic purchase() call.
    private val PRICE_TYPE = IapClient.PriceType.IN_APP_NONCONSUMABLE

    @JvmStatic
    fun connect(activity: Activity) {
        if (HuaweiApiAvailability.getInstance().isHuaweiMobileServicesAvailable(activity) !=
            ConnectionResult.SUCCESS
        ) {
            nativeOnEnvReady(false)
            return
        }
        val iapClient = Iap.getIapClient(activity).also { client = it }
        NxActivity.registerActivityResultHandler(this)
        checkEnvReady(activity, iapClient)
    }

    @JvmStatic
    fun disconnect() {
        client = null
    }

    // Re-checks after a successful HWID sign-in resolution too (see
    // onActivityResult's REQ_CODE_LOGIN branch) - isEnvReady() is not a
    // one-time gate, it is the actual signal that the signed-in HUAWEI ID
    // (if any) is in a region HUAWEI IAP supports at all.
    private fun checkEnvReady(activity: Activity, iapClient: IapClient) {
        iapClient.isEnvReady()
            .addOnSuccessListener { nativeOnEnvReady(true) }
            .addOnFailureListener { e ->
                val status = (e as? IapApiException)?.status
                if (status != null && status.hasResolution()) {
                    mainHandler.post {
                        try {
                            status.startResolutionForResult(activity, REQ_CODE_LOGIN)
                        } catch (ex: IntentSender.SendIntentException) {
                            nativeOnEnvReady(false)
                        }
                    }
                } else {
                    nativeOnEnvReady(false)
                }
            }
    }

    // The re-check every game should run at startup and after a purchase
    // completes - obtainOwnedPurchases() only lists entries actually in
    // PurchaseState.PURCHASED, the IAP Kit equivalent of Play Billing's
    // unconsumed-PURCHASED-state check in NxGooglePlayBilling.queryPurchases().
    @JvmStatic
    fun obtainOwnedPurchases() {
        val iapClient = client ?: return
        val req = OwnedPurchasesReq().apply { priceType = PRICE_TYPE }
        iapClient.obtainOwnedPurchases(req)
            .addOnSuccessListener { result ->
                val ids = result.inAppPurchaseDataList.orEmpty().mapNotNull { json ->
                    val purchase = InAppPurchaseData(json)
                    purchase.productId.takeIf {
                        purchase.purchaseState == InAppPurchaseData.PurchaseState.PURCHASED
                    }
                }
                nativeOnOwnedPurchasesQueried(true, ids.toTypedArray())
            }
            .addOnFailureListener { nativeOnOwnedPurchasesQueried(false, emptyArray()) }
    }

    @JvmStatic
    fun obtainProductDetails(productIds: Array<String>) {
        val iapClient = client ?: return
        val req = ProductInfoReq().apply {
            priceType = PRICE_TYPE
            this.productIds = productIds.toList()
        }
        iapClient.obtainProductInfo(req)
            .addOnSuccessListener { result ->
                val ids = mutableListOf<String>()
                val titles = mutableListOf<String>()
                val prices = mutableListOf<String>()
                for (info in result.productInfoList.orEmpty()) {
                    ids.add(info.productId)
                    titles.add(info.productName)
                    prices.add(info.price)
                }
                nativeOnProductDetailsResponse(
                    true,
                    ids.toTypedArray(),
                    titles.toTypedArray(),
                    prices.toTypedArray(),
                )
            }
            .addOnFailureListener {
                nativeOnProductDetailsResponse(false, emptyArray(), emptyArray(), emptyArray())
            }
    }

    @JvmStatic
    fun purchase(activity: Activity, productId: String) {
        val iapClient = client
        if (iapClient == null) {
            nativeOnPurchaseResult(OrderStatusCode.ORDER_STATE_FAILED, "")
            return
        }
        val req = PurchaseIntentReq().apply {
            priceType = PRICE_TYPE
            this.productId = productId
            developerPayload = ""
        }
        iapClient.createPurchaseIntent(req)
            .addOnSuccessListener { result ->
                val status = result.status
                if (status == null || !status.hasResolution()) {
                    nativeOnPurchaseResult(OrderStatusCode.ORDER_STATE_FAILED, "")
                    return@addOnSuccessListener
                }
                mainHandler.post {
                    try {
                        status.startResolutionForResult(activity, REQ_CODE_PURCHASE)
                    } catch (ex: IntentSender.SendIntentException) {
                        nativeOnPurchaseResult(OrderStatusCode.ORDER_STATE_FAILED, "")
                    }
                }
            }
            .addOnFailureListener { e ->
                val code = (e as? IapApiException)?.statusCode ?: OrderStatusCode.ORDER_STATE_FAILED
                nativeOnPurchaseResult(code, "")
            }
    }

    // Registered once, from connect() - handles both resolutions this
    // backend ever launches (see the two REQ_CODE_* constants above). Unlike
    // Play Billing's pure-listener flow, HMS IAP has no other way to learn a
    // resolution's outcome.
    override fun onActivityResult(
        activity: NxActivity,
        requestCode: Int,
        resultCode: Int,
        data: Intent?,
    ): Boolean {
        val iapClient = client ?: return false
        return when (requestCode) {
            REQ_CODE_LOGIN -> {
                checkEnvReady(activity, iapClient)
                true
            }
            REQ_CODE_PURCHASE -> {
                handlePurchaseResult(iapClient, data)
                true
            }
            else -> false
        }
    }

    private fun handlePurchaseResult(iapClient: IapClient, data: Intent?) {
        if (data == null) {
            nativeOnPurchaseResult(OrderStatusCode.ORDER_STATE_FAILED, "")
            return
        }
        val info: PurchaseResultInfo = iapClient.parsePurchaseResultInfoFromIntent(data)
        val productId = if (info.returnCode == OrderStatusCode.ORDER_STATE_SUCCESS) {
            InAppPurchaseData(info.inAppPurchaseData).productId
        } else {
            ""
        }
        nativeOnPurchaseResult(info.returnCode, productId)
    }

    // Declared external (Kotlin's `native`), implemented in C++
    // (store_app_gallery_platform.cpp / store_app_gallery_services.cpp) and
    // resolved by the JVM's own symbol-name convention against libnx2d.so,
    // the same Java-calls-C++ direction NxGooglePlayBilling.kt established
    // first. @JvmStatic is what makes these compile to real static methods
    // rather than instance methods on Kotlin's synthesized object singleton.
    @JvmStatic
    private external fun nativeOnEnvReady(ready: Boolean)

    @JvmStatic
    private external fun nativeOnOwnedPurchasesQueried(success: Boolean, productIds: Array<String>)

    @JvmStatic
    private external fun nativeOnProductDetailsResponse(
        success: Boolean,
        productIds: Array<String>,
        titles: Array<String>,
        formattedPrices: Array<String>,
    )

    @JvmStatic
    private external fun nativeOnPurchaseResult(returnCode: Int, productId: String)
}
