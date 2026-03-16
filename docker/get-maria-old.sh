#!/usr/bin/env bash

set -euo pipefail

# get current version of libmariadbd-dev
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

base="https://dlm.mariadb.com"
page="$base/browse/mariadb_server/$ver/?flat=1"
file="mariadb-$ver-winx64.zip"
sig_file="$file.asc"
signing_key_url="https://mariadb.org/mariadb_release_signing_key.asc"
expected_fingerprint="177F4010FE56CA3336300305F1656F24C74CD1D8"
html="$(curl -fsSL --retry 5 --retry-delay 2 --retry-all-errors "$page")"
url="$(
  printf '%s\n' "$html" |
  grep -oE 'https://dlm\.mariadb\.com/[0-9]+/MariaDB/mariadb-'"$ver"'/winx64-packages/mariadb-'"$ver"'-winx64\.zip' |
  head -n1 |
  tr -d '\n'
)"
sig_url="$(
  printf '%s\n' "$html" |
  grep -oE 'https://dlm\.mariadb\.com/[0-9]+/MariaDB/mariadb-'"$ver"'/winx64-packages/mariadb-'"$ver"'-winx64\.zip\.asc' |
  head -n1 |
  tr -d '\n'
)"

if [[ -z "${url:-}" ]]; then
  printf 'Error: could not find %s on %s\n' "$file" "$page" >&2
  exit 1
fi

if [[ -z "${sig_url:-}" ]]; then
  printf 'Error: could not find %s on %s\n' "$sig_file" "$page" >&2
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
  printf 'Using existing verified: %s %s\n' "$file" "$sig_file"
  exit 0
fi

printf 'Downloading: %s\n' "$file"
curl -fsSL --retry 5 --retry-delay 2 --retry-all-errors -o "$file" "$url"
printf 'Downloading: %s\n' "$sig_file"
curl -fsSL --retry 5 --retry-delay 2 --retry-all-errors -o "$sig_file" "$sig_url"

printf 'Verifying: %s\n' "$file"
if ! gpg --batch --quiet --homedir "$keyring_dir" --verify "$sig_file" "$file" >/dev/null 2>&1; then
  printf 'Error: signature verification failed for %s\n' "$file" >&2
  exit 1
fi

printf 'Downloaded and verified: %s %s\n' "$file" "$sig_file"

[ -d mariadb-${ver}-winx64 ] && rm -rf mariadb-${ver}-winx64
7z x ${file} \
    \*/include \
    \*/lib/libmariadb.dll \
    \*/share/english/errmsg.sys \

(
    export PATH=/usr/x86_64-w64-mingw32/bin:${PATH}

    cd mariadb-${ver}-winx64
    cd lib
    for dll in *.dll; do
        printf 'Creating Windows export library for %s\n' "${dll}"
        gendef -a ${dll}
        x86_64-w64-mingw32-dlltool -d ${dll%.*}.def -l ${dll}.a -k
        rm ${dll%.*}.def
    done
)
# rm ${file} ${sig_file}

exit 0
