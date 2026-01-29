plugindir = $(libdir)/ladspa

CFLAGS:=$(shell dpkg-buildflags --get CPPFLAGS)
LDFLAGS:=$(shell dpkg-buildflags --get LDFLAGS)

all:
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o peak-monitor-in.so peak-monitor-in.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o peak-monitor-out.so peak-monitor-out.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-leveler-0.3s.so rms-leveler-0.3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-leveler-1s.so rms-leveler-1s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-leveler-3s.so rms-leveler-3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-leveler-6s.so rms-leveler-6s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-0.3s.so rms-limiter-0.3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-1s.so rms-limiter-1s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-3s.so rms-limiter-3s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-6s.so rms-limiter-6s.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-limiter-instant-1m.so rms-limiter-instant-1m.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-monitor-in.so rms-monitor-in.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC -o rms-monitor-out.so rms-monitor-out.c
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC rms-leveler-6s-multi.c /usr/lib/*/libebur128.so -o rms-leveler-6s-multi.so
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC rms-limiter-6s-multi.c /usr/lib/*/libebur128.so -o rms-limiter-6s-multi.so
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC ebur128-leveler-6s.c /usr/lib/*/libebur128.so -o ebur128-leveler-6s.so
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC ebur128-limiter-6s.c /usr/lib/*/libebur128.so -o ebur128-limiter-6s.so
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC ebur128-leveler-3s.c /usr/lib/*/libebur128.so -o ebur128-leveler-3s.so
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC ebur128-limiter-3s.c /usr/lib/*/libebur128.so -o ebur128-limiter-3s.so
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC ebur128-monitor-in.c /usr/lib/*/libebur128.so -o ebur128-monitor-in.so
	gcc -O2 $(CFLAGS) $(LDFLAGS) -Wall -shared -fPIC ebur128-monitor-out.c /usr/lib/*/libebur128.so -o ebur128-monitor-out.so

clean:
	rm -f *.so

