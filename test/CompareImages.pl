#! /usr/bin/perl

# CompareImages.pl - A script to compare WMS plugin test responses with expected outputs.

# Tried to use Image::Magick, but I did not get it working (ImageMagick-perl RPM).

use strict;
use warnings;
use File::Basename;
use File::Compare;
use Image::Magick;
use File::Copy;

my $RESULT = $ARGV[0];
my $EXPECTED = $ARGV[1];

my $CONVERT = -e '/usr/bin/magick' ? '/usr/bin/magick' : '/usr/bin/convert';
my $GDALINFO = -e '/usr/gdal312/bin/gdalinfo' ? '/usr/gdal312/bin/gdalinfo' : 'gdalinfo';

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

sub CreateWebp {
    my ($webp_name, @png_files) = @_;
    return unless @png_files;
    my $image = Image::Magick->new;
    map { $image->Read($_) } @png_files;
    my $width = $image->[0]->Get('width');
    my $height = $image->[0]->Get('height');
    if ($width > 16383 || $height > 16383) {
        # WebP has a limit of 16383x16383 pixels. Crop image if necessary
        $width = 16383 if $width > 16383;
        $height = 16383 if $height > 16383;
        $image->Crop(geometry => "${width}x${height}");
    }
    $image->Set(magick => 'WebP');
    $image->Set(quality => 95);
    $image->Set(delay => 50);
    $image->Set(dispose => 'background');
    my $status = $image->Write($webp_name);
    if ($status) {
        die "Failed to create WEBP file '$webp_name': $status";
    }
}

# Run git diff with the histogram algorithm: it anchors on unique lines
# (e.g. <Name> values), so a layer inserted into capabilities shows up as
# one inserted block rather than smearing across neighbours the way Myers
# diff (GNU diff default) does. Strips the diff --git / index / --- / +++
# header lines because the temp file names they expose are noise.
# Convert a WebP file to a single PNG for the PSNR comparison.
#
# Animated WebP decodes to a multi-frame image sequence, and the frames are
# stored as delta regions (e.g. 500x448+0+26), not full canvases. Writing such
# a sequence straight to a .png makes ImageMagick emit numbered files
# (_result-0.png, _result-1.png, ...) and never create the plain _result.png
# the comparison expects -- the symptom seen on RHEL8. So we coalesce the delta
# frames into full canvases and stack them vertically into one PNG, which lets
# the existing single-file smartimagediff_psnr path compare every frame at once.
# Single-frame WebP is written directly, preserving the previous behaviour.
sub WebpToPng {
    my ($src, $dst) = @_;
    my $image = Image::Magick->new;
    my $status = $image->Read($src);
    if (defined $status and $status >= 400) { # Check for error status, ignore warnings
        die "Failed to read WEBP file '$src': $status";
    }
    my $out = $image;
    if (defined $image->[1]) {
        my $coalesced = $image->Coalesce();
        $out = $coalesced->Append(stack => 'true');
    }
    $out->Set(magick => 'PNG');
    $out->Set(depth => 8);
    $status = $out->Write($dst);
    if (defined $status && $status >= 400) { # Check for error status, ignore warnings
        die "Failed to write PNG file '$dst': $status";
    }
}

sub GitHistogramDiff {
    my ($file1, $file2) = @_;
    my $raw = `git diff --no-index --no-color --diff-algorithm=histogram -U4 -- "$file1" "$file2" 2>/dev/null`;
    my @out;
    for my $line (split /\n/, $raw, -1) {
        next if $line =~ m{^diff --git };
        next if $line =~ m{^index [0-9a-f]+\.\.[0-9a-f]+};
        next if $line =~ m{^--- (a/|/dev/null)};
        next if $line =~ m{^\+\+\+ (b/|/dev/null)};
        push @out, $line;
    }
    return join("\n", @out);
}

sub TrimDiff {
    my ($text, $max_lines, $max_cols) = @_;
    my @lines = split /\n/, $text, -1;
    my $truncated = (scalar(@lines) > $max_lines) ? 1 : 0;
    @lines = @lines[0..$max_lines-1] if $truncated;
    @lines = map { length($_) > $max_cols ? substr($_, 0, $max_cols) : $_ } @lines;
    my $result = join("\n", @lines);
    $result .= "\n... (output truncated)\n" if $truncated;
    return $result;
}

sub WriteTempLines {
    my ($prefix, $lines_ref) = @_;
    my $path = sprintf("/tmp/%s_%d_%d", $prefix, $$, int(rand(1_000_000)));
    open my $fh, '>', $path or die "Cannot write '$path': $!";
    print $fh @$lines_ref;
    close $fh;
    return $path;
}

# Compare two arrays of lines using the same fuzz rules previously applied
# only on the equal-line-count path (LegendURL / OnlineResource width/height
# differences up to 3 are tolerated). Returns 1 if equal within fuzz, else 0.
sub LineSequenceMatches {
    my ($exp_ref, $res_ref) = @_;
    return 0 if scalar(@$exp_ref) != scalar(@$res_ref);
    for (my $i = 0; $i <= $#{$exp_ref}; $i++) {
        my $e = $exp_ref->[$i];
        my $r = $res_ref->[$i];
        next if $e eq $r;
        if ($r =~ m/.*?<LegendURL\s+width="(\d+)"\s+height="(\d+)".*?>/) {
            my ($rw, $rh) = ($1, $2);
            if ($e =~ m/.*?<LegendURL\s+width="(\d+)"\s*height="(\d+)".*?>/) {
                my ($ew, $eh) = ($1, $2);
                my $wd = abs($rw - $ew);
                my $hd = abs($rh - $eh);
                next if (($wd > 0 && $wd <= 3) || ($hd > 0 && $hd <= 3));
            }
        }
        if ($r =~ m{^.*?<OnlineResource\s.*?;width=(\d+).*?;height=(\d+).*?>}) {
            my ($rw, $rh) = ($1, $2);
            if ($e =~ m{^.*?<OnlineResource\s.*?;width=(\d+).*?;height=(\d+).*?>}) {
                my ($ew, $eh) = ($1, $2);
                my $wd = abs($rw - $ew);
                my $hd = abs($rh - $eh);
                next if (($wd > 0 && $wd <= 3) || ($hd > 0 && $hd <= 3));
            }
        }
        if ($r =~ m/<LegendURL\s.*?width=(\d+).*?height=(\d+)/) {
            my ($rw, $rh) = ($1, $2);
            if ($e =~ m/<LegendURL\s.*?width=(\d+).*?height=(\d+)/) {
                my ($ew, $eh) = ($1, $2);
                my $wd = abs($rw - $ew);
                my $hd = abs($rh - $eh);
                next if (($wd > 0 && $wd <= 3) || ($hd > 0 && $hd <= 3));
            }
        }
        return 0;
    }
    return 1;
}

# Split xmllint-formatted capabilities output into:
#   header:   lines up to and including the root <Layer> container's open tag
#             plus any non-Layer children (e.g. <Title>) before the first child
#   blocks:   { Name => arrayref of lines } for each direct child <Layer>
#   order:    [ Name, ... ] preserving the original order
#   footer:   the root container's </Layer> close and everything after
# Key for each block is the first <Name>...</Name> inside it (the outermost
# layer name); nested layers stay inside the parent block. Documents with no
# <Layer> at all yield empty blocks/order and put every line into header.
sub SegmentCapabilities {
    my @lines = @_;
    my (@header, @footer, %blocks, @order);

    my $i = 0;
    while ($i < @lines && $lines[$i] !~ m{^\s*<Layer[\s>]}) {
        push @header, $lines[$i++];
    }
    if ($i >= @lines) {
        return (\@header, \%blocks, \@order, \@footer);
    }
    push @header, $lines[$i++];   # root <Layer ...> open

    while ($i < @lines) {
        my $line = $lines[$i];
        if ($line =~ m{^\s*<Layer\b} && $line !~ m{/>\s*$}) {
            my $start = $i;
            my $depth = 1;
            $i++;
            while ($i < @lines && $depth > 0) {
                my $l = $lines[$i];
                if ($l =~ m{^\s*<Layer\b} && $l !~ m{/>\s*$}) { $depth++; }
                elsif ($l =~ m{^\s*</Layer>})                  { $depth--; }
                $i++;
            }
            my @block = @lines[$start..$i-1];
            my $name;
            for my $bl (@block) {
                if ($bl =~ m{<Name>(.+?)</Name>}) { $name = $1; last; }
            }
            $name //= "(unnamed-layer-at-line-" . ($start + 1) . ")";
            if (exists $blocks{$name}) {
                $name .= "#" . scalar(@order);
            }
            $blocks{$name} = \@block;
            push @order, $name;
        }
        elsif ($line =~ m{^\s*</Layer>}) {
            push @footer, $line;
            $i++;
            last;
        }
        else {
            push @header, $line;
            $i++;
        }
    }
    while ($i < @lines) {
        push @footer, $lines[$i++];
    }
    return (\@header, \%blocks, \@order, \@footer);
}

sub DiffSection {
    my ($label, $exp_ref, $res_ref, $matcher) = @_;
    $matcher //= \&LineSequenceMatches;
    return '' if $matcher->($exp_ref, $res_ref);
    my $ep = WriteTempLines("cmp_exp", $exp_ref);
    my $rp = WriteTempLines("cmp_res", $res_ref);
    my $d = GitHistogramDiff($ep, $rp);
    unlink $ep, $rp;
    return "[$label differs]\n" . TrimDiff($d, 60, 128) . "\n";
}

# JSON counterpart to LineSequenceMatches. Preserves the original JSON-path
# behaviour: trim leading/trailing whitespace before comparing, and tolerate
# "width": N differences of up to 3 (both direct and embedded in xlink:href
# query strings).
sub LineSequenceMatchesJson {
    my ($exp_ref, $res_ref) = @_;
    return 0 if scalar(@$exp_ref) != scalar(@$res_ref);
    for (my $i = 0; $i <= $#{$exp_ref}; $i++) {
        my $e = $exp_ref->[$i];
        my $r = $res_ref->[$i];
        my $et = $e; $et =~ s/^\s+//; $et =~ s/\s+$//;
        my $rt = $r; $rt =~ s/^\s+//; $rt =~ s/\s+$//;
        next if $et eq $rt;
        if ($rt =~ m/"width"\s*:\s*(\d+)/) {
            my $rw = $1;
            if ($et =~ m/"width"\s*:\s*(\d+)/) {
                my $ew = $1;
                next if abs($rw - $ew) <= 3;
            }
        }
        if ($rt =~ m/"xlink:href"/ && $et =~ m/"xlink:href"/) {
            if ($rt =~ m/;width=(\d+)/ && $et =~ m/;width=(\d+)/) {
                my $rw = ($rt =~ m/;width=(\d+)/)[0];
                my $ew = ($et =~ m/;width=(\d+)/)[0];
                next if abs($rw - $ew) <= 3;
            }
        }
        return 0;
    }
    return 1;
}

# JSON counterpart to SegmentCapabilities. jq --raw-output emits the inner
# WMS layers array at indent 4 ("    \"Layer\": ["), each layer object opens
# with "      {" and closes with "      }," or "      }" at indent 6, and
# the array itself closes with "    ]". Anything that doesn't match the
# capabilities structure leaves header populated and blocks empty.
sub SegmentCapabilitiesJson {
    my @lines = @_;
    my (@header, @footer, %blocks, @order);

    my $i = 0;
    while ($i < @lines && $lines[$i] !~ m{^    "Layer":\s*\[\s*$}) {
        push @header, $lines[$i++];
    }
    if ($i >= @lines) {
        return (\@header, \%blocks, \@order, \@footer);
    }
    push @header, $lines[$i++];   # the '    "Layer": [' line

    while ($i < @lines) {
        my $line = $lines[$i];
        if ($line =~ m{^      \{\s*$}) {
            my $start = $i;
            $i++;
            while ($i < @lines && $lines[$i] !~ m{^      \},?\s*$}) {
                $i++;
            }
            if ($i < @lines) {
                $i++;  # include the close line in the block
            }
            my @block = @lines[$start..$i-1];
            my $name;
            for my $bl (@block) {
                if ($bl =~ m{^        "Name":\s*"([^"]+)"}) { $name = $1; last; }
            }
            $name //= "(unnamed-layer-at-line-" . ($start + 1) . ")";
            if (exists $blocks{$name}) {
                $name .= "#" . scalar(@order);
            }
            $blocks{$name} = \@block;
            push @order, $name;
        }
        elsif ($line =~ m{^    \]}) {
            push @footer, $line;
            $i++;
            last;
        }
        else {
            push @header, $line;
            $i++;
        }
    }
    while ($i < @lines) {
        push @footer, $lines[$i++];
    }
    return (\@header, \%blocks, \@order, \@footer);
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

# Detect if text/html is actual HTML (GetFeatureInfo output) vs SVG misdetected as text/html
my $is_plain_html = 0;
if ($MIME eq 'text/html' || $MIME eq 'text/x-asm') {
    open my $fh, '<', $RESULT or die "Cannot open '$RESULT': $!";
    my $first_line = <$fh>;
    close $fh;
    $is_plain_html = 1 if defined $first_line && $first_line =~ /^<html/i;
}

my $EXPECTED_PNG = "failures/${NAME}_expected.png";
my $RESULT_PNG = "failures/${NAME}_result.png";
my $DIFFERENCE_PNG = "failures/${NAME}_difference.png";

my $rsvg_params = "--unlimited --width=1024 --keep-aspect-ratio --background-color=white --format=png";

sub Cleanup {
    # On a clean pass remove the raw result the harness wrote into failures/
    # too, so a passing test leaves nothing behind to wade through.
    unlink $RESULT if -e $RESULT;
    unlink $EXPECTED_PNG if -e $EXPECTED_PNG;
    unlink $RESULT_PNG if -e $RESULT_PNG;
    unlink $DIFFERENCE_PNG if -e $DIFFERENCE_PNG;
}

# Mapbox Vector Tile: decode with protoc and compare as text proto.
# The raw binary differs between runs due to map ordering, but the decoded
# text form is stable for identical input data.
if ($MIME eq 'application/vnd.mapbox-vector-tile' || $MIME eq 'application/octet-stream')
{
    # Try to detect MVT by file extension or by checking if the expected output
    # is a text file (protoc-decoded form) while the result is binary.
    my $is_mvt = ($MIME eq 'application/vnd.mapbox-vector-tile')
              || ($RESULT =~ /\.(mvt|pbf)$/)
              || ($MIME eq 'application/octet-stream' && -e $EXPECTED && -T $EXPECTED);
    if ($is_mvt)
    {
        my $PROTO_DIR  = '../wms';
        my $PROTO_FILE = "$PROTO_DIR/vector_tile.proto";
        my $decoded_result = `protoc --decode=vector_tile.Tile --proto_path=$PROTO_DIR $PROTO_FILE < $RESULT 2>/dev/null`;
        open my $efh, '<', $EXPECTED or die "Cannot open '$EXPECTED': $!";
        my $decoded_expected = do { local $/; <$efh> };
        close $efh;

        if ($decoded_result eq $decoded_expected) {
            print "OK     ";
            Cleanup();
            exit(0);
        }

        # Show diff on failure
        my $tmpfile = "/tmp/mvt_result_$$.txt";
        open my $tmpfh, '>', $tmpfile or die "Cannot write '$tmpfile': $!";
        print $tmpfh $decoded_result;
        close $tmpfh;

        print "FAIL: MVT output differs: $RESULT <> $EXPECTED\n";
        print TrimDiff(GitHistogramDiff($EXPECTED, $tmpfile), 100, 128);
        unlink $tmpfile;
        exit(1);
    }
}

# GeoTiff: compare metadata via gdalinfo (binary TIFF bodies differ due to timestamps,
# so we extract human-readable metadata and compare that as text instead).
if ($MIME eq 'image/tiff')
{
    my $meta_result = `$GDALINFO -nofl -nomd -stats $RESULT 2>/dev/null`;
    open my $efh, '<', $EXPECTED or die "Cannot open '$EXPECTED': $!";
    my $meta_expected = do { local $/; <$efh> };
    close $efh;

    if ($meta_result eq $meta_expected) {
        print "OK     ";
        Cleanup();
        exit(0);
    }

    # Show diff on failure
    my $tmpfile = "/tmp/geotiff_result_meta_$$.txt";
    open my $tmpfh, '>', $tmpfile or die "Cannot write '$tmpfile': $!";
    print $tmpfh $meta_result;
    close $tmpfh;

    print "FAIL: GeoTiff metadata differs: $RESULT <> $EXPECTED\n";
    print TrimDiff(GitHistogramDiff($EXPECTED, $tmpfile), 100, 128);
    unlink $tmpfile;
    exit(1);
}

# Create result images before exiting of EXPECTED image is missing
if (($MIME eq 'text/html' || $MIME eq 'text/x-asm') && !$is_plain_html)
{
    system("rsvg-convert $rsvg_params -o $RESULT_PNG $RESULT") == 0
        or die "Failed to convert result $RESULT to PNG: $!";
}
elsif ($MIME eq 'application/pdf')
{
    my $image = Image::Magick->new;
    my $status = $image->Read($RESULT);
    if (defined $status and $status >= 400) { # Check for error status, ignore warnings
        die "Failed to read PDF file '$RESULT': $status";
    }
    $image->Set(magick => 'PNG');
    $image->Set(depth => 8);  # Set depth to 8 bits per channel
    $image->Set(ColorSpace => 'RGB');
    # Drop the RGB ICC profile that ghostscript embeds when rasterising the
    # PDF; otherwise smartimagediff_psnr inherits it for the difference PNG
    # and libpng rejects an RGB profile on grayscale-encoded output (some
    # builds escalate the warning to a fatal write failure).
    $image->Strip();
    $image->Write($RESULT_PNG); # Write the image to PNG format. Discard errors if any.
}
elsif ($MIME eq 'image/png' || $MIME eq 'image/jpeg') {
    File::Copy::copy($RESULT, $RESULT_PNG)
        or die "Failed to copy result $RESULT to PNG: $!";
}
elsif ($MIME eq 'image/webp') {
    WebpToPng($RESULT, $RESULT_PNG);
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

# Plain HTML (e.g. GetFeatureInfo text/html output) - line-by-line text comparison
if ($is_plain_html)
{
    open my $rfh, '<', $RESULT   or die "Cannot open '$RESULT': $!";
    open my $efh, '<', $EXPECTED or die "Cannot open '$EXPECTED': $!";
    my @result_lines   = <$rfh>;
    my @expected_lines = <$efh>;
    close $rfh;
    close $efh;

    my $match = (scalar(@result_lines) == scalar(@expected_lines));
    if ($match) {
        for my $i (0..$#result_lines) {
            (my $r = $result_lines[$i]) =~ s/\s+$//;
            (my $e = $expected_lines[$i]) =~ s/\s+$//;
            if ($r ne $e) { $match = 0; last; }
        }
    }
    if ($match) {
        print "OK     ";
        Cleanup();
        exit(0);
    }
    print "FAIL: HTML output differs: $RESULT <> $EXPECTED\n";
    print TrimDiff(GitHistogramDiff($EXPECTED, $RESULT), 100, 128);
    exit(1);
}

#  We need special handling for XML (there could be small accepted differences).
#  Capabilities documents are segmented by <Layer><Name> so that adding or
#  removing a layer reports as a one-line summary instead of cascading through
#  every following layer's bounding boxes and styles.

if ($MIME eq 'application/xml' || $MIME eq 'text/xml')
{
    my @result_lines   = ReadXmlFile $RESULT;
    my @expected_lines = ReadXmlFile $EXPECTED;

    my ($exp_header, $exp_blocks, $exp_order, $exp_footer) = SegmentCapabilities(@expected_lines);
    my ($res_header, $res_blocks, $res_order, $res_footer) = SegmentCapabilities(@result_lines);

    my $has_layers = (scalar(@$exp_order) || scalar(@$res_order));

    if (!$has_layers) {
        # Not a capabilities-style document — whole-file fuzz compare.
        if (LineSequenceMatches(\@expected_lines, \@result_lines)) {
            print "OK     ";
            Cleanup();
            exit(0);
        }
        print "FAIL: XML output differs: $RESULT <> $EXPECTED\n";
        print TrimDiff(GitHistogramDiff($EXPECTED, $RESULT), 100, 128);
        exit(1);
    }

    my @report;
    my $section = DiffSection("Capabilities header", $exp_header, $res_header);
    push @report, $section if $section ne '';

    my %exp_set = map { $_ => 1 } @$exp_order;
    my %res_set = map { $_ => 1 } @$res_order;
    my @added   = grep { !$exp_set{$_} } @$res_order;
    my @removed = grep { !$res_set{$_} } @$exp_order;
    if (@removed) {
        push @report, join("", map { "- Removed layer '$_'\n" } @removed);
    }
    if (@added) {
        push @report, join("", map { "+ Added layer '$_'\n" } @added);
    }

    # Layer order matters in WMS (layers are emitted alphabetically); flag a
    # reorder of the common set so a stale expected file gets caught instead of
    # silently passing when only positions shift.
    my @common_exp = grep { $res_set{$_} } @$exp_order;
    my @common_res = grep { $exp_set{$_} } @$res_order;
    if (join("\0", @common_exp) ne join("\0", @common_res)) {
        my @exp_lines = map { "$_\n" } @common_exp;
        my @res_lines = map { "$_\n" } @common_res;
        my $ep = WriteTempLines("order_exp", \@exp_lines);
        my $rp = WriteTempLines("order_res", \@res_lines);
        my $d = GitHistogramDiff($ep, $rp);
        unlink $ep, $rp;
        push @report, "[Layer order changed]\n" . TrimDiff($d, 40, 128) . "\n";
    }

    for my $name (@$exp_order) {
        next unless exists $res_blocks->{$name};
        my $sec = DiffSection("Layer '$name'", $exp_blocks->{$name}, $res_blocks->{$name});
        push @report, $sec if $sec ne '';
    }

    $section = DiffSection("Capabilities footer", $exp_footer, $res_footer);
    push @report, $section if $section ne '';

    if (!@report) {
        print "OK     ";
        Cleanup();
        exit(0);
    }

    print "FAIL: XML output differs: $RESULT <> $EXPECTED\n";
    print TrimDiff(join("\n", @report), 200, 128);
    exit(1);
}

# JSON GetCapabilities is segmented the same way as the XML branch: by
# <Layer><Name>, with added/removed layers reported as one-line entries, a
# common-set reorder flagged on its own, and per-layer content diffs
# localised so a change inside one layer doesn't smear into its neighbours.
# RHEL8 file command returns text/plain for JSON, so accept that too.
if ($MIME eq 'application/json' || $MIME eq 'text/json' || $MIME eq 'text/plain')
{
    my @result_lines   = ReadJsonFile $RESULT;
    my @expected_lines = ReadJsonFile $EXPECTED;

    my ($exp_header, $exp_blocks, $exp_order, $exp_footer) = SegmentCapabilitiesJson(@expected_lines);
    my ($res_header, $res_blocks, $res_order, $res_footer) = SegmentCapabilitiesJson(@result_lines);

    my $has_layers = (scalar(@$exp_order) || scalar(@$res_order));

    if (!$has_layers) {
        if (LineSequenceMatchesJson(\@expected_lines, \@result_lines)) {
            print "OK     ";
            Cleanup();
            exit(0);
        }
        print "FAIL: JSON output differs: $RESULT <> $EXPECTED\n";
        print TrimDiff(GitHistogramDiff($EXPECTED, $RESULT), 100, 1024);
        exit(1);
    }

    my @report;
    my $section = DiffSection("Capabilities header", $exp_header, $res_header, \&LineSequenceMatchesJson);
    push @report, $section if $section ne '';

    my %exp_set = map { $_ => 1 } @$exp_order;
    my %res_set = map { $_ => 1 } @$res_order;
    my @added   = grep { !$exp_set{$_} } @$res_order;
    my @removed = grep { !$res_set{$_} } @$exp_order;
    if (@removed) {
        push @report, join("", map { "- Removed layer '$_'\n" } @removed);
    }
    if (@added) {
        push @report, join("", map { "+ Added layer '$_'\n" } @added);
    }

    my @common_exp = grep { $res_set{$_} } @$exp_order;
    my @common_res = grep { $exp_set{$_} } @$res_order;
    if (join("\0", @common_exp) ne join("\0", @common_res)) {
        my @exp_ord_lines = map { "$_\n" } @common_exp;
        my @res_ord_lines = map { "$_\n" } @common_res;
        my $ep = WriteTempLines("order_exp", \@exp_ord_lines);
        my $rp = WriteTempLines("order_res", \@res_ord_lines);
        my $d = GitHistogramDiff($ep, $rp);
        unlink $ep, $rp;
        push @report, "[Layer order changed]\n" . TrimDiff($d, 40, 128) . "\n";
    }

    for my $name (@$exp_order) {
        next unless exists $res_blocks->{$name};
        my $sec = DiffSection("Layer '$name'", $exp_blocks->{$name}, $res_blocks->{$name}, \&LineSequenceMatchesJson);
        push @report, $sec if $sec ne '';
    }

    $section = DiffSection("Capabilities footer", $exp_footer, $res_footer, \&LineSequenceMatchesJson);
    push @report, $section if $section ne '';

    if (!@report) {
        print "OK     ";
        Cleanup();
        exit(0);
    }

    print "FAIL: JSON output differs: $RESULT <> $EXPECTED\n";
    print TrimDiff(join("\n", @report), 200, 1024);
    exit(1);
}


# Create the expected PNGs for comparisons.
# Note: sometimes 'file' reports text/x-asm instead of html due to dots in css class names

if ("$MIME" eq "text/html" || "$MIME" eq "text/x-asm" || "$MIME" eq "image/svg" || "$MIME" eq "image/svg+xml" )
{
    system("rsvg-convert $rsvg_params -o $EXPECTED_PNG $EXPECTED") == 0
        or die "Failed to convert expected $EXPECTED to PNG: $!";
    system("rsvg-convert $rsvg_params -o $RESULT_PNG $RESULT") == 0
        or die "Failed to convert result $RESULT to PNG: $!";
}
elsif ($MIME eq 'application/pdf')
{
    my $image = Image::Magick->new;
    my $status = $image->Read($EXPECTED);
    if (defined $status and $status >= 400) { # Check for error status, ignore warnings
        die "Failed to read PDF file '$RESULT': $status";
    }
    $image->Set(magick => 'PNG');
    $image->Set(depth => 8);  # Set depth to 8 bits per channel
    $image->Set(ColorSpace => 'RGB');
    $image->Strip();   # see matching comment in the RESULT_PNG branch above
    $status = $image->Write($EXPECTED_PNG); # Write the image to PNG format. Discard errors if any.
    if (defined $status && $status >= 400) { # Check for error status, ignore warnings
        die "Failed to write PNG file '$EXPECTED_PNG': $status";
    }
}
elsif ($MIME eq 'image/png' || $MIME eq 'image/jpeg')
{
    copy($EXPECTED, $EXPECTED_PNG);
}
elsif ($MIME eq 'image/webp')
{
    WebpToPng($EXPECTED, $EXPECTED_PNG);
}
else
{
    die "Unsupported MIME type '$MIME' for expected file conversion.\n";
}

# Compare the PNG images.
#
# Transition design: smartimagediff_psnr stays the AUTHORITATIVE gate -- it
# alone sets the exit code, preserving today's OK / WARNING / FAIL behaviour.
# The structural, anti-aliasing-aware comparator (smartimagediff) runs
# alongside in SHADOW mode: its verdict is appended to every status line as
# "[sid:...]" and it writes the red-boxed overlay, but it does not yet change
# pass/fail. Cases where the two verdicts disagree are flagged with a loud
# "SHADOW-DISAGREE" marker so a real test run can be grepped for them. Once the
# verdicts have been compared, smartimagediff becomes the gate and PSNR is
# retired. See docs/structural-image-diff.md in smartmet-library-regression.

my $difference_str = `smartimagediff_psnr $RESULT_PNG $EXPECTED_PNG $DIFFERENCE_PNG`;
if ($? != 0) {
    die "smartimagediff_psnr failed: $difference_str";
}

$difference_str =~ m/PSNR:\s+((?:\d+\.\d+|inf))\s+dB/ or
    die "Failed to get PSNR value from smartimagediff_psnr output: $difference_str";
my $dbz = $1;

# PSNR verdict (authoritative during the transition).
my ($psnr_label, $psnr_pass);
if ($dbz eq 'inf' || $dbz >= 50) { $psnr_label = 'OK';      $psnr_pass = 1; }
elsif ($dbz >= 20)               { $psnr_label = 'WARNING'; $psnr_pass = 1; }
else                             { $psnr_label = 'FAIL';    $psnr_pass = 0; }

# Structural comparator in shadow mode. Skipped cleanly when the binary is not
# installed, so this script keeps working against an older library package.
my $OVERLAY_PNG = "failures/${NAME}_overlay.png";
my ($sid_label, $sid_out, $sid_pass) = ('SKIP', '', $psnr_pass);
if (system("command -v smartimagediff >/dev/null 2>&1") == 0) {
    $sid_out = `smartimagediff $RESULT_PNG $EXPECTED_PNG $OVERLAY_PNG 2>&1`;
    my $sid_rc = $? >> 8;          # 0 = OK, 1 = regression, 2 = error
    chomp $sid_out;
    $sid_label = $sid_rc == 0 ? 'OK' : ($sid_rc == 1 ? 'FAIL' : 'ERROR');
    $sid_pass  = ($sid_rc == 0);
}
# A disagreement worth a human's eye: pass/fail verdicts differ. The important
# direction is structural-FAIL vs PSNR-pass (a clustered change PSNR averaged
# away); the reverse means structural tolerated noise PSNR happened to flag.
my $disagree = ($sid_label ne 'SKIP' && $sid_label ne 'ERROR' && $sid_pass != $psnr_pass);

# Clean pass: PSNR is happy and structural agrees. Drop the artifacts.
if ($psnr_label eq 'OK' && $sid_pass && !$disagree) {
    unlink $OVERLAY_PNG if -e $OVERLAY_PNG;
    print "OK     " . ($dbz eq 'inf' ? '' : "PSNR = $dbz dB ") . "[sid:$sid_label]";
    Cleanup();
    exit(0);
}

# Otherwise keep inspection artifacts: the structural overlay (already written,
# unless skipped) plus the expected<->result flicker webp.
CreateWebp("failures/${NAME}_error.webp", $EXPECTED_PNG, $RESULT_PNG);

print "SHADOW-DISAGREE " if $disagree;
my $tail = "PSNR = $dbz dB [sid:$sid_label" . ($sid_out ne '' ? " $sid_out" : "") . "]";
if ($psnr_pass) {
    print "$psnr_label  $tail";
    exit(0);
}
else {
    print "FAIL     $tail";
    exit(1);
}
