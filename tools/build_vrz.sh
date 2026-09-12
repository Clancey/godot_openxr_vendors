#!/usr/bin/env bash
# Reproducible build of the libraries VRZombies ships in
# addons/godotopenxrvendors/.bin/.
#
# The critical detail: OpenXRMetaBoundaryVisibilityExtension (and
# OpenXRStationaryReferenceSpaceExtension) live behind `#ifdef
# META_HEADERS_ENABLED` in plugin/src/main/cpp/register_types.cpp. SConstruct
# only defines that when `meta_headers=` points at Meta's OpenXR *preview*
# headers. Build without it and the classes are silently compiled out, so
# `Engine.get_singleton("OpenXRMetaBoundaryVisibilityExtension")` returns null
# at runtime and the Quest guardian can never be hidden. That is exactly how
# the previously shipped libraries were built.
#
# Those headers are under the Oculus SDK License Agreement, so they are NOT
# vendored into this public fork. Fetch them once with fetch_meta_headers.sh
# and point META_HEADERS at the result.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

META_HEADERS="${META_HEADERS:-$REPO_ROOT/../third_party/meta_openxr_preview}"
SCONS="${SCONS:-/opt/homebrew/anaconda3/bin/scons}"
NDK_VERSION="${NDK_VERSION:-28.1.13356709}"
JOBS="${JOBS:-8}"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/SDK}"
JDK17="${JDK17:-/Library/Java/JavaVirtualMachines/temurin-17.jdk/Contents/Home}"

if [[ ! -f "$META_HEADERS/meta_boundary_visibility.h" ]]; then
	echo "error: Meta preview headers not found at $META_HEADERS" >&2
	echo "       run tools/fetch_meta_headers.sh first (or set META_HEADERS)." >&2
	exit 1
fi

COMMON=(
	"meta_headers=$META_HEADERS/"
	"custom_api_file=thirdparty/godot_cpp_gdextension_api/extension_api.json"
	"build_profile=thirdparty/godot_cpp_build_profile/build_profile.json"
	"-j$JOBS"
)

for target in template_debug template_release; do
	echo "==> android arm64 $target"
	"$SCONS" platform=android arch=arm64 "target=$target" \
		"ndk_version=$NDK_VERSION" "${COMMON[@]}"
done

for target in template_debug template_release; do
	echo "==> macos universal $target"
	"$SCONS" platform=macos arch=universal "target=$target" "${COMMON[@]}"
done

echo "==> gradle meta aars"
JAVA_HOME="$JDK17" ./gradlew :plugin:clean :plugin:assembleMetaDebug \
	:plugin:assembleMetaRelease "-Pmeta_headers=$META_HEADERS/" \
	--no-daemon --console=plain

echo "==> verifying boundary visibility extension is compiled in"
for lib in demo/addons/godotopenxrvendors/.bin/android/template_*/arm64/libgodotopenxrvendors.so; do
	if strings "$lib" | grep -q "XR_META_boundary_visibility"; then
		echo "ok   $lib"
	else
		echo "FAIL $lib is missing XR_META_boundary_visibility" >&2
		exit 1
	fi
done

echo "done. copy demo/addons/godotopenxrvendors/.bin/** into the game repo."
