# rms-leveler

These LADSPA audio plugins can be used to level out stereo audio.

* The plugins work with look-ahead windows of different sizes.
  The look-ahead results in a certain time delay.
  The plugin rms_leveler_6s uses three windows: 0.3, 3 and 6 seconds.
* Both stereo channels are levelled independently.
* The limiter plugins limit the signal to -20dB RMS or -20 LUFS.
* The Leveler plugins additionally boost the signal above -40dB to -20db RMS.
* When the signal is faded out, the gain is not increased.
* The volume of the window is measured and interpolated three times a second.
* The EBU-R128 plugin uses libebur128 to measure the volume in LUFS instead of RMS.

There are no parameters necessary. Instead there are separate plugins for different settings:

* rms_leveler_0.3s
* rms_limiter_0.3s
* rms_leveler_1s
* rms_limiter_1s
* rms_leveler_3s
* rms_limiter_3s
* rms_leveler_6s
* rms_limiter_6s
* ebur128_leveler_3s
* ebur128_limiter_3s
* ebur128_leveler_6s
* ebur128_limiter_6s

Examples:
* export LADSPA_PATH=/usr/lib/ladspa/
* ffmpeg -i easy.wav -af ladspa=file=/usr/lib/ladspa/rms-leveler-3s.so:rms_leveler_3s output1.wav
* ffmpeg -i easy.wav -af ladspa=file=/usr/lib/ladspa/ebur128-leveler-3s.so:ebur128_leveler_3s output2.wav
* liquidsoap 'out(ladspa.rms_leveler_3s(mksafe(audio_to_stereo(input.http("http://ice.rosebud-media.de:8000/88vier-low")))))'
