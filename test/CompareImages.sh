#!/bin/sh

RESULT=$1
EXPECTED=$2

# Basename for the test

NAME=$(basename $(basename $1 .get) .post)

# Validate the arguments

if [[ ! -e "$EXPECTED" ]]; then
    echo -e "FAIL\t\t'$EXPECTED' is missing"
    exit 1
fi

if [[ ! -e "$RESULT" ]]; then
    echo -e "FAIL\t\t'$RESULT' is missing"
    exit 1
fi

if [[ ! -s "$RESULT" ]]; then
    echo -e "FAIL\t\t'$RESULT' is empty"
    exit 1
fi

# Quick byte by byte comparison

cmp --quiet $RESULT $EXPECTED
if [[ $? -eq 0 ]]; then
    echo "OK"
    rm -f $RESULT
    exit 0
fi

# Establish the result image type

MIME=$(file --brief --mime-type $EXPECTED)

# What we want to compare, and the difference image for locating problems

EXPECTED_PNG="failures/${NAME}_expected.png"
RESULT_PNG="failures/${NAME}_result.png"
DIFFERENCE_PNG="failures/${NAME}_difference.png"

# Handle XML failures

if [[ "$MIME" == "application/xml" ]]; then
    echo "FAIL: XML output differs: $RESULT <> $EXPECTED"
    echo "diff $EXPECTED $RESULT | head -n 100 | cut -c 1-80"
    diff $EXPECTED $RESULT | head -n 100 | cut -c 1-80
    exit 1
fi

# Handle text/plain failures

if [[ "$MIME" == "text/plain" ]]; then
    echo "FAIL: text output differs: $RESULT <> $EXPECTED"
    echo "diff $EXPECTED $RESULT | head -n 100 | cut -c 1-80"
    diff $EXPECTED $RESULT | head -n 100 | cut -c 1-80
    exit 1
fi


# Create the PNGs for comparisons.
# Note: sometimes 'file' reports text/x-asm instead of html due to dots in css class names

if [[ "$MIME" == "text/html" || "$MIME" == "text/x-asm" ]]; then
    rsvg-convert -b white -f png -o $EXPECTED_PNG $EXPECTED
    rsvg-convert -b white -f png -o $RESULT_PNG $RESULT

elif [[ "$MIME" == "application/pdf" ]]; then
    convert $EXPECTED $EXPECTED_PNG
    convert $RESULT $RESULT_PNG 

elif [[ "$MIME" == "image/png" ]]; then
    cp $EXPECTED $EXPECTED_PNG
    cp $RESULT $RESULT_PNG
else
    echo "FAIL: Unknown mime type '$MIME'"
    exit 1
fi

# Compare the images

DBZ=$((compare 2>&1 -metric PSNR $EXPECTED_PNG $RESULT_PNG /dev/null | head -1 | sed "-es/ dB//") || echo PNG COMPARISON FAILED && exit 1)

if [ "$DBZ" = inf ]; then
    echo -e "OK\t\tPSNR = inf"
    rm -f $EXPECTED_PNG $RESULT_PNG $DIFFERENCE_PNG
    exit 0
elif [ $(echo "$DBZ >= 50" | bc) = 1 ]; then
    echo -e "OK\t\tPNSR = $DBZ dB"
    composite $EXPECTED_PNG $RESULT_PNG -compose DIFFERENCE png:- | \
	convert - -contrast-stretch 0 $DIFFERENCE_PNG
    exit 0
elif [ $(echo "$DBZ >= 20" | bc) = 1 ]; then
    echo -e "WARNING\t\tPNSR = $DBZ dB (< 50 dB)"
    composite $EXPECTED_PNG $RESULT_PNG -compose DIFFERENCE png:- | \
	convert - -contrast-stretch 0 $DIFFERENCE_PNG
    exit 0
else
    echo -e "FAIL\t\tPNSR = $DBZ (< 20 dB)"
    composite $EXPECTED_PNG $RESULT_PNG -compose DIFFERENCE png:- | \
	convert - -contrast-stretch 0 $DIFFERENCE_PNG
    exit 1
fi
