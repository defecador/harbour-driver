# Source this before configuring/building the Android APK (see README, "Android").
export JAVA_HOME=$HOME/Android/jdk17                 # JDK 17: Gradle/AGP of Qt 6.7 reject newer JDKs
export ANDROID_SDK_ROOT=$HOME/Android/sdk-qt         # SDK view without API 35 (Qt 6.7's AGP can't read it)
export ANDROID_HOME=$ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/26.1.10909125
export QMapLibre_Android_DIR=$(echo $HOME/Qt/maplibre-android/*)
export ANDROID_OPENSSL_DIR=$HOME/Qt/android_openssl
export PATH=$JAVA_HOME/bin:$PATH
