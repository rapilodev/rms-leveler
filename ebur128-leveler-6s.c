#include <stdlib.h> 
#include <ladspa.h>
#include "ebur128.h"
#include <stdio.h>
#include <math.h>

/*
 * LADSPA plugin to level stereo audio to -20dBFS with -1dBFS head.
 * use a 6 second lookahead window.
 * level both channels independently.
 * limit amplification at -40dB
 * do not increase amplification on decreasing loudness
 * interpolate amplification
 * tests: 
 * - ffmpeg -i easy.wav -af ladspa=file=rmsLeveler_6s:rms_leveler_6s output1.wav
 * - ffmpeg -i easy.wav -af ladspa=file=ebur128Leveler_6s:ebur128_leveler_6s output1.wav
 * - liquidsoap 'out(ladspa.rms_leveler2(mksafe(audio_to_stereo(input.http("http://ice.rosebud-media.de:8000/88vier-low")))))'
 * Milan Chrobok, 2016 to 2019
 */

const int debug = 0;
const double maxChannels = 2;

// target loudness, should be -20 DB
const double TARGET_LOUDNESS = -20.0;

// long term measurement window
const double BUFFER_DURATION = 6;

// how often update statistics in seconds
const double ADJUST_RATE = 0.333;

// compression start = -3dB
const double compressionStart = 0.707945784;

// maximum level = -1dB
const double MAX_LEVEL = 0.891250938;

// amplitude limit to what DC offset is not removed
const double dcOffsetLimit = 0.005;

// minimum loudness = -40dB =0.01
const double MIN_LOUDNESS = -40;

// max amplication change per second
const double MAX_CHANGE = 0.5;

const int SECONDS = 1000;

// channel indexes
#define IN_LEFT    0
#define IN_RIGHT   1
#define OUT_LEFT   2
#define OUT_RIGHT  3

struct Window {
    unsigned long size;
    unsigned long dataSize;
    double duration;
    LADSPA_Data* data;
    double sum;
    double loudness;
    double oldLoudness;
    double position;
    double deltaPosition;
    unsigned long index;
    unsigned long adjustPosition;
    double adjustRate;
    double maxAmpChange;
    unsigned long playPosition;
    double amplification;
    double oldAmplification;
};

void initWindow(struct Window* window, double duration, double rate) {
    window->duration = duration;
    window->dataSize = duration * rate;
    window->data = (LADSPA_Data*) calloc(window->dataSize, sizeof(LADSPA_Data));
    window->sum = 0;
    window->loudness = 0.0;
    window->oldLoudness = 0.0;
    window->size = 0;
    window->position = 0.0;
    window->index = 0;
    window->adjustPosition = 0;
    window->adjustRate = (int) ( rate * ADJUST_RATE ) ;
    window->maxAmpChange = MAX_CHANGE * ADJUST_RATE;
    window->deltaPosition = 1.0 / rate;
    window->amplification = 1.0;
    window->oldAmplification = 1.0;
}


inline void addWindowData(struct Window* window, LADSPA_Data value) {
    window->sum -= window->data[window->index];
    window->data[window->index] = value;
    window->sum += window->data[window->index];
}


inline double getWindowDcOffset(struct Window* window) {
    double dcOffset = window->sum / window->size;
    if ((dcOffset > -dcOffsetLimit) && (dcOffset < dcOffsetLimit))
        return 0.0;

    return dcOffset;
}

inline void prepareWindow(struct Window* window) {
    //increase buffer size on start
    if (window->size < window->dataSize)
        window->size++;

    window->playPosition = window->index + window->dataSize / 2;
    if (window->playPosition >= window->dataSize)
        window->playPosition -= window->dataSize;

    while (window->index >= window->dataSize) {
        window->index -= window->dataSize;
    }
}

inline void moveWindow(struct Window* window) {
    window->index++;
    if (window->index >= window->dataSize)
        window->index -= window->dataSize;

    window->playPosition++;
    if (window->playPosition >= window->dataSize)
        window->playPosition -= window->dataSize;

    window->adjustPosition++;
    if (window->adjustPosition >= window->adjustRate)
        window->adjustPosition -= window->adjustRate;

    window->position += window->deltaPosition;

}

struct Channel {
    LADSPA_Data* in;
    LADSPA_Data* out;
    ebur128_state* ebur128;

    double amplification;
    double oldAmplification;

    struct Window window;
};

// define our handler type
typedef struct {
    struct Channel* channels[2];
    struct Channel left;
    struct Channel right;
    unsigned int rate;
} Leveler;

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    Leveler * h = malloc(sizeof(Leveler));

    h->channels[0] = &h->left;
    h->channels[1] = &h->right;
    h->rate = rate;

    int i = 0;
    for (i = 0; i < maxChannels; i++) {
        struct Channel* channel = h->channels[i];

        struct Window* window;
        window = &channel->window;
        initWindow(window, BUFFER_DURATION, h->rate);

        channel->ebur128 = ebur128_init(1, h->rate, EBUR128_MODE_LRA);
        ebur128_set_max_window(channel->ebur128, window->duration*SECONDS);

        channel->amplification = 1.0;
        channel->oldAmplification = 1.0;
    }
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    //fprintf(stderr, "cleanup\n");
    Leveler * h = (Leveler *) handle;
    free(h->left.window.data);
    free(h->right.window.data);

    ebur128_destroy(&h->left.ebur128);
    ebur128_destroy(&h->right.ebur128);
    
    free(handle);
}

static void connect_port(const LADSPA_Handle handle, unsigned long num, LADSPA_Data * port) {
    Leveler * h = (Leveler *) handle;
    if (num == IN_LEFT)
        h->left.in = port;
    if (num == IN_RIGHT)
        h->right.in = port;
    if (num == OUT_LEFT)
        h->left.out = port;
    if (num == OUT_RIGHT)
        h->right.out = port;
}

inline double getRmsValue(const double rmsSum, const double size) {
    double sum = rmsSum / size;
    if (sum == 0.0)
        sum = 0.0000000000001;
    return 20.0 * log10(sqrt(sum));
}

inline double getAmplification(const double loudness, const double oldLoudness, const double oldAmp) {

    if (loudness < MIN_LOUDNESS)
        return oldAmp;

    // get target factor from loudness delta
    double amp = pow(10., (TARGET_LOUDNESS - loudness) / 20.0);
    if (isnan(amp))
        amp = 1.;
    if (amp == 0.)
        amp = 1.;

    // on decreasing loudness do not increase amplification
    if ((amp > 1.0) && (loudness <= oldLoudness))
        amp = oldAmp;

    return amp;
}

inline double interpolateAmplification(const double amp, const double oldAmp, unsigned long pos, const unsigned long maxPos) {
    if (pos > maxPos)
        pos = maxPos;
    if (pos < 0)
        pos = 0;
    double x = (double) pos / maxPos;

    // linear
    //double proportion = x;

    // smoothstep
    double proportion = x * x * (3 - 2 * x);

    // smootherstep
    //double proportion =  x * x * x * (x * (x * 6 - 15) + 10);

    // smootheststep
    //double proportion = x * x * x * x * (x * (x * (-20.0 * x + 70.0) - 84.0) + 35.0);

    double amplification = (proportion * amp) + ((1.0 - proportion) * oldAmp);
    //fprintf(stderr, "%lu %lu %f %f\n", pos, maxPos, proportion,amplification);
    return amplification;
}


// compress by logarithm function (up to 10)
// scale by compressionFactor to limit value to -1 dB
// log10(1)=0, log10(10)=1
inline double compress(const double value) {
    const double factor = (1.0 - compressionStart);
    return factor * log10(value + 1.0);
}

// limit value by linear compression of values louder than compressionStart (-3dB) to reach a maximum of -1dB
inline double limit(double value) {

    double amplitude = value;
    if (value < 0.0)
        amplitude = -amplitude;
    if (amplitude > compressionStart) {
        amplitude = compressionStart + compress(amplitude - compressionStart);
    }
    if (value < 0.0)
        amplitude = -amplitude;

    if (amplitude > MAX_LEVEL)
        amplitude = MAX_LEVEL;
    if (amplitude < -MAX_LEVEL)
        amplitude = -MAX_LEVEL;
    return amplitude;
}

void calcWindowAmplification(struct Window* window, double loudness) {
    window->oldLoudness = window->loudness;
    window->loudness = loudness;

    window->oldAmplification = window->amplification;
    window->amplification = getAmplification(window->loudness, window->oldLoudness, window->amplification);

    
    double limit = window->oldAmplification + window->maxAmpChange;
    if (window->amplification > limit) {
        window->amplification = limit;
    }
}



static void run(LADSPA_Handle handle, unsigned long samples) {
    Leveler * h = (Leveler *) handle;
    double loudness_window;

    int c = 0;
    for (c = 0; c < maxChannels; c++) {
        struct Channel* channel = h->channels[c];
        struct Window* window = &channel->window;

        unsigned long s;
        for (s = 0; s < samples; s++) {

            prepareWindow(window);
            addWindowData(window, channel->in[s]);

            ebur128_add_frames_float(channel->ebur128, &channel->in[s], (size_t) 1);

            // interpolate with shifted adjust position
            double ampFactor = interpolateAmplification(channel->amplification, channel->oldAmplification, window->adjustPosition,
                    window->adjustRate);

            // read from playPosition, amplify and limit
            double value = (window->data[window->playPosition] - getWindowDcOffset(window)) * ampFactor;
            value = limit(value);
            channel->out[s] = (LADSPA_Data) value;

            // calculate amplification from EBU R128 values
            if (window->adjustPosition == 0) {
                ebur128_loudness_window(channel->ebur128, window->duration*SECONDS, &loudness_window);
                calcWindowAmplification(window, loudness_window);

                channel->amplification    = window->amplification;
                channel->oldAmplification = window->oldAmplification;

                if ((debug==1)&&(c==0)){
                    fprintf(
                        stderr, "%.1f\t%2.3f\t%2.3f\t%2.3f\n", 
                        window->position, loudness_window, channel->in[s], value
                    );
                }
            }
            moveWindow(window);
        }
    }
}

static const char * c_port_names[4] = { "Input - Left Channel", "Input - Right Channel", "Output - Left Channel", "Output - Right Channel" };

static LADSPA_PortDescriptor c_port_descriptors[4] = { LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
        LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT };

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b3e5, .Label = "ebur128_leveler_6s", .Name =
        "EBU R128 level -20dBFS, stereo, 6 seconds", .Maker = "Milan Chrobok", .Copyright = "GPL 2 or 3", .PortCount = 4,
        .PortDescriptors = c_port_descriptors, .PortNames = c_port_names, .instantiate = instantiate, .connect_port = connect_port, .run =
                run, .cleanup = cleanup };

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0)
        return &c_ladspa_descriptor;
    return 0;
}

