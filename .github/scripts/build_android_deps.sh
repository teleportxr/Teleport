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
#   libavstream/thirdparty/curl-7.74.0-android-arm64-v8a/include/curl/*  (headers)
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

build_openssl
openssl_cmake_args >/dev/null   # resolves OSSL_LIBDIR (lib vs lib64)
cp -v "$OSSL_LIBDIR/libssl.a"    "$EXT/libssl.a"
cp -v "$OSSL_LIBDIR/libcrypto.a" "$EXT/libcrypto.a"
build_curl
build_libdatachannel

echo "== Staged Android dependencies =="
ls -l "$EXT"
echo "curl headers: $CURL_HDR_DIR"

# Fail early if anything the AGDE link step needs is missing.
missing=0
for lib in libssl.a libcrypto.a libcurl.a libdatachannel-static.a libjuice-static.a libsrtp2.a libusrsctp.a; do
	if [ ! -f "$EXT/$lib" ]; then
		echo "ERROR: missing $lib" >&2
		missing=1
	fi
done
exit "$missing"
