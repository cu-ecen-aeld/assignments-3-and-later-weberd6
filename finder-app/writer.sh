#!/bin/bash

if [ $# -eq 2 ]; then

    writefile=$1
    writestr=$2

    if [ -e $writefile ]; then
        rm $writefile
    fi

    if [ ! -d $(dirname $writefile) ]; then
        mkdir -p $(dirname $writefile)
    fi

    touch $writefile

    echo $writestr > $writefile

else

    echo "ERROR: Invalid number of arguments."
    exit 1

fi
