#!/bin/sh

function="$1"
build_dir=build
exe_name=out

mkdir -p $build_dir

case "$function" in
    profile)
        make profile > /dev/null
        cd $build_dir
        ./"$exe_name"
        # -Q no call graph
        # -J no annotated source code
        # -b no explanations
        # -z include functions in output which have no apperent time
        gprof -Q -J -b -z "$exe_name" gmon.out > profile.txt
        ;;
    debug)
        make debug
        ;;
    release)
        make release
        ;;
    *)
        echo "Enter arg correctly"
        ;;
esac
