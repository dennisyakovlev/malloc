#!/bin/sh

base_dir=build/tests

make tests >> /dev/null
if [ "$?" -ne 0 ]; then
    exit 1
fi

for group in "$base_dir"/*; do
    printf "Testing Group \033[1;35m$(basename $group)\033[0m\n"

    for file in "$group"/*; do
        "$file"
        if [ "$?" -ne 0 ]; then
            printf "\033[1;31mError\033[0m"
        else
            printf "\t \033[1;32mPass\033[0m"
        fi
        printf " $(basename $file)\n"
    done
done
