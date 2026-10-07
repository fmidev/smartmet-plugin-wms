#!/bin/bash
#
# Run the tests in several TestRunner.sh processes in parallel
#
# Usage: ParallelTestRunner.sh SHARDS [test files]
#
# Each shard starts a PluginTest of its own, so the cost of one more shard is
# one more server startup and its memory. The tests are dealt to the shards in
# turn so that slow groups of similar tests are spread evenly.
#
# The output of each test is printed as one block in the original test order
# once all the shards have finished, followed by a combined summary. A
# progress line is printed every 30 seconds meanwhile, which also keeps CI
# from aborting a long silent step. SHARDS=1 runs TestRunner.sh directly.

if [ $# -lt 1 ] || ! [[ "$1" =~ ^[1-9][0-9]*$ ]]; then
    echo "Usage: $0 SHARDS [test files]" >&2
    exit 1
fi

shards=$1
shift

if [ $# -gt 0 ]; then
    tests=("$@")
else
    tests=(input/*.get)
fi

if [ "$shards" -eq 1 ]; then
    exec ./TestRunner.sh "${tests[@]}"
fi

[ ${#tests[@]} -lt "$shards" ] && shards=${#tests[@]}

logdir=$(mktemp -d /tmp/wms-test-shards.XXXXXX)
pids=()

stop_shards()
{
    kill "${pids[@]}" 2>/dev/null
    wait
    rm -rf "$logdir"
    echo "Parallel test run interrupted"
    exit 1
}
trap stop_shards INT TERM

# Deal the tests to the shards in turn

declare -a shard_tests
for ((i = 0; i < ${#tests[@]}; i++)); do
    s=$((i % shards))
    shard_tests[$s]+="${tests[$i]}"$'\n'
done

echo "Running ${#tests[@]} tests in $shards parallel test processes"

rm -f failures/*

for ((s = 0; s < shards; s++)); do
    mapfile -t files <<< "${shard_tests[$s]%$'\n'}"
    # TestRunner.sh must not clear failures/ under the other shards
    KEEP_FAILURES=1 ./TestRunner.sh "${files[@]}" > "$logdir/$s.log" 2>&1 &
    pids+=($!)
done

# Report progress until all shards are done

count_done()
{
    cat "$logdir"/*.log 2>/dev/null | grep -acE '^[^[:space:]]+\.(get|post) \.+ '
}

while true; do
    running=0
    for pid in "${pids[@]}"; do
        kill -0 "$pid" 2>/dev/null && running=1
    done
    [ $running -eq 0 ] && break
    for ((t = 0; t < 30 && running; t++)); do
        sleep 1
        running=0
        for pid in "${pids[@]}"; do
            kill -0 "$pid" 2>/dev/null && running=1
        done
    done
    [ $running -eq 1 ] && echo "... $(count_done) of ${#tests[@]} tests done"
done

statuses=()
for pid in "${pids[@]}"; do
    wait "$pid"
    statuses+=($?)
done

# Merge the shard outputs into one report in the original test order

printf '%s\n' "${tests[@]}" > "$logdir/tests.txt"

perl - "$logdir" "$shards" "${statuses[@]}" <<'EOF'
use strict;
use warnings;

my ($dir, $shards, @status) = @ARGV;

open(my $tfh, '<', "$dir/tests.txt") or die "$dir/tests.txt: $!";
chomp(my @tests = <$tfh>);
close($tfh);

my %known = map { my $n = $_; $n =~ s{.*/}{}; ($n => 1) } @tests;

my (%block, %failed, @crashed);
my ($ntests, $nfailures) = (0, 0);

for my $s (0 .. $shards - 1)
{
    open(my $fh, '<', "$dir/$s.log") or die "$dir/$s.log: $!";
    my @lines = <$fh>;
    close($fh);

    my ($current, @preamble, $summary_seen);
    my $in_failed_list = 0;
    for my $line (@lines)
    {
        if ($line =~ /^(\S+\.(?:get|post)) \.+ / && $known{$1})
        {
            $current = $1;
            $block{$current} = $line;
            next;
        }
        if ($line =~ /^\*\*\* All (\d+) tests passed/)
        {
            $ntests += $1;
            $summary_seen = 1;
            $current = undef;
            next;
        }
        if ($line =~ /^\*\*\* (\d+) tests out of (\d+) failed/)
        {
            $nfailures += $1;
            $ntests += $2;
            $summary_seen = 1;
            $current = undef;
            next;
        }
        if ($summary_seen)
        {
            # The list of failed tests of the shard
            $failed{$1} = 1 if $line =~ /^\t(\S+)\s*$/ && $known{$1};
            next;
        }
        if (defined $current)
        {
            $block{$current} .= $line;
        }
        else
        {
            push(@preamble, $line);
        }
    }

    # The plugin startup messages are the same in every shard, show them once
    print @preamble if $s == 0;

    if (!$summary_seen)
    {
        push(@crashed, $s);
        print "\n*** Test process $s did not finish (exit status $status[$s]), its output:\n\n";
        print @lines;
        print "\n";
    }
}

for my $test (@tests)
{
    (my $name = $test) =~ s{.*/}{};
    next unless exists $block{$name};
    print $block{$name};
}

if (@crashed)
{
    print "\n*** Test processes " . join(", ", @crashed) . " did not finish\n";
    exit 1;
}

if ($nfailures == 0)
{
    print "\n*** All $ntests tests passed\n";
}
else
{
    print "\n*** $nfailures tests out of $ntests failed\n";
    my @failed = grep { exists $failed{$_} } map { (my $n = $_) =~ s{.*/}{}; $n } @tests;
    if (@failed && @failed < 32)
    {
        print "\nFailed tests:\n";
        print "\t$_\n" for @failed;
        print "\n";
    }
}
exit($nfailures > 255 ? 255 : $nfailures);
EOF
status=$?

rm -rf "$logdir"
exit $status
