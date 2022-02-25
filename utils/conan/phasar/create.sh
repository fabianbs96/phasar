#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

(
    cd "$(dirname "$0")"
    if [ "$#" -eq "0" ]; then
        echo "[error] You didn't provide a phasar version"
        last_version="$(grep -Pzo '[^"]*":\v*\s*(?=url:)' conandata.yml | tr -d '\0' | tail -n 2)" # tail produces binary data -> grep doesn't like it directly
        last_version="$(echo "$last_version" | grep -Poe '[^ "]*(?=":)' )"
        echo "[error] last release is \"$last_version\""
        echo "[error] use from within phasar directory \"develop\""
        exit 10
    else
        readonly phasar_version="$1"
    fi

    bash ../scripts/clear.sh "phasar" "$phasar_version"

    options=()
    for llvm_shared in False; do
    for boost_shared in False; do
        options+=("-o llvm-core:shared=$llvm_shared -o boost:shared=$boost_shared")
    done
    done

    bash ../scripts/create.sh "phasar" "$phasar_version" "${options[@]}"
)
