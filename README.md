# rms-leveler

LADSPA plugins for loudness leveling of stereo audio streams.

## What It Does

- **Levelers**: Normalize audio to -20dB RMS or -20 LUFS (boost quiet parts, reduce loud parts)
- **Limiters**: Cap audio at -20dB RMS or -20 LUFS (only reduce, never boost)
- **Monitors**: Broadcast real-time RMS, LUFS, and peak measurements to UDP port 65432

## Installation

```bash
make
sudo make install
```

**Dependencies**: ladspa-sdk, libebur128-dev

## Available Plugins

### RMS Levelers

| Plugin | Window | Latency |
|--------|--------|---------|
| `rms_leveler_0.3s` | 0.3s | 300ms |
| `rms_leveler_1s` | 1s | 1s |
| `rms_leveler_3s` | 3s | 3s |
| `rms_leveler_6s` | 6s | 6s |

### RMS Limiters

| Plugin | Window | Latency |
|--------|--------|---------|
| `rms_limiter_0.3s` | 0.3s | 300ms |
| `rms_limiter_1s` | 1s | 1s |
| `rms_limiter_3s` | 3s | 3s |
| `rms_limiter_6s` | 6s | 6s |
| `rms_limiter_instant_1m` | 1min rolling | 0ms |

### EBU R128 (LUFS)

| Plugin | Window | Standard |
|--------|--------|----------|
| `ebur128_leveler_3s` | 3s | EBU R128 short-term |
| `ebur128_leveler_6s` | 6s | EBU R128 |
| `ebur128_limiter_3s` | 3s | EBU R128 short-term |
| `ebur128_limiter_6s` | 6s | EBU R128 |

### Monitors

| Plugin | Measurement |
|--------|-------------|
| `rms_monitor_in_6s` | RMS input |
| `rms_monitor_out_6s` | RMS output |
| `ebur128_monitor_in_6s` | LUFS input |
| `ebur128_monitor_out_6s` | LUFS output |
| `peak_monitor_in_6s` | Peak input |
| `peak_monitor_out_6s` | Peak output |

## Usage

### FFmpeg

```bash
# Normalize audio
ffmpeg -i input.wav -af ladspa=file=rms-leveler-3s.so:rms_leveler_3s output.wav

# LUFS limiting (broadcast)
ffmpeg -i input.wav -af ladspa=file=ebur128-limiter-6s.so:ebur128_limiter_6s output.wav

# Zero-latency limiting
ffmpeg -i input.wav -af ladspa=file=rms-limiter-instant-1m.so:rms_limiter_instant_1m output.wav
```

### Liquidsoap

```liquidsoap
# Normalize stream
radio = input.http("https://example.com/stream.mp3")
radio = ladspa.rms_leveler_3s(radio)
output.icecast(%mp3, mount="/normalized", radio)

# Monitor input and output
radio = input.http("https://example.com/stream.mp3")
radio = ladspa.rms_monitor_in_6s(radio)
radio = ladspa.rms_leveler_3s(radio)
radio = ladspa.rms_monitor_out_6s(radio)
output.icecast(%mp3, mount="/stream", radio)
```

### Batch Processing

```bash
for file in *.wav; do
    ffmpeg -i "$file" -af ladspa=file=rms-leveler-3s.so:rms_leveler_3s "normalized_${file}"
done
```

## Monitoring Output

Monitor plugins broadcast to **UDP port 65432**. Set `MONITOR_LOG_DIR` environment variable to enable file logging.

### Capture Data

```bash
nc -luk 65432
```

### Example Output

```
2026-01-19 21:24:13 rms-in       -21.129  -21.129
2026-01-19 21:24:13 rms-out      -20.143  -20.143
2026-01-19 21:24:13 peak-in       -2.432   -1.987
2026-01-19 21:24:13 peak-out      -2.289   -1.998
2026-01-19 21:26:15 ebur128-in   -68.561  -68.561
2026-01-19 21:26:15 ebur128-out  -56.952  -56.952
```

Format: `timestamp type left_channel right_channel`

### Log to File

```bash
# Via UDP capture
nc -luk 65432 >> levels_$(date +%Y%m%d_%H%M%S).log

# Via environment variable
export MONITOR_LOG_DIR=/var/log/audio
# Monitoring data will be written to files in this directory
```

### Filter Output

```bash
# Only RMS
nc -luk 65432 | grep "rms-"

# Only input
nc -luk 65432 | grep "\-in"

# Only output  
nc -luk 65432 | grep "\-out"
```

## Window Selection

| Window | Best For |
|--------|----------|
| 0.3s | Fast speech, dynamic content |
| 1s | Podcasts, dialogue |
| 3s | Music (recommended default) |
| 6s | Gentle processing, classical |
| 1m instant | Live streams (no latency) |

## How It Works

**Levelers**:
- Target: -20dB RMS / -20 LUFS
- Boost signals below -40dB to target
- Reduce signals above target
- Smooth transitions via look-ahead window

**Limiters**:
- Hard ceiling: -20dB RMS / -20 LUFS
- Only reduce, never boost
- Fast response to transients

**Instant Limiter**:
- 1-minute rolling RMS window
- Zero latency (no look-ahead)
- For live broadcasts where delay is unacceptable

**Measurements**:
- **RMS**: Root Mean Square (traditional power measurement)
- **LUFS**: Loudness Units Full Scale (EBU R128, perceptually weighted)
- **Peak**: Maximum sample value

## Verification

```bash
# Set plugin path
export LADSPA_PATH=/usr/lib/ladspa

# List plugins
listplugins | grep rms

# Analyze plugin
analyseplugin /usr/lib/ladspa/rms-leveler-3s.so
```

## License

GPL-3.0

## Author

Milan Chrobok ([@rapilodev](https://github.com/rapilodev))

## Related Projects

- [libebur128](https://github.com/jiixyj/libebur128) - EBU R128 library
- [rms](https://github.com/rapilodev/rms) - RMS measurement tool
- [compare-rms](https://github.com/rapilodev/compare-rms) - Compare RMS differences
- [master-audio](https://github.com/rapilodev/master-audio) - Batch mastering to -20dB RMS
