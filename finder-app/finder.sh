#!/bin/sh

if [ $# -ne 2 ]; then
    echo "ERROR: Invalid number of arguments."
    exit 1
fi

filesdir=$1
searchstr=$2
nfiles=0
nlines=0

recurse() {

    path=$1

    if [ -d $path ]; then
        for FILE in "${path}/"* ; do
            recurse $FILE
        done
    elif [ -f $path ]; then
        nlines=`expr $nlines + $(grep -c $searchstr $path)`
        nfiles=`expr $nfiles + 1`
    fi
}

if [ -d $filesdir ]; then

    recurse $filesdir

    echo "The number of files are $nfiles and the number of matching lines are $nlines"

else

    echo "$filesdir is not a valid directory."
    exit 1

fi
