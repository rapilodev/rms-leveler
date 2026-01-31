# rms-leveler

LADSPA plugins for loudness leveling of stereo audio streams.

## What It Does

- **Levelers**: Normalize audio to -20dB RMS or -20 LUFS (boost quiet parts, reduce loud parts)
- **Limiters**: Cap audio at -20dB RMS or -20 LUFS (only reduce, never boost)
- **Monitors**: Broadcast real-time RMS, LUFS, and peak measurements via UDP and optional file logging

## Installation

```bash
make
sudo make install
```

**Dependencies**: `ladspa-sdk`, `libebur128-dev`

**Installation Location**: `/usr/lib/ladspa/` (default)

### Verification

```bash
# Set plugin path
export LADSPA_PATH=/usr/lib/ladspa

# List installed plugins
listplugins | grep rms

# Analyze specific plugin
analyseplugin /usr/lib/ladspa/rms-leveler-3s.so
```

## Available Plugins

### RMS Levelers

| Plugin | Window | Latency | Best For |
|--------|--------|---------|----------|
| `rms_leveler_0.3s` | 0.3s | 300ms | Fast speech, dynamic content |
| `rms_leveler_1s` | 1s | 1s | Podcasts, dialogue |
| `rms_leveler_3s` | 3s | 3s | Music (recommended default) |
| `rms_leveler_6s` | 6s | 6s | Gentle processing, classical |

### RMS Limiters

| Plugin | Window | Latency | Best For |
|--------|--------|---------|----------|
| `rms_limiter_0.3s` | 0.3s | 300ms | Fast dynamics |
| `rms_limiter_1s` | 1s | 1s | Speech limiting |
| `rms_limiter_3s` | 3s | 3s | Music limiting |
| `rms_limiter_6s` | 6s | 6s | Smooth limiting |
| `rms_limiter_instant_1m` | 1min rolling | **0ms** | Live streams (no delay) |

### EBU R128 (LUFS) - Broadcast Standard

| Plugin | Window | Standard | Latency |
|--------|--------|----------|---------|
| `ebur128_leveler_3s` | 3s | EBU R128 short-term | 3s |
| `ebur128_leveler_6s` | 6s | EBU R128 | 6s |
| `ebur128_limiter_3s` | 3s | EBU R128 short-term | 3s |
| `ebur128_limiter_6s` | 6s | EBU R128 | 6s |

### Monitors

| Plugin | Measurement | Configurable Window |
|--------|-------------|---------------------|
| `rms_monitor_in` | RMS input | Yes (default: 6.0s) |
| `rms_monitor_out` | RMS output | Yes (default: 6.0s) |
| `ebur128_monitor_in` | LUFS input | Yes (default: 6.0s) |
| `ebur128_monitor_out` | LUFS output | Yes (default: 6.0s) |
| `peak_monitor_in` | Peak input | Yes (default: 6.0s) |
| `peak_monitor_out` | Peak output | Yes (default: 6.0s) |

**Note**: Monitor plugins support custom window durations via the `window` parameter (see usage examples below).

## Usage

### FFmpeg

#### Basic Processing

```bash
# Normalize audio (recommended for most use cases)
ffmpeg -i input.wav -af ladspa=file=rms-leveler-3s.so:rms_leveler_3s output.wav

# LUFS limiting for broadcast compliance
ffmpeg -i input.wav -af ladspa=file=ebur128-limiter-6s.so:ebur128_limiter_6s output.wav

# Zero-latency limiting for live streams
ffmpeg -i input.wav -af ladspa=file=rms-limiter-instant-1m.so:rms_limiter_instant_1m output.wav
```

#### Batch Processing

```bash
# Process all WAV files in current directory
for file in *.wav; do
    ffmpeg -i "$file" -af ladspa=file=rms-leveler-3s.so:rms_leveler_3s "normalized_${file}"
done
```

### Liquidsoap

#### Basic Stream Normalization

```liquidsoap
# Simple normalization
radio = input.http("https://example.com/stream.mp3")
radio = ladspa.rms_leveler_3s(radio)
output.icecast(%mp3, mount="/normalized", radio)
```

#### Advanced Monitoring

```liquidsoap
# Monitor with custom 3-second window
radio = input.http("https://example.com/stream.mp3")
radio = ladspa.rms_monitor_in(window=3., radio)
radio = ladspa.rms_leveler_3s(radio)
radio = ladspa.rms_monitor_out(window=3., radio)
output.icecast(%mp3, mount="/stream", radio)
```

#### Broadcast Chain

```liquidsoap
# Full broadcast processing chain with LUFS compliance
radio = input.http("https://example.com/stream.mp3")

# Monitor input levels
radio = ladspa.ebur128_monitor_in(window=6., radio)
radio = ladspa.peak_monitor_in(window=6., radio)

# Apply EBU R128 leveling
radio = ladspa.ebur128_leveler_6s(radio)

# Monitor output levels
radio = ladspa.ebur128_monitor_out(window=6., radio)
radio = ladspa.peak_monitor_out(window=6., radio)

output.icecast(%mp3, mount="/broadcast", radio)
```

## Monitoring Output

Monitor plugins broadcast measurements to **UDP port 65432 (localhost only)** and optionally write to log files.

### Capture Monitoring Data

```bash
# Capture UDP broadcast
nc -luk 65432

# Capture to timestamped log file
nc -luk 65432 >> levels_$(date +%Y%m%d_%H%M%S).log
```

### Example Output Format

```
2026-01-19 21:24:13 rms-in    -21.129  -21.129
2026-01-19 21:24:13 rms-out   -20.143  -20.143
2026-01-19 21:24:13 peak-in    -2.432   -1.987
2026-01-19 21:24:13 peak-out   -2.289   -1.998
2026-01-19 21:26:15 lufs-in   -68.561  -68.561
2026-01-19 21:26:15 lufs-out  -56.952  -56.952
```

**Format**: `timestamp type left_channel right_channel`

### File Logging

Enable file logging by setting the `MONITOR_LOG_DIR` environment variable to a writable directory:

```bash
# Set log directory (must exist and be writable)
export MONITOR_LOG_DIR=/var/log/audio
mkdir -p /var/log/audio

# Default log directory if not set
# /var/log/monitor
```

Monitor data will be written to individual files in the specified directory.

## How It Works

### Levelers

- **Target**: -20dB RMS / -20 LUFS
- **Noise Floor**: Signals below -40dB pass unchanged
- **Boosting**: Signals between -40dB and -20dB are boosted toward target
- **Reduction**: Signals above -20dB are reduced to target
- **Smoothing**: Look-ahead window provides smooth gain transitions

### Limiters

- **Hard Ceiling**: -20dB RMS / -20 LUFS
- **Behavior**: Only reduce, never boost
- **Response**: Fast response to transients via look-ahead window
- **Use Case**: Prevent signal from exceeding target level

### Instant Limiter (Zero Latency)

- **Window**: 1-minute rolling RMS calculation
- **Latency**: 0ms (no look-ahead buffer)
- **Trade-off**: Less smooth than look-ahead limiters, but no delay
- **Ideal For**: Live broadcasts where latency is unacceptable

### Measurement Types

- **RMS**: Root Mean Square - traditional power measurement
- **LUFS**: Loudness Units Full Scale (EBU R128) - perceptually weighted loudness
- **Peak**: Maximum sample value in the window

## Window Selection Guide

| Window | Latency | Response | Best For | Use Cases |
|--------|---------|----------|----------|-----------|
| 0.3s | 300ms | Very fast | Dynamic speech | Talk radio, sports commentary |
| 1s | 1s | Fast | Podcasts | Interviews, dialogue-heavy content |
| 3s | 3s | Balanced | Music | **Recommended default for most music** |
| 6s | 6s | Gentle | Classical | Orchestral music, gentle dynamics |
| 1m instant | 0ms | No look-ahead | Live streams | When any latency is unacceptable |

**General Rule**: 
- Shorter windows = faster response + more aggressive processing
- Longer windows = smoother processing + more natural dynamics

## Troubleshooting

### Plugin Not Found

```bash
# Verify installation
ls -la /usr/lib/ladspa/rms-*

# Check LADSPA path
echo $LADSPA_PATH

# Set path if needed
export LADSPA_PATH=/usr/lib/ladspa
```

### No UDP Output from Monitors

- Monitors broadcast to **localhost only** (127.0.0.1:65432)
- Check firewall is not blocking local UDP
- Verify netcat is listening: `nc -luk 65432`

### Unexpected Volume Behavior

- **Too aggressive**: Try longer window (6s instead of 3s)
- **Too gentle**: Try shorter window (1s instead of 3s)
- **Check input levels**: Use monitor plugins to verify RMS/LUFS values
- **Remember**: Levelers boost AND reduce; limiters only reduce

### Memory or Performance Issues

- Longer windows use more memory (larger buffers)
- Monitor plugins add minimal overhead
- For CPU-constrained systems, use shorter windows or instant limiter

## Technical Details

### Processing Flow

1. **Input Buffer**: Audio samples collected in circular buffer
2. **RMS/LUFS Calculation**: Computed over look-ahead window
3. **Gain Calculation**: Target gain determined based on measurement
4. **Interpolation**: Smooth gain transitions between samples
5. **Output**: Gain applied to audio samples

### Thread Safety

- Plugins are designed for single-threaded use per instance
- Multiple instances can run safely in parallel

### Sample Rate Support

- Tested with: 44.1kHz, 48kHz
- Should work with other standard sample rates
- Window sizes are time-based (seconds), not sample-based

## Related Projects

- [libebur128](https://github.com/jiixyj/libebur128) - EBU R128 library used for LUFS measurement
- [rms](https://github.com/rapilodev/rms) - RMS measurement tool
- [compare-rms](https://github.com/rapilodev/compare-rms) - Compare RMS differences between files
- [master-audio](https://github.com/rapilodev/master-audio) - Batch mastering to -20dB RMS

## Contributing

Bug reports and feature requests are welcome! Please use GitHub issues.

## License

GPL-3.0 - See [LICENSE](LICENSE) file for details

## Author

Milan Chrobok ([@rapilodev](https://github.com/rapilodev))

## Acknowledgments

- EBU R128 implementation powered by [libebur128](https://github.com/jiixyj/libebur128)
- LADSPA framework for audio plugin architecture