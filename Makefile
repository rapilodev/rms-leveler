plugindir = $(libdir)/ladspa

CFLAGS:=$(shell dpkg-buildflags --get CPPFLAGS)
LDFLAGS:=$(shell dpkg-buildflags --get LDFLAGS)

all:
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-leveler-0.3s.so rms-leveler-0.3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-leveler-1s.so rms-leveler-1s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-leveler-3s.so rms-leveler-3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-leveler-6s.so rms-leveler-6s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-leveler-10s.so rms-leveler-10s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-0.3s.so rms-limiter-0.3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-1s.so rms-limiter-1s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-3s.so rms-limiter-3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-6s.so rms-limiter-6s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-10s.so rms-limiter-10s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC ebur128-leveler-6s.c /usr/lib/*/libebur128.so -o ebur128-leveler-6s.so

clean:
	rm -f *.so

#  ffmpeg -i easy.wav -af ladspa=file=rms-leveler-6s:rms_leveler_6s output2.wav
#  ffmpeg -i easy.wav -af ladspa=file=ebur128Leveler_6s:r128_leveler_6s output1.wav

