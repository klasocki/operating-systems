#!/bin/bash


test_thread() {
    printf "\nThread number: $3\n\n"
    ./main "$3" "$2" "baboon.ascii.pgm" "$1" "filtered.ascii.pgm"
}

test_type() {
    echo "---------"
    echo "===== $2 ======"
    test_thread "$1" "$2" 1
    test_thread "$1" "$2" 2
    test_thread "$1" "$2" 4
    test_thread "$1" "$2" 8
    echo "---------"
}

test_filter() {
    printf "___________________\n"
    echo "Filter: $1"
    echo "__________________"
    test_type "$1" "block"
    test_type "$1" "interleaved"
}

test_filter "3x3"
test_filter "65x65"
test_filter "33x33"
