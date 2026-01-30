#!/usr/bin/env perl

use strict;
use warnings;
use File::Temp qw(tempfile tempdir);
use IPC::Run qw(run timeout);

my $PLUGIN_DIR = '.';
my $SAMPLE_RATE = 48000;
my $TIMEOUT = 60;

# Simple timing: 10s warmup + 10s measurement = 20s total
my $WARMUP_TIME = 10;
my $MEASURE_TIME = 10;
my $TOTAL_DURATION = $WARMUP_TIME + $MEASURE_TIME;

my $tempdir = tempdir(CLEANUP => 1);

my @plugins = (
    { file => "$PLUGIN_DIR/rms-leveler-0.3s.so", label => 'rms_leveler_0.3s', window => 1 },
    { file => "$PLUGIN_DIR/rms-leveler-1s.so", label => 'rms_leveler_1s', window => 1 },
    { file => "$PLUGIN_DIR/rms-leveler-3s.so", label => 'rms_leveler_3s', window => 3 },
    { file => "$PLUGIN_DIR/rms-leveler-6s.so", label => 'rms_leveler_6s', window => 6 },
    { file => "$PLUGIN_DIR/rms-leveler-6s-multi.so", label => 'rms_leveler_6s_multi', window => 6 },
    { file => "$PLUGIN_DIR/rms-limiter-0.3s.so", label => 'rms_limiter_0.3s', window => 1 },
    { file => "$PLUGIN_DIR/rms-limiter-1s.so", label => 'rms_limiter_1s', window => 1 },
    { file => "$PLUGIN_DIR/rms-limiter-3s.so", label => 'rms_limiter_3s', window => 3 },
    { file => "$PLUGIN_DIR/rms-limiter-6s.so", label => 'rms_limiter_6s', window => 6 },
    { file => "$PLUGIN_DIR/ebur128-leveler-3s.so", label => 'ebur128_leveler_3s', window => 3 },
    { file => "$PLUGIN_DIR/ebur128-leveler-6s.so", label => 'ebur128_leveler_6s', window => 6 },
    { file => "$PLUGIN_DIR/ebur128-limiter-3s.so", label => 'ebur128_limiter_3s', window => 3 },
    { file => "$PLUGIN_DIR/ebur128-limiter-6s.so", label => 'ebur128_limiter_6s', window => 6 },
);

for my $plugin (@plugins) {
    next unless -f $plugin->{file};
    analyze_plugin($plugin);
    print "\n" . "=" x 80 . "\n\n";
}

sub analyze_plugin {
    my ($plugin) = @_;
    my $name = $plugin->{label};
    my $window = $plugin->{window};

    print "PLUGIN: $name (${window}s window)\n";
    print "Timing: ${TOTAL_DURATION}s total (skip first ${WARMUP_TIME}s, measure ${MEASURE_TIME}s)\n";
    print "=" x 80 . "\n";
    printf "%-15s | %-12s | %-12s | %-12s | %s\n",
           "Input Level", "Input RMS", "Output RMS", "Change", "Behavior";
    print "-" x 80 . "\n";

    my @test_levels = (
        { amp => 0.05, desc => "Very Quiet" },
        { amp => 0.1,  desc => "Quiet" },
        { amp => 0.2,  desc => "Almost Quiet" },
        { amp => 0.4,  desc => "Medium-Low" },
        { amp => 0.5,  desc => "Medium" },
        { amp => 0.7,  desc => "Medium-High" },
        { amp => 0.9,  desc => "Loud" },
        { amp => 0.95, desc => "Very Loud" },
        { amp => 1.5,  desc => "Loud Loud" },
        { amp => 2.0,  desc => "Mega Loud" },
        { amp => 4.0,  desc => "Ultra Loud" },
    );

    my @results;
    for my $test (@test_levels) {
        my ($in, $out) = generate_test_signal($test->{amp}, $TOTAL_DURATION);
        process_with_plugin($in, $out, $plugin);

        # Skip first 10s, measure next 10s
        my $in_rms = get_rms_level($in, $WARMUP_TIME, $MEASURE_TIME);
        my $out_rms = get_rms_level($out, $WARMUP_TIME, $MEASURE_TIME);

        if (defined $in_rms && defined $out_rms) {
            my $change = $out_rms - $in_rms;
            my $behavior =
                $change > 1  ? "BOOSTING ↑" :
                $change < -1 ? "ATTENUATING ↓" :
                "PASSING →";

            printf "%-15s | %9.2f dB | %9.2f dB | %+9.2f dB | %s\n",
                   $test->{desc},
                   $in_rms,
                   $out_rms,
                   $change,
                   $behavior;

            push @results, {
                level => $test->{desc},
                amp => $test->{amp},
                in => $in_rms,
                out => $out_rms,
                change => $change,
            };
        }
    }

    print "-" x 80 . "\n";

    # Analyze the pattern
    if (@results >= 3) {
        my $quiet_change = $results[0]{change};
        my $medium_change = $results[3]{change};
        my $loud_change = $results[-1]{change};

        print "\nBEHAVIOR ANALYSIS:\n";
        print sprintf("  Quiet signals  (amp=%.2f): %+.2f dB %s\n",
                     $results[0]{amp}, $quiet_change,
                     $quiet_change > 0 ? "(boosted)" : "(attenuated)");
        print sprintf("  Medium signals (amp=%.2f): %+.2f dB %s\n",
                     $results[3]{amp}, $medium_change,
                     abs($medium_change) < 0.5 ? "(unchanged)" :
                     $medium_change > 0 ? "(boosted)" : "(attenuated)");
        print sprintf("  Loud signals   (amp=%.2f): %+.2f dB %s\n",
                     $results[-1]{amp}, $loud_change,
                     $loud_change < 0 ? "(attenuated)" : "(boosted)");

        # Determine actual behavior
        print "\nCONCLUSION: ";
        if ($quiet_change > 3 && $loud_change < -3) {
            print "✓ PROPER LEVELER - Boosts quiet, reduces loud\n";
        } elsif ($quiet_change < -1 && $loud_change < -1) {
            print "⚠ COMPRESSOR/ATTENUATOR - Reduces all signals\n";
        } elsif ($quiet_change < -1 && $loud_change > 1) {
            print "✗ INVERTED LEVELER - Does the opposite! (BUG)\n";
        } elsif (abs($quiet_change) < 1 && $loud_change < -3) {
            print "✓ PROPER LIMITER - Passes quiet, limits loud\n";
        } elsif (abs($quiet_change) < 1 && abs($loud_change) < 1) {
            print "⚠ BYPASS/TRANSPARENT - No significant effect\n";
        } else {
            print "? UNCLEAR PATTERN - See values above\n";
        }

        # Dynamic range
        my $in_range = $results[-1]{in} - $results[0]{in};
        my $out_range = $results[-1]{out} - $results[0]{out};
        print sprintf("\nDynamic Range: %.2f dB → %.2f dB ", $in_range, $out_range);

        my $reduction = $in_range - $out_range;
        if ($reduction > 3) {
            print "(REDUCED by " . sprintf("%.2f", $reduction) . " dB) ✓\n";
        } elsif ($reduction < -3) {
            print "(INCREASED by " . sprintf("%.2f", -$reduction) . " dB) ✗\n";
        } else {
            print "(UNCHANGED)\n";
        }

        # Output variance
        my @output_levels = map { $_->{out} } @results;
        my $min_out = (sort { $a <=> $b } @output_levels)[0];
        my $max_out = (sort { $a <=> $b } @output_levels)[-1];
        my $out_variance = $max_out - $min_out;

        print sprintf("Output variance: %.2f dB ", $out_variance);
        if ($out_variance < 6) {
            print "(EXCELLENT - tight leveling)\n";
        } elsif ($out_variance < 12) {
            print "(GOOD - moderate leveling)\n";
        } elsif ($out_variance < 18) {
            print "(FAIR - some leveling)\n";
        } else {
            print "(POOR - minimal leveling)\n";
        }

        # Average output level
        my $avg_out = 0;
        $avg_out += $_->{out} for @results;
        $avg_out /= @results;
        print sprintf("Average output level: %.2f dB\n", $avg_out);
    }
}

sub generate_test_signal {
    my ($amplitude, $duration) = @_;
    my ($fh_in, $in_file) = tempfile(DIR => $tempdir, SUFFIX => '.wav', UNLINK => 1);
    my ($fh_out, $out_file) = tempfile(DIR => $tempdir, SUFFIX => '.wav', UNLINK => 1);
    close $fh_in;
    close $fh_out;

    my @cmd = (
        'ffmpeg',
        '-f', 'lavfi',
        '-i', "sine=frequency=1000:duration=$duration:sample_rate=$SAMPLE_RATE",
        '-af', "volume=$amplitude",
        '-y', $in_file,
        '-v', 'error'
    );

    my ($in, $out, $err);
    run(\@cmd, \$in, \$out, \$err, timeout => $TIMEOUT);

    if ($err) {
        warn "Error generating signal: $err\n";
    }

    return ($in_file, $out_file);
}

sub process_with_plugin {
    my ($in_file, $out_file, $plugin) = @_;

    my @cmd = (
        'ffmpeg',
        '-i', $in_file,
        '-af', "ladspa=file=$plugin->{file}:$plugin->{label}",
        '-y', $out_file,
        '-v', 'error'
    );

    my ($in, $out, $err);
    run(\@cmd, \$in, \$out, \$err, timeout => $TIMEOUT);

    if ($err) {
        warn "Error processing with plugin: $err\n";
    }
}

sub get_rms_level {
    my ($file, $skip_seconds, $measure_seconds) = @_;

    my @cmd = (
        'ffmpeg',
        '-ss', $skip_seconds,
        '-t', $measure_seconds,
        '-i', $file,
        '-af', 'volumedetect',
        '-f', 'null',
        '-'
    );

    my ($in, $out, $err);
    run(\@cmd, \$in, \$out, \$err, timeout => $TIMEOUT);

    if ($err && $err =~ /mean_volume:\s*([-\d.]+)\s*dB/) {
        return $1;
    }

    return undef;
}
