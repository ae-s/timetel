#!/bin/bash

# splicer.  runs after processor.  splices ten minutes worth of
# recordings into a new tape, adding generation loss as necessary.

# developed as part of the Time Telephone,
# an art project for Burning Man 2019.
#
# copyright 2019 æstrid smith

cd /home/asterisk/tapes
DURATION=0

FN1=_tape-prelim.au
OUTFILE=tape.au

for P in $( ls -1 | rev ) ; do
    CUR_LEN=$( echo $P | cut -d_ -f2 )
    DURATION=$((DURATION + CUR_LEN))
    if [ $DURATION -le 600 ] ; then
        echo $P
    else
        break
    fi
done | tac | xargs cat > "$FN1"
# TODO: use `sox splice` instead of `cat` like some sort of heathen

sox "$FN1" "$OUTFILE" trim -10:00
rm "$FN1"

