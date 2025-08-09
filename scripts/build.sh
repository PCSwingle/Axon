./cmake-build-debug/Axon $1 -o - | clang testdir/c_lib.c -x ir - -o testdir/a.out
