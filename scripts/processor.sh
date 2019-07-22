#/bin/bash

# post processor.  runs after hangup.  moves the recording into the
# right place to be worked on by the splicer.

# developed as part of the Time Telephone,
# an art project for Burning Man 2019.
#
# copyright 2019 æstrid smith

INFILE="/var/spool/asterisk/call-out.ulaw"
OUTDIR="/home/asterisk/tapes"
LENGTH=$( sox -t raw -e mu-law -r 8000 -b 8 "$INFILE" -n stat 2>&1 | 
              grep -i seconds | 
              cut -d: -f2 )
LAST_RECORDING=$( ls "$OUTDIR" | tail -n 1 | cut -d- -f1 )
OUTFILE=$(( LAST_RECORDING + 1 ))_"$LENGTH"_.au

mv "$INFILE" "$OUTFILE"-"$LENGTH".au

