#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "${script_dir}/.." && pwd)"

target="${1:-win64}"
case "$target" in
  win64)
    toolchain_file="${project_root}/cmake/win64.toolchain"
    stage_pattern="${project_root}/mariadb-__VER__-winx64"
    ;;
  *)
    printf 'Error: unsupported target %s\n' "$target" >&2
    exit 1
    ;;
esac

for tool in cmake curl gpg make tar perl; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'Error: required tool %s is not installed\n' "$tool" >&2
    exit 1
  fi
done

if [[ ! -f "$toolchain_file" ]]; then
  printf 'Error: missing toolchain file %s\n' "$toolchain_file" >&2
  exit 1
fi

ver="$(dpkg-query -W -f='${Version}' libmariadbd-dev 2>/dev/null || true)"
if [[ -z "$ver" ]]; then
  printf 'Error: package %s is not installed\n' 'libmariadbd-dev' >&2
  exit 1
fi

ver=${ver#*:}
ver=${ver%%-*}

if [[ -z "$ver" ]]; then
  printf 'Error: could not determine MariaDB version from %s\n' 'libmariadbd-dev' >&2
  exit 1
fi

src_dir="${project_root}/mariadb-${ver}"
native_build_dir="${project_root}/mariadb-${ver}-native-build"
cross_build_dir="${project_root}/mariadb-${ver}-winx64-build"
stage_dir="${stage_pattern/__VER__/$ver}"
file="${project_root}/mariadb-${ver}.tar.gz"
sig_file="${file}.asc"

base="https://dlm.mariadb.com"
page="$base/browse/mariadb_server/$ver/?flat=1"
signing_key_url="https://mariadb.org/mariadb_release_signing_key.asc"
expected_fingerprint="177F4010FE56CA3336300305F1656F24C74CD1D8"
html="$(curl -fsSL --retry 5 --retry-delay 2 --retry-all-errors "$page")"
url="$({
  printf '%s\n' "$html" |
    grep -oE 'https://dlm\.mariadb\.com/[0-9]+/MariaDB/mariadb-'"$ver"'/source/mariadb-'"$ver"'\.tar\.gz' |
    head -n1 |
    tr -d '\n'
} || true)"
sig_url="$({
  printf '%s\n' "$html" |
    grep -oE 'https://dlm\.mariadb\.com/[0-9]+/MariaDB/mariadb-'"$ver"'/source/mariadb-'"$ver"'\.tar\.gz\.asc' |
    head -n1 |
    tr -d '\n'
} || true)"

if [[ -z "${url:-}" ]]; then
  printf 'Error: could not find %s on %s\n' "$(basename "$file")" "$page" >&2
  exit 1
fi

if [[ -z "${sig_url:-}" ]]; then
  printf 'Error: could not find %s on %s\n' "$(basename "$sig_file")" "$page" >&2
  exit 1
fi

[[ "$url" == http* ]] || url="$base$url"
[[ "$sig_url" == http* ]] || sig_url="$base$sig_url"
keyring_dir="$(mktemp -d)"
trap 'rm -rf "$keyring_dir"' EXIT

verify_download() {
  gpg --batch --quiet --homedir "$keyring_dir" --verify "$sig_file" "$file" >/dev/null 2>&1
}

curl -fsSL --retry 5 --retry-delay 2 --retry-all-errors "$signing_key_url" |
  gpg --batch --quiet --homedir "$keyring_dir" --import >/dev/null 2>&1
actual_fingerprint="$({
  gpg --batch --homedir "$keyring_dir" --with-colons --fingerprint "signing-key@mariadb.org" |
    grep '^fpr:' |
    head -n1 |
    cut -d: -f10
} || true)"

if [[ "$actual_fingerprint" != "$expected_fingerprint" ]]; then
  printf 'Error: unexpected MariaDB signing key fingerprint: %s\n' "$actual_fingerprint" >&2
  exit 1
fi

if [[ -f "$file" && -f "$sig_file" ]] && verify_download; then
  printf 'Using existing verified: %s %s\n' "$(basename "$file")" "$(basename "$sig_file")"
else
  printf 'Downloading: %s\n' "$(basename "$file")"
  curl -fsSL --retry 5 --retry-delay 2 --retry-all-errors -o "$file" "$url"
  printf 'Downloading: %s\n' "$(basename "$sig_file")"
  curl -fsSL --retry 5 --retry-delay 2 --retry-all-errors -o "$sig_file" "$sig_url"

  printf 'Verifying: %s\n' "$(basename "$file")"
  if ! verify_download; then
    printf 'Error: signature verification failed for %s\n' "$(basename "$file")" >&2
    exit 1
  fi
fi

printf 'Downloaded and verified: %s %s\n' "$(basename "$file")" "$(basename "$sig_file")"

if [[ ! -d "$src_dir" ]]; then
  printf 'Extracting: %s\n' "$(basename "$file")"
  rm -rf "$src_dir"
  tar -xf "$file" -C "$project_root"
fi

python3 - "$src_dir" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'win/packaging/CMakeLists.txt'
text = path.read_text()
old = '  MESSAGE_ONCE("unknown upgrade code for arch ${WIX_ARCH}")\n'
new = '  MESSAGE(STATUS "unknown upgrade code for arch ${WIX_ARCH}")\n'
if old in text:
    path.write_text(text.replace(old, new, 1))

pcre_path = Path(sys.argv[1]) / 'cmake/pcre.cmake'
pcre_text = pcre_path.read_text()
old = '    IF(WIN32)\n      SET(PCRE_STATIC "-static")\n    ELSE()\n      SET(PCRE_STATIC "")\n    ENDIF()\n'
new = '    SET(PCRE_STATIC "")\n'
if old in pcre_text:
    pcre_path.write_text(pcre_text.replace(old, new, 1))
PY

if [[ ! -f "$native_build_dir/import_executables.cmake" ]]; then
  printf 'Configuring native MariaDB helper tools\n'
  rm -rf "$native_build_dir"
  cmake -S "$src_dir" -B "$native_build_dir" -DCMAKE_BUILD_TYPE=Release -DWITH_UNIT_TESTS=OFF -DWITH_SSL=bundled -DWITH_ZLIB=system -DDISABLE_SHARED=ON
  printf 'Building native MariaDB helper tools\n'
  cmake --build "$native_build_dir" --target import_executables -- -j"$(nproc)"
fi

printf 'Configuring MariaDB embedded cross-build\n'
rm -rf "$cross_build_dir" "$stage_dir"
cmake -S "$src_dir" -B "$cross_build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$toolchain_file" \
  -DIMPORT_EXECUTABLES="$native_build_dir/import_executables.cmake" \
  -DHAVE_PTHREAD_RWLOCK_RDLOCK=0 \
  -DHAVE_RWLOCK_INIT=0 \
  -DHAVE_PTHREAD_ATTR_CREATE=0 \
  -DHAVE_PTHREAD_ATTR_GETSTACKSIZE=0 \
  -DHAVE_PTHREAD_ATTR_SETSCOPE=0 \
  -DHAVE_PTHREAD_ATTR_SETSTACKSIZE=0 \
  -DHAVE_PTHREAD_CONDATTR_CREATE=0 \
  -DHAVE_PTHREAD_GETAFFINITY_NP=0 \
  -DHAVE_PTHREAD_INIT=0 \
  -DHAVE_PTHREAD_KEY_DELETE=0 \
  -DHAVE_PTHREAD_SIGMASK=0 \
  -DHAVE_PTHREAD_YIELD_NP=0 \
  -DHAVE_PTHREAD_YIELD_ZERO_ARG=0 \
  -DHAVE_SIGWAIT=0 \
  -DHAVE_SIGWAITINFO=0 \
  -DHAVE_POSIX_SIGNALS=0 \
  -DWITH_EMBEDDED_SERVER=ON \
  -DWITH_UNIT_TESTS=OFF \
  -DWITH_WSREP=OFF \
  -DWITHOUT_DYNAMIC_PLUGINS=ON \
  -DPLUGIN_OQGRAPH=NO \
  -DPLUGIN_CONNECT=NO \
  -DPLUGIN_MROONGA=NO \
  -DPLUGIN_ROCKSDB=NO \
  -DPLUGIN_ARCHIVE=NO \
  -DPLUGIN_BLACKHOLE=NO \
  -DPLUGIN_FEDERATEDX=NO \
  -DPLUGIN_PERFSCHEMA=NO \
  -DPLUGIN_SPIDER=NO \
  -DPLUGIN_COLUMNSTORE=NO \
  -DPLUGIN_AWS_KEY_MANAGEMENT=NO \
  -DPLUGIN_S3=NO \
  -DPLUGIN_AUTH_GSSAPI=NO \
  -DPLUGIN_AUTH_PAM=NO \
  -DPLUGIN_FEEDBACK=NO \
  -DPLUGIN_SERVER_AUDIT=NO \
  -DPLUGIN_TYPE_GEOM=NO \
  -DPLUGIN_TYPE_INET=NO \
  -DPLUGIN_TYPE_UUID=NO \
  -DPLUGIN_USERSTAT=NO \
  -DPLUGIN_USER_VARIABLES=NO \
  -DWITH_SSL=bundled \
  -DWITH_ZLIB=system \
  -DDISABLE_SHARED=ON

printf 'Building MariaDB embedded cross-build\n'
cmake --build "$cross_build_dir" --target mysqlserver comp_err comp_sql -- -j"$(nproc)"

printf 'Staging MariaDB embedded artifacts into %s\n' "$(basename "$stage_dir")"
mkdir -p "$stage_dir/lib" "$stage_dir/include" "$stage_dir/share/english"
cp -a "$src_dir/include/." "$stage_dir/include/"
cp -a "$cross_build_dir/include/." "$stage_dir/include/"
cp -a "$cross_build_dir/libmysqld/mysqlserver.lib" "$stage_dir/lib/"
cp -a "$cross_build_dir/sql/share/english/errmsg.sys" "$stage_dir/share/english/"
mkdir -p "$stage_dir/share"
cp -a "$src_dir/scripts/mariadb_system_tables.sql" "$stage_dir/share/"
cp -a "$src_dir/scripts/mariadb_system_tables_data.sql" "$stage_dir/share/"
cp -a "$src_dir/scripts/fill_help_tables.sql" "$stage_dir/share/"

printf 'MariaDB embedded tree prepared in %s\n' "$stage_dir"
