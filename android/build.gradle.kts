// Applied into :app's own build script via apply(from = ...) - see
// android/app/build.gradle.kts's generic per-module loop, which does this
// for any enabled module that ships this exact file. This is NOT a
// separate Gradle subproject - it executes in :app's own Project context,
// so dependencies{}/apply(plugin = ...) here behave exactly as if written
// directly in :app's build.gradle.kts, without the engine ever having to
// know this module - or HMS IAP/AGConnect - exist. Configurations are
// added by string name ("implementation", not the typed implementation(...)
// function) - Gradle's type-safe accessors for a project's own
// configurations aren't generated for a script applied this way via
// apply(from = ...). The one piece that can't live here is the AGConnect
// plugin's buildscript-classpath registration itself (Gradle requires
// buildscript{} to be the very first block in a script, before nxModules
// is even known, and a buildscript{} block in :app's own script would
// break its android{}/androidComponents{} DSL accessors) - see this
// module's own android/settings.properties, scanned generically by the
// root project's own build.gradle.kts buildscript{} block instead.
//
// agconnect-services.json is a per-project credential (like
// AndroidManifest.xml/res - not like this file, which is shared across
// every project) - read from the current project's own android/ directory.
val nxProjectDir = project.extra["nxProjectDir"] as java.io.File
val nxAgConnectServices = nxProjectDir.resolve("android/agconnect-services.json")
if (!nxAgConnectServices.isFile) {
    throw GradleException(
        "store_app_gallery requires " +
            "${nxAgConnectServices.invariantSeparatorsPath} - download it " +
            "from AppGallery Connect for this app and place it there",
    )
}
// The AGConnect plugin reads agconnect-services.json synchronously at
// configuration time (not lazily via a task), so the copy must happen
// before apply(plugin = ...) below.
nxAgConnectServices.copyTo(project.file("agconnect-services.json"), overwrite = true)
apply(plugin = "com.huawei.agconnect")

dependencies {
    "implementation"("com.huawei.hms:iap:6.16.6.305")
}
