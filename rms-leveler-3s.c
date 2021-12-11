#include <stdlib.h> 
#include <ladspa.h>

#include <stdio.h>
#include <math.h>

//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

const int debug = 0;
const double maxChannels = 2.0;
// target loudness, should be -20 DB
const double TARGET_LOUDNESS = -20.0;
// long term measurement window
const double BUFFER_DURATION1 = 3.0;
// how often update statistics
const double ADJUST_RATE = 0.333;
// compression start = -3dB
const double compressionStart = 0.707945784;
// maximum level = -1dB
const double MAX_LEVEL = 0.891250938;
// amplitude limit to what DC offset is not removed
const double dcOffsetLimit = 0.005;
// minimum loudness = -40dB =0.01
const double MIN_LOUDNESS = -40.0;
// max amplication change per second
const double MAX_CHANGE = 0.7;

// channel indexes
#define IN_LEFT    0
#define IN_RIGHT   1
#define OUT_LEFT   2
#define OUT_RIGHT  3

struct Window {
    unsigned long size;
    unsigned long dataSize;
    LADSPA_Data* data;
    double* square;
    double sumSquare;
    double sum;
    double rms;
    double oldRms;
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
    window->dataSize = (unsigned long) (duration * rate);
    window->data = (LADSPA_Data*) calloc(window->dataSize, sizeof(LADSPA_Data));
    window->square = (double*) calloc(window->dataSize, sizeof(double));
    window->sum = 0;
    window->sumSquare = 0;
    window->rms = 0.0;
    window->oldRms = 0.0;
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

inline void sumWindowData(struct Window* window) {
    double value = window->data[window->index];
    window->sumSquare -= window->square[window->index];
    window->square[window->index] = value * value;
    window->sumSquare += window->square[window->index];
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

    double amplification;
    double oldAmplification;
    double oldAmplificationSmoothed;

    struct Window window1;
};

// define our handler type
typedef struct {
    struct Channel* channels[2];
    struct Channel left;
    struct Channel right;
    unsigned long rate;
} Leveler;

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    Leveler * h = malloc(sizeof(Leveler));

    h->channels[0] = &h->left;
    h->channels[1] = &h->right;

    h->rate = rate;

    int i = 0;
    for (i = 0; i < maxChannels; i++) {
        struct Channel* channel = h->channels[i];

        struct Window* window1;
        window1 = &channel->window1;
        initWindow(window1, BUFFER_DURATION1, h->rate);

        channel->amplification = 0.0;
        channel->oldAmplification = 0.0;
        channel->oldAmplificationSmoothed = 0.0;
    }
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    Leveler * h = (Leveler *) handle;
    free(h->left.window1.data);
    free(h->left.window1.square);
    free(h->right.window1.data);
    free(h->right.window1.square);
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

inline double interpolateAmplification(const double amp, const double oldAmp, double pos, const double maxPos) {
    if (pos > maxPos)
        pos = maxPos;
    if (pos < 0)
        pos = 0;
    double x = pos / maxPos;
    double proportion = x * x * (3 - 2 * x);
    double amplification = (proportion * amp) + ((1.0 - proportion) * oldAmp);
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

void calcWindowAmplification(struct Window* window) {
    window->oldRms = window->rms;
    window->rms = getRmsValue(window->sumSquare, window->size);

    window->oldAmplification = window->amplification;
    window->amplification = getAmplification(window->rms, window->oldRms, window->amplification);

    double limit = window->oldAmplification + window->maxAmpChange;
    if (window->amplification > limit) {
        window->amplification = limit;
    }
}

void printWindow(struct Window* window, int isLast) {
    if (debug == 1) {
        fprintf(stderr, "%.1f\t%2.3f\t%2.3f\t%2.3f", window->position, window->rms, (TARGET_LOUDNESS - window->rms), window->amplification);
        if (isLast) {
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, "  ");
        }
    }
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    Leveler * h = (Leveler *) handle;

    int c = 0;
    for (c = 0; c < maxChannels; c++) {
        struct Channel* channel = h->channels[c];
        struct Window* window1 = &channel->window1;

        unsigned long s;
        for (s = 0; s < samples; s++) {

            prepareWindow(window1);
            addWindowData(window1, channel->in[s]);
            sumWindowData(window1);

            // interpolate with shifted adjust position
            double ampFactor = interpolateAmplification(channel->amplification, channel->oldAmplification, window1->adjustPosition,
                    window1->adjustRate);

            // read from playPosition, amplify and limit
            double value = (window1->data[window1->playPosition] - getWindowDcOffset(window1)) * ampFactor;
            value = limit(value);
            channel->out[s] = (LADSPA_Data) value;

            // calculate amplification from RMS values
            //if (debug==1) printWindow(window1, (channel == h->channels[1]));
            if (window1->adjustPosition == 0) {
                calcWindowAmplification(window1);
            }

            channel->amplification    = window1->amplification;
            channel->oldAmplification = window1->oldAmplification;

            moveWindow(window1);
        }
    }
}

static const char * c_port_names[4] = { "Input - Left Channel", "Input - Right Channel", "Output - Left Channel", "Output - Right Channel" };

static LADSPA_PortDescriptor c_port_descriptors[4] = { LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
        LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT };

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b3e5, .Label = "rms_leveler_3s", .Name =
        "RMS leveler -20dBFS, stereo, 3 seconds window", .Maker = "Milan Chrobok", .Copyright = "GPL 2 or 3", .PortCount = 4,
        .PortDescriptors = c_port_descriptors, .PortNames = c_port_names, .instantiate = instantiate, .connect_port = connect_port, .run =
                run, .cleanup = cleanup };

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0)
        return &c_ladspa_descriptor;
    return 0;
}

