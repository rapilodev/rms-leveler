# rms-leveler

These LADSPA audio plugins can be used to level audio streams.

The plugins work with windows of different sizes.

In order to level exactly, this results in a certain time delay.

The limiter plugins limit the signal to -20dB RMS.

The leveler plugins also raises the signal above a certain threshold to -20db RMS.

The EBU-R128 plugin uses libebur128 to measure the volume in LUFS instead of RMS.

