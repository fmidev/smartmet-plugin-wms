#!/bin/bash

RESULT=$1
EXPECTED=$2

if [ -z $MAX_LINES ] ; then MAX_LINES=100; fi
if [ -z $CUT_LINES ] ; then CUT_LINES=80; fi

if [ -x /usr/bin/magick ] ; then
    CONVERT=/usr/bin/magick
else
    CONVERT=convert
fi

# Basename for the test

NAME=$(basename $(basename $1 .get) .post)

# Validate the arguments

if [[ ! -e "$RESULT" ]]; then
    echo -n -e "FAIL\t\t'$RESULT' is missing"
    exit 1
fi

if [[ ! -s "$RESULT" ]]; then
    echo -n -e "FAIL\t\t'$RESULT' is empty"
    exit 1
fi

# Establish the result image type

MIME=$(file --brief --mime-type $RESULT)

# What we want to compare, and the difference image for locating problems

EXPECTED_PNG="failures/${NAME}_expected.png"
RESULT_PNG="failures/${NAME}_result.png"
DIFFERENCE_PNG="failures/${NAME}_difference.png"

# Create result images before exiting of EXPECTED image is missing

if [[ "$MIME" == "text/html" || "$MIME" == "text/x-asm" ]]; then
    rsvg-convert -u -b white -f png -o $RESULT_PNG $RESULT
elif [[ "$MIME" == "application/pdf" ]]; then
    $CONVERT -quiet $RESULT $RESULT_PNG
elif [[ "$MIME" == "image/png" ]]; then
    cp $RESULT $RESULT_PNG
fi

# Check expected output exists after result image has been created

if [[ ! -e "$EXPECTED" ]]; then
    echo -n -e "FAIL\t\t'$EXPECTED' is missing"
    exit 1
fi

# Quick byte by byte comparison

cmp --quiet $RESULT $EXPECTED
if [[ $? -eq 0 ]]; then
    echo -n "OK"
    rm -f $RESULT $RESULT_PNG
    exit 0
fi

# Support alternative expected files with numbered suffixes for
# XML and JSON

case "$MIME" in
    application/xml | text/xml | application/json | text/json)
        for index in $(seq 1 9); do
            if test -e $EXPECTED.$index ; then
                if cmp $RESULT $EXPECTED.$index ; then
                    echo -n "OK"
                    rm -f $RESULT
                    exit 0
                fi
            fi
        done
        ;;
esac

# Handle XML failures

if [[ "$MIME" == "application/xml" || "$MIME" == "text/xml" ]]; then
    echo -n "FAIL: XML output differs: $RESULT <> $EXPECTED"
    echo " diff -U4 | head -n $MAX_LINES | cut -c 1-$CUT_LINES:"
    diff -U4 $EXPECTED $RESULT | head -n 100 | cut -c 1-$CUT_LINES
    exit 1
fi

# Handle JSON failures
if [[ "$MIME" == "application/json" || "$MIME" == "text/json" ]]; then
    echo -n "FAIL: JSON output differs: $RESULT <> $EXPECTED"
    echo " diff -U4 | head -n $MAX_LINES | cut -c 1-$CUT_LINES:"
    diff -U4 $EXPECTED $RESULT | head -n 100 | cut -c 1-$CUT_LINES
    exit 1
fi

# Handle text/plain failures

if [[ "$MIME" == "text/plain" ]]; then
    echo -n "FAIL: text output differs: $RESULT <> $EXPECTED"
    echo " diff -U4 | head -n $MAX_LINES | cut -c 1-$CUT_LINES:"
    diff -U4 $EXPECTED $RESULT | head -n 100 | cut -c 1-$CUT_LINES
    exit 1
fi

# Create the expected PNGs for comparisons.
# Note: sometimes 'file' reports text/x-asm instead of html due to dots in css class names

if [[ "$MIME" == "text/html" || "$MIME" == "text/x-asm" || "$MIME" == "image/svg" || "$MIME" == "image/svg+xml" ]]; then
    rsvg-convert -u -b white -f png -o $EXPECTED_PNG $EXPECTED
    rsvg-convert -u -b white -f png -o $RESULT_PNG $RESULT

elif [[ "$MIME" == "application/pdf" ]]; then
    $CONVERT -quiet $EXPECTED $EXPECTED_PNG
elif [[ "$MIME" == "image/png" ]]; then
    cp $EXPECTED $EXPECTED_PNG
else
    echo -n "FAIL: Unknown mime type '$MIME'"
    exit 1
fi

# Compare the images

DBZ=$((compare 2>&1 -metric PSNR $EXPECTED_PNG $RESULT_PNG /dev/null | head -1 | sed "-es/ dB//") || echo -n PNG COMPARISON FAILED && exit 1)

if test $(echo $DBZ | wc -w) -ge 2 ; then
    MATCHES="0"
    DBZ=$(echo $DBZ | sed -e 's:\s.*$::')
else
    MATCHES="inf"
fi

if ! echo -n "$DBZ" | grep -Eq '^(inf|[\+\-]?[0-9][0-9]*(\.[0-9]*)?)$' ; then
    echo -n -e "FAIL\t\t$DBZ"
    exit 1
elif [ "$DBZ" = "$MATCHES" ]; then
    echo -n -e "OK ~"
    rm -f $RESULT $RESULT_PNG $EXPECTED_PNG
    exit 0
elif [ $(echo "$DBZ >= 50" | bc) = 1 ]; then
    echo -n -e "OK\t\tPNSR = $DBZ dB"
    composite $EXPECTED_PNG $RESULT_PNG -compose DIFFERENCE png:- | \
	$CONVERT -quiet - -contrast-stretch 0 $DIFFERENCE_PNG
    exit 0
elif [ $(echo "$DBZ >= 20" | bc) = 1 ]; then
    echo -n -e "WARNING\t\tPNSR = $DBZ dB (< 50 dB)"
    composite $EXPECTED_PNG $RESULT_PNG -compose DIFFERENCE png:- | \
	$CONVERT -quiet - -contrast-stretch 0 $DIFFERENCE_PNG
    exit 0
else
    echo -n -e "FAIL\t\tPNSR = $DBZ (< 20 dB)"
    composite $EXPECTED_PNG $RESULT_PNG -compose DIFFERENCE png:- | \
	$CONVERT -quiet - -contrast-stretch 0 $DIFFERENCE_PNG
    exit 1
fi
