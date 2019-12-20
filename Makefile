plugindir = $(libdir)/ladspa

CFLAGS:=$(shell dpkg-buildflags --get CPPFLAGS)
LDFLAGS:=$(shell dpkg-buildflags --get LDFLAGS)

all:
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLeveler_0.3s.so rmsLeveler_0.3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLeveler_1s.so rmsLeveler_1s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLeveler_3s.so rmsLeveler_3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLeveler_6s.so rmsLeveler_6s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLeveler_10s.so rmsLeveler_10s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLimiter_0.3s.so rmsLimiter_0.3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLimiter_1s.so rmsLimiter_1s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLimiter_3s.so rmsLimiter_3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLimiter_6s.so rmsLimiter_6s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rmsLimiter_10s.so rmsLimiter_10s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o libebur128.so ebur128.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC ebur128Leveler_6s.c libebur128.so  -o ebur128Leveler_6s.so

clean:
	rm -f rmsLeveler.so
	rm -f rmsLimiter.so


#  ffmpeg -i easy.wav -af ladspa=file=rmsLeveler_6s:rms_leveler_6s output2.wav
#  ffmpeg -i easy.wav -af ladspa=file=ebur128Leveler_6s:r128_leveler_6s output1.wav

