#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root="$PWD/.cache"
src="$root/libdatachannel-src"
prefix="$root/p2p"
if [[ ! -f "$src/CMakeLists.txt" ]]; then
  git clone --no-checkout https://github.com/paullouisageneau/libdatachannel.git "$src"
fi
# Fixed release commit, including pinned submodules (MPL-2.0).
git -C "$src" checkout --detach 4e4f4892dccb2a57fe3a490d0c9d958de4244e74
git -C "$src" submodule update --init --recursive --depth 1 deps/libjuice deps/usrsctp deps/plog
extra=()
if [[ "$(uname -s)" == Darwin ]]; then
  extra+=("-DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)")
fi
if [[ -n "${MACOSX_DEPLOYMENT_TARGET:-}" ]]; then
  extra+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET")
fi
cmake -S "$src" -B "$root/p2p-build" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
  -DBUILD_SHARED_LIBS=ON -DBUILD_SHARED_DEPS_LIBS=OFF \
  -DNO_MEDIA=ON -DNO_WEBSOCKET=ON -DNO_EXAMPLES=ON -DNO_TESTS=ON \
  "${extra[@]}"
cmake --build "$root/p2p-build" --parallel 3
cmake --install "$root/p2p-build"
mkdir -p "$prefix/lib/pkgconfig" "$prefix/share/licenses/libdatachannel"
cat > "$prefix/lib/pkgconfig/libdatachannel.pc" <<PC
prefix=$prefix
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: libdatachannel
Description: WebRTC Data Channels
Version: 0.24.2
Libs: -L\${libdir} -ldatachannel
Cflags: -I\${includedir}
PC
cp "$src/LICENSE" "$prefix/share/licenses/libdatachannel/LICENSE"
for dep in libjuice usrsctp plog; do
  license="$src/deps/$dep/LICENSE"
  [[ -f "$license" ]] || license="$license.md"
  cp "$license" "$prefix/share/licenses/libdatachannel/$dep-LICENSE"
done
