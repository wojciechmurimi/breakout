#!/usr/bin/env bash
set -euo pipefail

PROJECT_NAME="ROOT"
IMGUI_EXAMPLE="/home/m/gl/imgui/examples/example_android_opengl3/android/"

ANDROID_HOME="/home/m/Android"
LEVEL="34.0.0"

ANDROID_JAR="$ANDROID_HOME/platforms/android-34/android.jar"

MIN_SDK=21
TARGET_SDK=34

BUILD_TOOL="$ANDROID_HOME/build-tools/$LEVEL"
PLATFORM_TOOL="$ANDROID_HOME/platform-tools"

#cmake ../ -DANDROID_NDK=$ANDROID_NDK -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake  -DANDROID_PLATFORM=android-21 -DCMAKE_BUILD_TYPE=Debug -DANDROID_STL=c++_shared -DANDROID_ABI=arm64-v8a

#mkdir $PROJECT_NAME
#cd $PROJECT_NAME

mkdir -p out/res
mkdir -p out/classes
mkdir -p out/dex

#cp $IMGUI_EXAMPLE .

javac -bootclasspath "$ANDROID_JAR" --source 8 --target 8 -d ./out/classes app/src/main/java/MainActivity.java

$BUILD_TOOL/d8 --output out/dex out/classes/imgui/example/android/MainActivity.class --lib "$ANDROID_JAR"

$BUILD_TOOL/aapt2 compile --dir out/res -o out/

$BUILD_TOOL/aapt2 link -o out/unsigned.apk -I "$ANDROID_JAR" --manifest app/src/main/AndroidManifest.xml --min-sdk-version $MIN_SDK --target-sdk-version $TARGET_SDK

zip -j out/unsigned.apk out/dex/classes.dex

mkdir -p lib/arm64-v8a

cp $ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/21/{libc,libdl,libEGL,libGLESv3,liblog,libm,libandroid}.so lib/arm64-v8a/

cp $ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so lib/arm64-v8a/

cp ../build/libImGuiExample.so lib/arm64-v8a/libImGuiExample.so

zip -u out/unsigned.apk lib/arm64-v8a/* 

rm -f out/aligned.apk

$BUILD_TOOL/zipalign -v 4 out/unsigned.apk out/aligned.apk

$BUILD_TOOL/apksigner sign --ks "dbg.keystore" --ks-key-alias "dbgkey" --out out/signed.apk out/aligned.apk

$PLATFORM_TOOL/adb install -r out/signed.apk

$PLATFORM_TOOL/adb shell monkey -p imgui.example.android 1
