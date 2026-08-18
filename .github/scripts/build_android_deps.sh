#!/usr/bin/env bash
#
# build_android_deps.sh
#
# Cross-compiles, for Android arm64-v8a, the prebuilt static libraries that the
# AGDE solution (build_android_vs) links from $(SolutionDir)external_libs/, plus
# the curl headers that libavstream includes. These artefacts are normally
# staged by hand on a developer machine (and are .gitignored), so CI must
# reproduce them before running MSBuild.
#
# Produced (relative to the repo root, into $STAGE):
#   build_android_vs/external_libs/libssl.a  libcrypto.a               (OpenSSL)
#   build_android_vs/external_libs/libcurl.a                           (curl)
#   build_android_vs/external_libs/libdatachannel-static.a
#   build_android_vs/external_libs/libjuice-static.a
#   build_android_vs/external_libs/libsrtp2.a
#   build_android_vs/external_libs/libusrsctp.a
#   build_android_vs/external_libs/libktx.a                            (KTX)
#   build_pc_client/ktx/include/ktx.h  include/KHR/khr_df.h            (headers)
#   libavstream/thirdparty/curl-7.74.0-android-arm64-v8a/include/curl/*  (headers)
#   build_android_vs/_deps/magic_enum_src/include/magic_enum/...       (headers)
#
# NDK-built arm64 archives are host-independent, so libraries produced here on
# Linux link cleanly into the Windows AGDE build.
#
# Inputs (environment):
#   ANDROID_NDK_HOME   path to the NDK (required)
#   ANDROID_API        Android API level to target (default 29, matches minSdk)
#   OPENSSL_VERSION    OpenSSL tag to build (default 3.0.15)
#
set -euo pipefail

: "${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME to the Android NDK path}"
ANDROID_API="${ANDROID_API:-29}"
OPENSSL_VERSION="${OPENSSL_VERSION:-3.0.15}"

ABI="arm64-v8a"
TRIPLE="aarch64-linux-android"
NPROC="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

# Repo root = two levels up from this script (.github/scripts/..).
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="${WORK:-$ROOT/build_android_deps}"
STAGE="${STAGE:-$WORK/stage}"
EXT="$STAGE/build_android_vs/external_libs"
CURL_HDR_DIR="$STAGE/libavstream/thirdparty/curl-7.74.0-android-arm64-v8a/include"
mkdir -p "$WORK" "$EXT" "$CURL_HDR_DIR"

TOOLCHAIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64"
NDK_CMAKE_TC="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"

OSSL_PREFIX="$WORK/openssl-install"
CURL_TAG="9eb8e07e026735f3d92a825fc0e5f46f83b80f3f"   # teleportxr/curl fork, matches libavstream/CMakeLists.txt

echo "== Android deps: NDK=$ANDROID_NDK_HOME API=$ANDROID_API ABI=$ABI OpenSSL=$OPENSSL_VERSION =="

# ---------------------------------------------------------------------------
# 1. OpenSSL (libssl.a, libcrypto.a) — the crypto backend for curl, libjuice,
#    libSRTP and libdatachannel.
# ---------------------------------------------------------------------------
build_openssl()
{
	if [ -f "$OSSL_PREFIX/lib/libssl.a" ] && [ -f "$OSSL_PREFIX/lib/libcrypto.a" ]; then
		echo "-- OpenSSL already built"
		return 0
	fi
	cd "$WORK"
	if [ ! -d openssl ]; then
		git clone --depth 1 --branch "openssl-$OPENSSL_VERSION" https://github.com/openssl/openssl.git openssl
	fi
	cd openssl
	# OpenSSL's android-arm64 target derives its own clang invocation from
	# ANDROID_NDK_ROOT and the toolchain on PATH; do not export CC/CXX here.
	export ANDROID_NDK_ROOT="$ANDROID_NDK_HOME"
	PATH="$TOOLCHAIN/bin:$PATH" ./Configure android-arm64 \
		-D__ANDROID_API__="$ANDROID_API" \
		no-shared no-tests \
		--prefix="$OSSL_PREFIX" --openssldir="$OSSL_PREFIX"
	PATH="$TOOLCHAIN/bin:$PATH" make -j"$NPROC"
	PATH="$TOOLCHAIN/bin:$PATH" make install_sw
}

# Emit the explicit -DOPENSSL_* cache variables that find_package(OpenSSL)
# needs. When cross-compiling for Android, the NDK toolchain sets
# CMAKE_FIND_ROOT_PATH_MODE_{LIBRARY,INCLUDE}=ONLY, so find_library/find_path
# search only the NDK sysroot and silently ignore OPENSSL_ROOT_DIR. Pointing
# CMake straight at the staged static archives and headers sidesteps that.
OSSL_LIBDIR=""
openssl_cmake_args()
{
	if [ -z "$OSSL_LIBDIR" ]; then
		if [ -f "$OSSL_PREFIX/lib/libcrypto.a" ]; then
			OSSL_LIBDIR="$OSSL_PREFIX/lib"
		elif [ -f "$OSSL_PREFIX/lib64/libcrypto.a" ]; then
			OSSL_LIBDIR="$OSSL_PREFIX/lib64"
		else
			echo "ERROR: OpenSSL static libs not found under $OSSL_PREFIX" >&2
			exit 1
		fi
	fi
	printf '%s ' \
		"-DOPENSSL_ROOT_DIR=$OSSL_PREFIX" \
		"-DOPENSSL_USE_STATIC_LIBS=ON" \
		"-DOPENSSL_INCLUDE_DIR=$OSSL_PREFIX/include" \
		"-DOPENSSL_CRYPTO_LIBRARY=$OSSL_LIBDIR/libcrypto.a" \
		"-DOPENSSL_SSL_LIBRARY=$OSSL_LIBDIR/libssl.a"
}

# ---------------------------------------------------------------------------
# 2. curl (libcurl.a) — built from the teleportxr fork against the OpenSSL above.
# ---------------------------------------------------------------------------
build_curl()
{
	cd "$WORK"
	if [ ! -d curl ]; then
		git clone https://github.com/teleportxr/curl.git curl
		git -C curl checkout "$CURL_TAG"
	fi
	rm -rf curl-build
	cmake -S curl -B curl-build \
		-DCMAKE_TOOLCHAIN_FILE="$NDK_CMAKE_TC" \
		-DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$ANDROID_API" \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF \
		-DCURL_USE_OPENSSL=ON $(openssl_cmake_args) \
		-DCURL_USE_LIBPSL=OFF -DUSE_LIBIDN2=OFF -DCURL_ENABLE_SSL=ON
	cmake --build curl-build -j"$NPROC"
	find curl-build -name 'libcurl.a' -exec cp -v {} "$EXT/libcurl.a" \;
	# Headers: the public curl/ headers plus the generated curl_config.h.
	cp -a curl/include/curl "$CURL_HDR_DIR/"
}

# ---------------------------------------------------------------------------
# 3. libdatachannel + bundled deps (libdatachannel-static.a, libjuice-static.a,
#    libsrtp2.a, libusrsctp.a). Uses OpenSSL (default), libjuice (not libnice).
# ---------------------------------------------------------------------------
build_libdatachannel()
{
	cd "$ROOT"
	rm -rf "$WORK/ldc-build"
	cmake -S thirdparty/libdatachannel -B "$WORK/ldc-build" \
		-DCMAKE_TOOLCHAIN_FILE="$NDK_CMAKE_TC" \
		-DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$ANDROID_API" \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF \
		-DNO_TESTS=ON -DNO_EXAMPLES=ON \
		-DUSE_NICE=OFF -DUSE_GNUTLS=OFF -DUSE_MBEDTLS=OFF \
		$(openssl_cmake_args)
	# datachannel-static is declared EXCLUDE_FROM_ALL, so the default "all"
	# target would build only the shared libdatachannel.so. Build the static
	# target explicitly; its deps (juice/srtp2/usrsctp) come along transitively.
	cmake --build "$WORK/ldc-build" --target datachannel-static -j"$NPROC"

	local b="$WORK/ldc-build"
	find "$b" -name 'libdatachannel-static.a' -exec cp -v {} "$EXT/libdatachannel-static.a" \;
	find "$b" -name 'libjuice-static.a'        -exec cp -v {} "$EXT/libjuice-static.a" \;
	find "$b" -name 'libsrtp2.a'               -exec cp -v {} "$EXT/libsrtp2.a" \;
	find "$b" -name 'libusrsctp.a'             -exec cp -v {} "$EXT/libusrsctp.a" \;
}

# (ozz-animation is no longer cross-compiled here: the Android build now gets it
# from the CMake-generated AGDE project — see thirdparty/ozz_animation/ and the
# TELEPORT_ANDROID_MINIMAL_SETUP configure step.)

build_openssl
openssl_cmake_args >/dev/null   # resolves OSSL_LIBDIR (lib vs lib64)
cp -v "$OSSL_LIBDIR/libssl.a"    "$EXT/libssl.a"
cp -v "$OSSL_LIBDIR/libcrypto.a" "$EXT/libcrypto.a"
build_curl
build_libdatachannel

# ---------------------------------------------------------------------------
# 3c. KTX-Software (libktx.a) — texture loading/saving used by ClientRender.
#     The PC CMake build fetches KTX into ${CMAKE_BINARY_DIR}/ktx; the AGDE
#     build does not run that path, so we cross-compile it here and stage the
#     headers under build_pc_client/ktx/include to match ClientRender_Android.
# ---------------------------------------------------------------------------
KTX_VERSION="v5.0.0-rc1"
KTX_SRC_DIR="$WORK/ktx"
KTX_BUILD_DIR="$WORK/ktx-build"
build_ktx()
{
	if [ -f "$EXT/libktx.a" ] && [ -f "$STAGE/build_pc_client/ktx/include/ktx.h" ] \
		&& [ -f "$STAGE/build_pc_client/ktx/include/KHR/khr_df.h" ]; then
		echo "-- KTX already built"
		return 0
	fi
	cd "$WORK"
	if [ ! -d "$KTX_SRC_DIR" ]; then
		git clone --depth 1 --branch "$KTX_VERSION" \
			https://github.com/KhronosGroup/KTX-Software.git "$KTX_SRC_DIR"
	fi
	rm -rf "$KTX_BUILD_DIR"
	cmake -S "$KTX_SRC_DIR" -B "$KTX_BUILD_DIR" \
		-DCMAKE_TOOLCHAIN_FILE="$NDK_CMAKE_TC" \
		-DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$ANDROID_API" \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF \
		-DKTX_FEATURE_TOOLS=OFF -DKTX_FEATURE_TESTS=OFF \
		-DKTX_FEATURE_GL_UPLOAD=OFF
	cmake --build "$KTX_BUILD_DIR" -j"$NPROC"

	find "$KTX_BUILD_DIR" -name 'libktx.a' -exec cp -v {} "$EXT/libktx.a" \;

	mkdir -p "$STAGE/build_pc_client/ktx/include"
	mkdir -p "$STAGE/build_pc_client/ktx/lib"
	# KTX v5 moved public headers from include/ to lib/include/ and internal
	# sources into lib/src/. Stage them into the v4-shaped layout that the
	# AGDE ClientRender_Android.vcxproj include paths expect.
	if [ -d "$KTX_SRC_DIR/lib/include" ]; then
		cp -a "$KTX_SRC_DIR/lib/include/." "$STAGE/build_pc_client/ktx/include/"
		cp -a "$KTX_SRC_DIR/lib/src/."     "$STAGE/build_pc_client/ktx/lib/"
		# version.h is generated into the build tree; flatten it into lib/ too.
		cp -v "$KTX_BUILD_DIR/lib/src/version.h" "$STAGE/build_pc_client/ktx/lib/version.h" 2>/dev/null || true
	else
		# KTX v4 layout (kept for compatibility with older pins).
		cp -a "$KTX_SRC_DIR/include/." "$STAGE/build_pc_client/ktx/include/"
		cp -a "$KTX_SRC_DIR/lib/."     "$STAGE/build_pc_client/ktx/lib/"
	fi
	# The public ktx.h includes <KHR/khr_df.h>. In v4 that header sat in include/ and so came
	# free with the copy above; v5 keeps its only copy in external/dfdutils/KHR, published to
	# CMake consumers through the ktx target's header FILE_SET BASE_DIRS. The hand-rolled AGDE
	# projects do not link that target, so stage it beside ktx.h as v4 had it. Idempotent, so
	# it is harmless on the v4 path.
	if [ -d "$KTX_SRC_DIR/external/dfdutils/KHR" ]; then
		mkdir -p "$STAGE/build_pc_client/ktx/include/KHR"
		cp -a "$KTX_SRC_DIR/external/dfdutils/KHR/." "$STAGE/build_pc_client/ktx/include/KHR/"
	fi
}

build_ktx

# ---------------------------------------------------------------------------
# 4. magic_enum header-only library used by Platform/Vulkan and ClientRender.
#    The AGDE .vcxproj files reference _deps/magic_enum_src/include, but the
#    Android build does not run CMake for firstparty/Platform, so we stage the
#    same version that the PC FetchContent build downloads.
# ---------------------------------------------------------------------------
MAGIC_ENUM_VERSION="0.9.5"
MAGIC_ENUM_DIR="$STAGE/build_android_vs/_deps/magic_enum_src"
stage_magic_enum()
{
	if [ -d "$MAGIC_ENUM_DIR/include" ]; then
		echo "-- magic_enum already staged"
		return 0
	fi
	cd "$WORK"
	rm -rf magic_enum magic_enum.zip
	curl -fsSL -o magic_enum.zip \
		"https://github.com/Neargye/magic_enum/archive/refs/tags/v$MAGIC_ENUM_VERSION.zip"
	unzip -q magic_enum.zip
	mkdir -p "$(dirname "$MAGIC_ENUM_DIR")"
	mv "magic_enum-$MAGIC_ENUM_VERSION" "$MAGIC_ENUM_DIR"
	rm -f magic_enum.zip
}

stage_magic_enum

echo "== Staged Android dependencies =="
ls -l "$EXT"
echo "curl headers: $CURL_HDR_DIR"
echo "magic_enum headers: $MAGIC_ENUM_DIR/include"
echo "ktx headers: $STAGE/build_pc_client/ktx/include"

# Fail early if anything the AGDE link step needs is missing.
missing=0
for lib in libssl.a libcrypto.a libcurl.a libdatachannel-static.a libjuice-static.a libsrtp2.a libusrsctp.a libktx.a; do
	if [ ! -f "$EXT/$lib" ]; then
		echo "ERROR: missing $lib" >&2
		missing=1
	fi
done
if [ ! -f "$MAGIC_ENUM_DIR/include/magic_enum/magic_enum.hpp" ]; then
	echo "ERROR: missing magic_enum/magic_enum.hpp" >&2
	missing=1
fi
if [ ! -f "$STAGE/build_pc_client/ktx/include/ktx.h" ]; then
	echo "ERROR: missing ktx.h" >&2
	missing=1
fi
exit "$missing"
