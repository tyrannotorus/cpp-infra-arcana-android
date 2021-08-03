#!/usr/bin/env sh

set -eu

# Extra parameters can be added as arguments to this script, e.g.:
# $ ./clang-tidy.sh -fix

# This expects the files .clang-tidy and compile_commands.json
# run-clang-tidy -j=4 -style=None -extra-arg=-Wno-unknown-warning-option $*


# TODO: This is a workaround needed due to two issues:
#
# 1) The same source files occur multiple times for different targets (release,
# debug, test) in compile_commands.json. See this issue for a possible actual
# solution (when/if it is resolved):
#
# https://gitlab.kitware.com/cmake/cmake/issues/19462.
#
# 2) "Third party" source files, which should not be analyzed, exists in
# compile_commands.json (tinyxml2). By running clang-tidy on an explicit set of
# files (instead of all files in compile_commands.json), we can control which
# files are included. 3PP code should probably be separated in the cmake project
# somehow though.
#
IFS="
"

headers_regex="include/[a-z0-9_]*.hpp"
sources_regex="src/[a-z0-9_]*.cpp"

rm -f clang-tidy-commands.log

for c in $(grep -oE "[^\"]*ia-debug.dir[^\"]*" compile_commands.json); do
    f=$(echo $c | grep -oE "[^ ]*.cpp$")

    # echo "================================================================================"
    echo "${f}"
    # echo "================================================================================"

    if ! echo ${f} | grep -qE "${sources_regex}"; then
        echo "Skipping ${f}"
        echo ""
        continue
    fi

    cmd="clang-tidy"
    cmd=${cmd}" -quiet"
    cmd=${cmd}" -extra-arg=-Wno-unknown-warning-option"
    cmd=${cmd}" -header-filter=${headers_regex}"
    cmd=${cmd}" $*"
    cmd=${cmd}" ${f}"
    cmd=${cmd}" -- ${c}"

    echo ${cmd} >> clang-tidy-commands.log
    eval ${cmd}
    # echo ""
done
