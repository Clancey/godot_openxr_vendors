#!/usr/bin/env bash
# Fetches Meta's OpenXR *preview* headers, which gate META_HEADERS_ENABLED and
# therefore decide whether OpenXRMetaBoundaryVisibilityExtension exists at all.
#
# These headers are covered by the Oculus SDK License Agreement, so they are
# deliberately never committed to this public fork. They land outside the repo
# by default.
#
# Upstream CI pulls the whole ovr_openxr_mobile_sdk zip from Meta's CDN
# (see .github/workflows/build-addon-on-push.yml, META_OPENXR_HEADERS_URL).
# That CDN host is not reachable from every network, so this script prefers
# Meta's official GitHub mirror and falls back to the CDN.
#
# Note: SDK v85 dropped meta_body_tracking_calibration.h, but
# openxr_fb_body_tracking_extension.h still includes it, so it is fetched
# separately from an official SDK fork that still carries it.
set -euo pipefail

DEST="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/third_party/meta_openxr_preview}"
SDK_TAG="${SDK_TAG:-v85}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "==> cloning meta-quest/Meta-OpenXR-SDK $SDK_TAG"
git clone --depth 1 --branch "$SDK_TAG" --filter=blob:none --sparse \
	https://github.com/meta-quest/Meta-OpenXR-SDK.git "$WORK/sdk"
git -C "$WORK/sdk" sparse-checkout set OpenXR/meta_openxr_preview

mkdir -p "$DEST"
cp -R "$WORK/sdk/OpenXR/meta_openxr_preview/." "$DEST/"

# Still referenced by openxr_fb_body_tracking_extension.h, removed in v85.
if [[ ! -f "$DEST/meta_body_tracking_calibration.h" ]]; then
	echo "==> fetching meta_body_tracking_calibration.h (dropped in $SDK_TAG)"
	curl -fsSL -o "$DEST/meta_body_tracking_calibration.h" \
		"https://raw.githubusercontent.com/BOD88/Meta-OpenXR-SDK./master/OpenXR/meta_openxr_preview/meta_body_tracking_calibration.h"
fi

for required in meta_boundary_visibility.h meta_body_tracking_calibration.h \
		meta_body_tracking_fidelity.h extx2_stationary_reference_space.h; do
	if [[ ! -f "$DEST/$required" ]]; then
		echo "FAIL missing required header: $required" >&2
		exit 1
	fi
done

echo "done. headers in $DEST"
echo "now run: META_HEADERS=$DEST tools/build_vrz.sh"
