## rms-leveler

`rms-leveler` is a comprehensive set of LADSPA audio plugins designed to level out stereo audio effectively. These plugins leverage look-ahead windows of varying sizes to provide precise RMS or LUFS-based volume control, ensuring consistent audio levels across different tracks or streams.

### Features
- **Independent Stereo Channel Leveling:** Each stereo channel is processed independently for balanced audio output.
- **Multiple Look-Ahead Windows:** Available in 0.3s, 1s, 3s, and 6s windows, allowing for fine-tuned handling of audio dynamics.
- **RMS and LUFS Limiting:** Plugins cap the signal at -20dB RMS or -20 LUFS for consistent output levels.
- **Signal Boosting:** Leveler plugins can boost signals below -40dB up to -20dB RMS, enhancing low-volume audio.
- **EBU-R128 Compliance:** EBU-R128 plugins measure volume in LUFS using `libebur128` for broadcast-standard compliance.
- **Monitoring Capabilities:** Monitor plugins log RMS or LUFS values to a file with timestamps, allowing detailed post-processing analysis.
- **Peak Monitoring:** New peak monitoring plugins provide insight into peak levels over a 6-second window, ideal for detecting clipping or other peak-related issues.

### Available Plugins
- **RMS Levelers & Limiters:** For different window sizes (0.3s, 1s, 3s, 6s)
  - `rms_leveler_0.3s`, `rms_limiter_0.3s`
  - `rms_leveler_1s`, `rms_limiter_1s`
  - `rms_leveler_3s`, `rms_limiter_3s`
  - `rms_leveler_6s`, `rms_limiter_6s`
- **EBU-R128 Levelers & Limiters:** For broadcast compliance (3s and 6s windows)
  - `ebur128_leveler_3s`, `ebur128_limiter_3s`
  - `ebur128_leveler_6s`, `ebur128_limiter_6s`
- **Monitoring Plugins:** To log and compare RMS, LUFS, or Peak values
  - **RMS Monitoring:**
    - `rms_monitor_in_6s`, `rms_monitor_out_6s`
  - **LUFS Monitoring:**
    - `ebur128_monitor_in_6s`, `ebur128_monitor_out_6s`
  - **Peak Monitoring:**
    - `peak_monitor_in_6s`, `peak_monitor_out_6s`

### Examples

**1. Basic File Processing with FFmpeg:**

- **Level a Stereo Audio File (RMS Leveler with 3s Window):**

```bash
export LADSPA_PATH=/usr/lib/ladspa/
ffmpeg -i input.wav -af ladspa=file=/usr/lib/ladspa/rms-leveler-3s.so:rms_leveler_3s output.wav
```

- **Apply LUFS Limiting (EBU-R128 Limiter with 6s Window):**

```bash
ffmpeg -i input.wav -af ladspa=file=/usr/lib/ladspa/ebur128-limiter-6s.so:ebur128_limiter_6s output_limited.wav
```

**2. Real-Time Audio Processing with Liquidsoap:**

- **Normalize a Live Audio Stream (RMS Leveler with 6s Window):**

```bash
liquidsoap 'out(ladspa.rms_leveler_6s(input.http("http://example.com/stream")))'
```

- **Broadcast with LUFS Leveling (EBU-R128 Leveler with 3s Window):**

```bash
liquidsoap 'out(ladspa.ebur128_leveler_3s(input.http("http://example.com/stream")))'
```

**4. Detailed Logging and Analysis:**

- **Monitor LUFS Levels Over Time (EBU-R128 Monitor with 6s Window):**

```bash
liquidsoap 'radio=input.http("http://example.com/stream"); radio=ladspa.ebur128_monitor_in_6s(radio); out(radio)'
```

- **Log RMS Values Before and After Processing (Using Liquidsoap):**

```bash
liquidsoap 'radio=input.http("http://example.com/stream"); radio=ladspa.rms_monitor_in_6s(radio); radio=ladspa.rms_leveler_3s(radio); radio=ladspa.rms_monitor_out_6s(radio); out(radio)'
```

- **Monitor Peak Levels (Using Liquidsoap):**

```bash
liquidsoap 'radio=input.http("http://example.com/stream"); radio=ladspa.peak_monitor_in_6s(radio); radio=ladspa.peak_monitor_out_6s(radio); out(radio)'
```

### Additional Use Cases

**Batch Processing:**

- **Batch Level Multiple Files (RMS Leveler with 3s Window):**

```bash
for file in *.wav; do
    ffmpeg -i "$file" -af ladspa=file=/usr/lib/ladspa/rms-leveler-3s.so:rms_leveler_3s "leveled_$file"
done
```

### Integrating with Other Tools:

**Integrate with Audacity:**
  1. Install LADSPA plugins in the appropriate directory.
  2. In Audacity, go to Effects > LADSPA and select the desired plugin, such as `rms_leveler_3s`.
  3. Apply the effect to smooth out volume levels in your audio project.

**Read Data from Broadcast:**

  1. Data is sent to broadcast port 65432, where it can be collected from:
  
```bash
nc -luk 65432
2024-08-17 23:21:25 rms-a   -19.892 -20.137
2024-08-17 23:21:25 rms-b   -19.788 -19.832
2024-08-17 23:21:25 peak-a   -2.432  -1.987
2024-08-17 23:21:25 peak-b   -2.289  -1.998
```
