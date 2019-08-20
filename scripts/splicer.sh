#!/bin/bash

# splicer.  runs after processor.  splices ten minutes worth of
# recordings into a new tape, adding generation loss as necessary.

# developed as part of the Time Telephone,
# an art project for Burning Man 2019.
#
# copyright 2019 æstrid smith

set -x

cd /home/asterisk/tapes
DURATION=0

FN1=/home/asterisk/_tape-prelim.au
OUTFILE=/home/asterisk/latest-tape.ulaw
NAMES=""

rawfmt="-t raw -e mu-law -r 8000 -b 8"

echo "debug info ==="
pwd
ls -1
echo '==='
for P in $( ls -1 | tac ) ; do
    CUR_LEN=$( echo $P | cut -d_ -f2 )
    echo "P is" $P
    echo "CUR_LEN is" $CUR_LEN
    echo "DURATION is" $DURATION

    if [ $DURATION -le 600 ] ; then
        NAMES="$P $NAMES"
    else
        break
    fi
    # 1/ will make `dc` round to an integer
    DURATION=$( echo $DURATION $CUR_LEN + 1/ p | dc )
done

# TODO: use `sox splice` instead of `cat` like some sort of heathen
cat $NAMES > $FN1

sox $rawfmt "$FN1" $rawfmt "$OUTFILE" reverse trim 0 10:00 reverse ||
    mv "$FN1" "$OUTFILE"
rm "$FN1"

