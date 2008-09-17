#!/bin/sh
perl -p -i.orig -e "s/\r//g; s/\t+$//" types.txt
perl -pe 's/\t/;/g; s/\n/\r\n/s' < types.txt > types.csv

