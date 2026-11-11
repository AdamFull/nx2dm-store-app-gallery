# The module's native code calls these static methods by name.
-keep class com.nx2d.runtime.NxHuaweiIap { public static <methods>; }

# HMS has not been built with R8 here, so its SDK is kept whole rather than
# trusted to rules nobody has run.
-keep class com.huawei.hms.** { *; }
-keep class com.huawei.agconnect.** { *; }
