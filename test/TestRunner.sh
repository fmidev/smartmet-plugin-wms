#!/bin/bash

# Delete old results

rm -f failures/*

# Start the test handler with named pipes

in=/tmp/test.in.$$
out=/tmp/test.out.$$

echo -e "Starting the plugin, please wait a moment until it has been initialized...\n"

rm -f $in $out
mkfifo $in $out

# Handle quitting this test runner when child process terminates
export TOP_PID=$$

function testerExit
{
    # reset terminal colour, the plugin init sequence modifies the terminal
    if [[ $CI == "" ]]; then
	tput sgr0
    fi
    echo ""
    echo "PluginTest has shutdown unexpectedly. A timeout for a single test may have occurred."

    # Remove the pipes
    rm -f $in $out
    exit 1
}

trap testerExit SIGUSR1

# Actually start the plugintest in background
function pluginTestRun
{
    # We also force the killing of the main test runner
    ./PluginTest
    ret=$?
    if [ "$ret" != "0" ] ; then
	# Kill this main script, pid is recorded in environment before fork
	# Use /bin/kill- internal shell kill doesn't work with signames
    sleep 5
	/bin/kill -s SIGUSR1 $TOP_PID
	exit $ret
    fi
    exit 0
}

pluginTestRun < $in > $out 2>$out &
export TESTER_PID=$!
exec 3> $in 4< $out

# Function to request shutdown for the Plugin if a signal has been thrown
function ClosePlugin
{
    # Let it die peacefully
    echo "quit" >&3
    sleep 1
    # Let it die forcefully
    test -d /proc/$TESTER_PID && kill $TESTER_PID
    # Remove the pipes
    rm -f $in $out
    # reset terminal colour, the plugin init sequence modifies the terminal
    if [[ $CI == "" ]]; then
	tput sgr0
    fi
    # return faulty status for interrupted test runner
    echo "PluginTest killed because of signal"
    exit 1
}

trap ClosePlugin 1 2 3 4 5 6 7 8 11 15

# Wait for the plugin to start

while true; do
    read line <&4
    if [[ "$line" == "OK" ]]; then
	break
    elif [[ "$line" != "" ]]; then
	echo "$line"
    fi
done

# Feed all the queries to the running plugin process one by one

dots='......................................................................'

ntests=0
nfailures=0
failed_tests=()
for f in input/*.get; do
    request_name=$(basename $f)
    name=$(basename $(basename $request_name .get) .post)

    result=failures/$request_name
    expected=output/$request_name

    # The plugin will echo DONE when it's done producing the result,
    # but it may print something else first, hence we start by
    # displaying the test message first

    printf "%s %s " $request_name "${dots:${#request_name}}"

    ignore=$(grep -xc $request_name input/.testignore)

    if [[ $ignore -eq 1 ]]; then
	echo "IGNORED"
    else
	start_time=$(date +%s.%3N)
	echo "$request_name" >&3
	extralines=""
	while true; do
	    IFS= read -r line <&4
	    if [[ "$line" == "DONE" ]]; then
		break
	    else
		# FOR DEBUGGING PluginTest CRASHES:
		# echo $line
		if [[ -z $extralines ]]; then
		    extralines="\t${line}"
		else
		    extralines+="\n\t"
		    extralines+=$line
		fi
	    fi
	done
	end_time=$(date +%s.%3N)
	elapsed=$(echo "scale=3; $end_time - $start_time" | bc)

	ntests=$((ntests+1))
	if [[ -z  "$result" ]]; then
	    nfailures=$((nfailures+1))
        failed_tests+=("$request_name")
	    echo -e "FAIL - NO OUTPUT\t$elapsed sec"
	else
	    if ! ./CompareImages.sh $result $expected ; then
	        nfailures=$((nfailures+1))
	        failed_tests+=("$request_name")
	    fi
	    echo -e "\t$elapsed sec"
	fi

	# Print extra messages collected during the test after the actual comparison
	if [[ ! -z "$extralines" ]]; then
	    echo -e "$extralines"
	fi
    fi

done

# Send quit request to plugin
echo "quit" >&3
rm -f $in $out

# Report final status

if [[ $nfailures -eq 0 ]]; then
    echo -e "\n*** All $ntests tests passed"
else
    echo -e "\n*** $nfailures tests out of $ntests failed"

    # List the failed tests if there are not too many
    if [ $nfailures -lt 32 ]; then
        echo -e ""
        echo -e "Failed tests:"
        for test in "${failed_tests[@]}"; do
            echo -e "\t$test"
        done
        echo -e ""
    fi
fi

exit $nfailures
