#!/bin/bash
set -o errexit
source "$(dirname "$0")/config.sh" "$@" # include config
set -o nounset
set -o pipefail

# preparation / cleanup
rm -rf "./logs" || True
mkdir logs

# default remote = current dir for version != develop
# if version develop -> use local build
conanfile_path="."
create_directory="."
log_path="./logs"
if [ "$VERSION" = "develop" ]; then
    conanfile_path="utils/conan/phasar"
    log_path="utils/conan/phasar/logs"
    create_directory="./../../../"
fi

# is conan recipe here? -> build
if [ -f "conanfile.py" ]; then
    echo "create recipe $log_path/build.log"

    (
        cd "$create_directory"
        if conan create --build=missing "$conanfile_path" "$VERSION@$TARGET" &> "$log_path/build.log"; then
            echo "create successful $log_path/build.log"
        else
            echo "create failed $log_path/build.log"
            exit 10
        fi
    )

# else assume its in of the remotes
else
    echo "download recipe $log_path/download.log"
    if conan download --recipe "$NAME/$VERSION@" &> "$log_path/download.log"; then
        echo "download successful $log_path/download.log"
    else
        echo "download failed $log_path/download.log"
        exit 20
    fi

    echo "copy recipe $log_path/copy.log"
    if conan copy --force "$NAME/$VERSION" "$TARGET" &> "logs/copy.log"; then
        echo "copy successful $log_path/copy.log"
    else
        echo "copy failed $log_path/copy.log"
        exit 30
    fi
fi

# step 3 ensure that every shared / build_type is completed
successful_counter=0
for shared in False; do
for release in Debug Release; do
for option in "${CONAN_ARGS[@]}"; do
    options="$option -o $NAME:shared=$shared -s build_type=$release"
    dir="$(mktemp -d)"
    log="$options"
    log_name="$(sed -E "s/[^a-zA-Z0-9:_.=-]/_/g" <<< "$log")"
    echo ""
    echo "using configuration: $options"

    # create binary with given options
    echo "install $log_path/$log_name.install.log"
    #shellcheck disable=SC2086 #needed for options
    if conan install -if "$dir" --build=missing \
            $options \
            "$NAME/$VERSION@$TARGET" &> "./logs/$log.install.log"; then
        mkdir logs/successful &> /dev/null || true
        mv "./logs/$log.install.log" "./logs/successful/$log_name.install.log"
        echo "install successful see log $log_path/successful/$log_name.install.log"
        successful_counter=$((successful_counter + 1))
    else
        mkdir logs/failed &> /dev/null || true
        mv "./logs$log.install.log" "./logs/failed/$log_name.install.log"
        echo "install failed see log $log_path/failed/$log_name.install.log"
    fi
    rm -rf "$dir" || true

    # test binary with given options
    if [ -f "conanfile.py" ]; then
        echo "test $log_path/$log.install.log"

        #shellcheck disable=SC2086 #needed for options
        if conan test \
            $options \
            test_package "$NAME/$VERSION@$TARGET" &> "./logs/$log.test.log"; then
            mkdir logs/successful &> /dev/null || true
            mv "./logs/$log.test.log" "./logs/successful/$log_name.test.log"
            echo "test successful see log $log_path/successful/$log_name.test.log"
        else
            mkdir logs/failed &> /dev/null || true
            mv "./logs/$log.test.log" "./logs/failed/$log_name.test.log"
            echo "test failed see log $log_path/failed/$log_name.test.log"
        fi
    fi # no else because its probably an official one, we dont have a test_package folder here
done
done
done

echo ""
echo "expected at least $successful_counter packages"
#shellcheck disable=SC2207
current_packages=($(conan search "$NAME/$VERSION@$TARGET" | grep -Po '(?<=Package_ID:\s).*'))
echo "found ${#current_packages[@]} packages"

if [ "${#current_packages[@]}" -lt "$successful_counter" ]; then
    echo "some options arent applied at buildtime"
fi
