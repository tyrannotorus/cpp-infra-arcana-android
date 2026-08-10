import java.security.MessageDigest
import java.util.Properties

plugins {
    id("com.android.application")
}

// Release signing credentials live OUTSIDE the repository: a gitignored
// android/keystore.properties (storeFile, storePassword, keyAlias,
// keyPassword). CI reconstructs it from repository secrets. Without it,
// assembleRelease produces an unsigned APK.
val keystoreProperties = Properties().apply {
    val f = rootProject.file("keystore.properties")
    if (f.exists()) f.inputStream().use { load(it) }
}

// Generates asset_manifest.txt ("sha256  relative/path" per line) into a generated
// assets dir. The native bootstrap (jni/android_bootstrap.cpp) reads it to extract
// the APK's game data to internal storage, re-extracting whenever any file changes.
abstract class GenerateAssetManifestTask : DefaultTask() {
    @get:InputDirectory
    abstract val gameDataDir: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun generate() {
        val root = gameDataDir.get().asFile
        val md = MessageDigest.getInstance("SHA-256")
        val lines = root.walkTopDown()
            .filter { it.isFile }
            .map { f ->
                md.reset()
                val hash = md.digest(f.readBytes()).joinToString("") { "%02x".format(it) }
                "$hash  ${f.relativeTo(root).invariantSeparatorsPath}"
            }
            .sorted()
            .toList()
        val out = outputDir.get().asFile
        out.mkdirs()
        out.resolve("asset_manifest.txt").writeText(lines.joinToString("\n") + "\n")
    }
}

android {
    namespace = "camp.werewolf.infraarcana"
    compileSdk = 35

    // Pinned explicitly. Left unset, AGP 8.7 falls back to its own default
    // (34.0.0) and silently downloads it at build time; pinning keeps CI and
    // local builds on the same, pre-installed build tools.
    buildToolsVersion = "35.0.0"

    ndkVersion = "28.0.13004108"

    defaultConfig {
        applicationId = "camp.werewolf.infraarcana"
        minSdk = 24
        targetSdk = 35
        versionCode = 23000013
        versionName = "23.0.0.13"

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/jni/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    dependenciesInfo {
        includeInApk = false
        includeInBundle = false
    }

    signingConfigs {
        // debug/staging use the machine-local ~/.android/debug.keystore.
        // The release config exists only when keystore.properties is present.
        if (keystoreProperties.containsKey("storeFile")) {
            create("release") {
                storeFile = file(keystoreProperties["storeFile"] as String)
                storePassword = keystoreProperties["storePassword"] as String
                keyAlias = keystoreProperties["keyAlias"] as String
                keyPassword = keystoreProperties["keyPassword"] as String
            }
        }
    }

    buildTypes {
        // Staging: the local development build, debug-signed.
        create("staging") {
            initWith(getByName("debug"))
            isDebuggable = true
            isJniDebuggable = true
            signingConfig = signingConfigs.getByName("debug")
            matchingFallbacks += listOf("debug")
        }
        // Release: the build to hand to someone else. Same code as staging,
        // but NOT debuggable - a debuggable APK lets anyone who installs it
        // read and modify the app's private data and attach to the process.
        // Unsigned unless keystore.properties provides the release key.
        getByName("release") {
            isMinifyEnabled = false
            signingConfig = signingConfigs.findByName("release")
        }
    }

    sourceSets {
        getByName("main") {
            // The complete game data tree (gfx, audio, data, manual.txt, licenses)
            assets.srcDirs("../../installed_files")
            // SDL2's Java glue (SDLActivity etc.) straight from the vendored SDL source
            java.srcDirs(
                "src/main/java",
                "../../third_party/SDL/src/SDL2-2.30.9/android-project/app/src/main/java"
            )
        }
    }
}

androidComponents {
    onVariants { variant ->
        val genTask = tasks.register<GenerateAssetManifestTask>(
            "generate${variant.name.replaceFirstChar { it.uppercase() }}AssetManifest"
        ) {
            gameDataDir.set(layout.projectDirectory.dir("../../installed_files"))
            outputDir.set(layout.buildDirectory.dir("generated/assetmanifest/${variant.name}"))
        }
        variant.sources.assets?.addGeneratedSourceDirectory(
            genTask,
            GenerateAssetManifestTask::outputDir
        )
    }
}
