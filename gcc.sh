#!/bin/sh
-include $(SPEC)/benchspec/Makefile.defaults
list='find . *.c'

for i in ${list}
do
	if [ ${#i} -gt 2 ]
	then
		clang -I ./ -std=gnu89 -c -emit-llvm ${i:0:${#i}-2}.c -o ${i:0:${#i}-2}.bc
	fi
done

listbc='find . *.bc'
for i in ${listbc}
do
    if [ ${#i} -gt 3 ]
    then
        ../bin/llvm-link ${i:0:${#i}-3}.bc main.bc -o main.bc
    fi
done
