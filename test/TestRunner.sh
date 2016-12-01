#!/bin/sh

# Delete old results

rm -rf failures

# Start the test handler with named pipes

in=/tmp/test.in.$$
out=/tmp/test.out.$$

echo -e "Starting the plugin, please wait a moment until it has been initialized...\n"

rm -f $in $out
mkfifo $in $out

# Here we force output to be buffered one line at a time so we can display other messages while waiting for an OK

stdbuf -oL -e0 ./PluginTest < $in > $out 2>$out &
exec 3> $in 4< $out

# Function to request shutdown for the Plugin if a signal has been thrown
function ClosePlugin
{
    # Let it die peacefully
    echo "quit" >&3
    sleep 1
    # Let it die forcefully
    killall PluginTest
    # Remove the pipes
    rm -f $in $out
    # reset terminal colour, the plugin init sequence modifies the terminal
    tput sgr0
    exit
}

trap ClosePlugin 1 2 3 4 5 6 7 8 10 11 15

# Wait for the plugin to start

while true; do
    read line <&4
    if [[ "$line" == "OK" ]]; then
	break
    elif [[ "$line" != "" ]]; then
	echo $line
    fi
done

# Feed all the queries to the running plugin process one by one

dots='............................................................'

ntests=0
nfailures=0
for f in input/*.get; do
    request_name=$(basename $f)
    name=$(basename $(basename $request_name .get) .post)
    echo $request_name >&3

    result=failures/$request_name
    expected=output/$request_name
    
    # The plugin will echo DONE when it's done producing the result,
    # but it may print something else first, hence we start by
    # displaying the test message first

    printf "%s %s " $request_name "${dots:${#request_name}}"

    extralines=""
    while true; do
	read line <&4
	if [[ "$line" == "DONE" ]]; then
	    break
	else
	    if [[ -z $extralines ]]; then
		extralines="\t${line}"
	    else
		extralines="${extralines}\n\t$line"
	    fi
	fi
    done

    ntests=$((ntests+1))
    if [[ -z  "$result" ]]; then
	nfailures=$((nfailures+1))
	echo "FAIL - NO OUTPUT"
    else
	./CompareImages.sh $result $expected
	if [[ $? -ne 0 ]]; then
	    nfailures=$((nfailures+1))
	fi
    fi

    # Print extra messages collected during the test after the actual comparison
    if [[ ! -z "$extralines" ]]; then
	echo -e $extralines
    fi

done

# Send quit request to plugin
echo "quit" >&3
rm -f $in $out

# Report final status

if [[ $nfailures -eq 0 ]]; then
    echo -e "\n*** All $ntests tests passed"
else
    echo -e "\n*** $nfailures test(s) out of $ntests failed"
fi

exit $nfailures
