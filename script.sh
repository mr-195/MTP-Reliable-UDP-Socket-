#!/bin/bash

# gcc -Wall myinitmsocket.c -o myinitmsocket && ./myinitmsocket


for ((i=0;i<10;i++)) do
    ipcrm -m "$((65544+i))"
done
