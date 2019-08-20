#/bin/bash

# post processor.  runs after hangup.  moves the recording into the
# right place to be worked on by the splicer.

# developed as part of the Time Telephone,
# an art project for Burning Man 2019.
#
# copyright 2019 æstrid smith

set -x

rawfmt="-t raw -e mu-law -r 8000 -b 8"

INFILE="/var/spool/asterisk/monitor/call-in.ulaw"
OUTDIR="/home/asterisk/tapes"
LENGTH=$( sox $rawfmt "$INFILE" -n stat 2>&1 | 
              grep -i seconds | 
              cut -d: -f2 |
	      sed -e 's/[^0-9.]//g'
      )
LAST_RECORDING=$( ls "$OUTDIR" | grep '^[0-9][0-9][0-9][0-9]_' | tail -n 1 | cut -d_ -f1 | sed -e 's/^0*//' )
OUTNR=$( printf "%04d" $(( LAST_RECORDING + 1)) )
OUTFILE="$OUTNR"_"$LENGTH"_.au

mv "$INFILE" "$OUTDIR"/"$OUTFILE"
rm -f "/var/spool/asterisk/monitor/call-in.ulaw"

