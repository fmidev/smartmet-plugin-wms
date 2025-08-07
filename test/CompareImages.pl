#! /usr/bin/perl

# CompareImages.pl - A script to compare WMS plugin test responses with expected outputs.

# Tried to use Image::Magick, but I did not get it working (ImageMagick-perl RPM).

use strict;
use warnings;
use File::Basename;
use File::Compare;
#use Image::Magick;
use File::Copy;

my $RESULT = $ARGV[0];
my $EXPECTED = $ARGV[1];

my $CONVERT = -e '/usr/bin/magick' ? '/usr/bin/magick' : '/usr/bin/convert';

sub ReadXmlFile {
    my ($file) = @_;
    open my $fh, "xmllint --format $file|" or die "Could not open '$file' $!";
    my @lines = <$fh>;
    close $fh;
    return @lines;
}

sub ReadJsonFile {
    my ($file) = @_;
    open my $fh, "jq --raw-output . $file |" or die "Could not open '$file' $!";
    my @lines = <$fh>;
    close $fh;
    return @lines;
}

sub CreateAnimatedGif {
    my ($gif_name, @png_files) = @_;
    my $cmd = "$CONVERT -delay 50 @png_files -set dispose previous $gif_name";
    system($cmd) == 0 or die "Failed to create animated GIF '$gif_name': $!";
}

if (!defined $RESULT || !defined $RESULT) {
    die "Usage: CompareImages.pl <result>\n";
}

if (!-e $RESULT) {
    die "Result file '$RESULT' does not exist.\n";
}

my $NAME = basename($RESULT, ('.get', '.post'));
my $MIME = `file --brief --mime-type $RESULT`;
chomp $MIME;

my $EXPECTED_PNG = "failures/${NAME}_expected.png";
my $RESULT_PNG = "failures/${NAME}_result.png";
my $DIFFERENCE_PNG = "failures/${NAME}_difference.png";

sub Cleanup {
    unlink $EXPECTED_PNG if -e $EXPECTED_PNG;
    unlink $RESULT_PNG if -e $RESULT_PNG;
    unlink $DIFFERENCE_PNG if -e $DIFFERENCE_PNG;
    unlink $RESULT if -e $RESULT;
}

# Create result images before exiting of EXPECTED image is missing
if ($MIME eq 'text/html' || $MIME eq 'text/x-asm')
{
    system("rsvg-convert -u -b white -f png -o $RESULT_PNG $RESULT") == 0
        or die "Failed to convert result $RESULT to PNG: $!";
}
elsif ($MIME eq 'application/pdf')
{
    my $cmd = "$CONVERT $RESULT $RESULT_PNG";
    system($cmd) == 0
        or die "$cmd: Failed to convert result $RESULT to PNG: $!";
}
elsif ($MIME eq 'image/png' || $MIME eq 'image/jpeg') {
    File::Copy::copy($RESULT, $RESULT_PNG)
        or die "Failed to copy result $RESULT to PNG: $!";
}

# Check expected output exists after result image has been created

if (!-e $EXPECTED) {
    die "Expected file '$EXPECTED' does not exist.\n";
}

# Compare images if both exist (initially exact comparison)

my $cmp = File::Compare::compare($RESULT, $EXPECTED);
if (!defined $cmp) {
    die "Failed to compare file images: $!";
}
elsif ($cmp == 0) {
    print "OK     ";
    Cleanup();
    exit(0);
}

#  We need special handling for XML (there could be small accepted differences)

if ($MIME eq 'application/xml' || $MIME eq 'text/xml')
{
    my @result_lines = ReadXmlFile $RESULT;
    my @expected_lines = ReadXmlFile $EXPECTED;

    if ($#result_lines == $#expected_lines) {
        for (my $i = 0; $i <= $#result_lines; $i++) {
            my $result_line = $result_lines[$i];
            my $expected_line = $expected_lines[$i];
            if ($result_line eq $expected_line) {
                next;  # Lines match, continue to next line
            }
            # Lines does not match - look for accepted differences
            my $result_width = -1;
            my $result_height = -1;
            my $expected_width = -1;
            my $expected_height = -1;
            if ($result_line =~ m/.*?<LegendURL\s+width="(\d+)"\s+height="(\d+)".*?>/)
            {
                $result_width = $1;
                $result_height = $2;
                if ($expected_line =~ m/.*?<LegendURL\s+width="(\d+)"\s*height="(\d+)".*?>/) {
                    $expected_width = $1;
                    $expected_height = $2;

                    if (abs($result_width - $expected_width) <= 3 &&
                        abs($result_height - $expected_height) <= 3)
                    {
                        next;  # Width and height differences are acceptable
                    }
                }
            }
            if ($result_line =~ m/^.*?<OnlineResource\s.*?;width=(\d+).*?;height=(\d+).*?>/)
            {
                $result_width = $1;
                $result_height = $2;
                if ($expected_line =~ m/^.*?<OnlineResource\s.*?;width=(\d+).*?;height=(\d+).*?>/)
                {
                    $expected_width = $1;
                    $expected_height = $2;

                    if (abs($result_width - $expected_width) <= 3 &&
                        abs($result_height - $expected_height) <= 3)
                    {
                        next;  # Width and height differences are acceptable
                    }
                }
            }

            print "FAIL: XML output differs: $RESULT <> $EXPECTED\n";
            system("diff -U4 $EXPECTED $RESULT | head -n 100 | cut -c 1-128");
            exit (1);
        }
        print "OK     ";
        Cleanup();
        exit(0);
    } else {
        print "FAIL\tXML output has different number of lines.";
        exit(1);
    }
}

# FIXME: handle JSON output similarly to XML
if ($MIME eq 'application/json' || $MIME eq 'text/json')
{
    my @result_lines = ReadJsonFile $RESULT;
    my @expected_lines = ReadJsonFile $EXPECTED;

    if ($#result_lines == $#expected_lines) {
        for (my $i = 0; $i <= $#result_lines; $i++) {
            my $result_line = $result_lines[$i];
            my $expected_line = $expected_lines[$i];
            $result_line =~ s/^\s*//;  # Remove leading whitespace
            $result_line =~ s/\s*$//;  # Remove trailing whitespace
            $expected_line =~ s/^\s*//;  # Remove leading whitespace
            $expected_line =~ s/\s*$//;  # Remove trailing whitespace
            if ($result_line eq $expected_line) {
                next;  # Lines match, continue to next line
            }
            # Lines does not match - look for accepted differences
            if ($result_line =~ m/"width"\s*:\s*(\d+)/gm)
            {
                my $result_width = $1;
                if ($expected_line =~ m/^\s*"width"\s*:\s*(\d+)/) {
                    my $expected_width = $1;

                    if (abs($result_width - $expected_width) <= 3)
                    {
                        next;  # Small width differences are acceptable
                    }
                }
            }
            if ($result_line =~ m/"xlink:href"/ && $expected_line =~ m/"xlink:href"/)  {
                if ($result_line =~ m/;width=(\d+)/)
                {
                    my $result_width = $1;
                    if ($expected_line =~ m/;width=(\d+)/)
                    {
                        my $expected_width = $1;

                        if (abs($result_width - $expected_width) <= 3)
                        {
                            next;  # Small width differences are acceptable
                        }
                    }
                }
            }

            print "FAIL: JSON output differs: $RESULT <> $EXPECTED\n";
            system("diff -U4 $EXPECTED $RESULT | head -n 100 | cut -c 1-1024");
            exit (1);
        }
        print "OK     ";
        Cleanup();
        exit(0);
    } else {
        print "FAIL\tXML output has different number of lines.";
        exit(1);
    }
}


# Create the expected PNGs for comparisons.
# Note: sometimes 'file' reports text/x-asm instead of html due to dots in css class names

if ("$MIME" eq "text/html" || "$MIME" eq "text/x-asm" || "$MIME" eq "image/svg" || "$MIME" eq "image/svg+xml" )
{
    system("rsvg-convert -u -b white -f png -o $EXPECTED_PNG $EXPECTED") == 0
        or die "Failed to convert expected $EXPECTED to PNG: $!";
    system("rsvg-convert -u -b white -f png -o $RESULT_PNG $RESULT") == 0
        or die "Failed to convert result $RESULT to PNG: $!";
}
elsif ($MIME eq 'application/pdf')
{
    my $cmd = "$CONVERT $EXPECTED $EXPECTED_PNG";
    system($cmd) == 0
        or die "$cmd: Failed to convert expected $EXPECTED to PNG: $!";
}
elsif ($MIME eq 'image/png' || $MIME eq 'image/jpeg')
{
    copy($EXPECTED, $EXPECTED_PNG);
}
else
{
    die "Unsupported MIME type '$MIME' for expected file conversion.\n";
}

# Compare the PNG images
my $difference_str = `smartimgdiff_psnr $RESULT_PNG $EXPECTED_PNG $DIFFERENCE_PNG`;
if ($? != 0) {
    die "smartimgdiff_psnr failed: $difference_str";
}

if ($difference_str =~ m/PSNR:\s+inf\s+dB/) {
    print "OK     ";
    Cleanup();
    exit(0);
}

$difference_str =~ m/PSNR:\s+((?:\d+\.\d+|inf))\s+dB/ or
    die "Failed to get PSNR value from smartimgdiff_psnr output: $difference_str";

my $dbz = $1;
if ($dbz >= 50)
{
    print "OK       PSNR = $dbz dB";
    Cleanup();
    exit(0);
}
else
{
    if ($dbz >= 20)
    {
        print "WARNING  PSNR = $dbz dB (< 50 dB)";
        CreateAnimatedGif("failures/${NAME}.gif", $EXPECTED_PNG, $RESULT_PNG);
        exit (0);
    }
else
    {
        print "FAIL     PSNR = $dbz dB (< 20 dB)";
        exit (1);
    }
}
