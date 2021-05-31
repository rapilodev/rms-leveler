#include <stdlib.h> 
#include <ladspa.h>

#include <stdio.h>
#include <math.h>

/*
 * LADSPA plugin to level stereo audio to -20dBFS with -1dBFS head.
 * use a 6 second lookahead window.
 * level both channels independently.
 * limit amplification at -40dB
 * do not increase amplification on decreasing loudness
 * interpolate amplification
 * test with audacity or
 * liquidsoap 'out(ladspa.rms_leveler2(mksafe(audio_to_stereo(input.http("http://ice.rosebud-media.de:8000/88vier-low")))))'
 * Milan Chrobok, 2016
 */

const int debug = 0;
const double maxChannels = 2;

// target rms level, should be -20 DB
const int TARGET_RMS = -20.0;
// long term measurement window
const unsigned int BUFFER_DURATION = 10;
// compression start = -3dB
const double compressionStart = 0.707945784;
// maximum level = -1dB
const double MAX_LEVEL = 0.891250938;
// amplitude limit to what DC offset is not removed
const double offsetLimit = 0.0001;
// minimum level = -40dB
const double MIN_RMS = -40;

const double maxAmpChangeUp = 0.05;

// channel indexes
#define IN_LEFT    0
#define IN_RIGHT   1
#define OUT_LEFT   2
#define OUT_RIGHT  3

struct Channel {
    LADSPA_Data* in;
    LADSPA_Data* out;
    LADSPA_Data* data;
    double* square;

    double amplification;
    double oldAmplification;
    double oldAmplificationSmoothed;
    double sumSquare;
    double sum;
    double rms;
};

// define our handler type
typedef struct {
    struct Channel* channels[2];
    struct Channel left;
    struct Channel right;

    unsigned long size;
    double position;
    unsigned long index;
    unsigned long adjustPosition;
    unsigned long rate;
    unsigned long bufferSize;
} Leveler;

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    Leveler * h = malloc(sizeof(Leveler));

    h->channels[0] = &h->left;
    h->channels[1] = &h->right;
    h->left.out = NULL;
    h->right.out = NULL;
    h->left.in = NULL;
    h->right.in = NULL;

    h->size = 0;
    h->rate = rate;
    h->bufferSize = BUFFER_DURATION * h->rate;
    h->position = 0.0;
    h->index = 0;
    h->adjustPosition = 0;

    int i = 0;
    for (i = 0; i < maxChannels; i++) {
        struct Channel* channel = h->channels[i];
        channel->sumSquare = 0;
        channel->sum = 0;
        channel->amplification = 1.0;
        channel->oldAmplification = 1.0;
        channel->oldAmplificationSmoothed = 1.0;
        channel->rms = 0.0;
        channel->data = (LADSPA_Data*) calloc(h->bufferSize, sizeof(LADSPA_Data));
        channel->square = (double*) calloc(h->bufferSize, sizeof(double));
    }
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    //fprintf(stderr, "cleanup\n");
    Leveler * h = (Leveler *) handle;
    free(h->left.data);
    free(h->right.data);
    free(h->left.square);
    free(h->right.square);
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

inline double getAmplification(const double rms, const double oldRms, const double oldAmp) {
    //if (rms < MIN_RMS) rms = MIN_RMS;
    if (rms < MIN_RMS)
        return oldAmp;

    // get target factor from rms delta
    double amp = pow(10., (TARGET_RMS - rms) / 20.0);
    if (isnan(amp))
        amp = 1.;
    if (amp == 0.)
        amp = 1.;

    // on decreasing loudness do not increase amplification
    if ((amp > 1.0) && (rms <= oldRms))
        amp = oldAmp;
    //if (rms <= oldRms) amp = oldAmp;

    return amp;
}

inline double interpolateAmplification(const double amp, const double oldAmp, unsigned long pos, const unsigned long maxPos) {
    if (pos > maxPos)
        pos = maxPos;
    if (pos < 0)
        pos = 0;
    double proportion = (double) pos / maxPos;
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

static void run(LADSPA_Handle handle, unsigned long samples) {
    Leveler * h = (Leveler *) handle;

    //if (debug==1)
    //fprintf(stderr, "run %lu samples, buffer=%d, rate=%lu\n", samples, h->bufferSize, h->rate);

    const double deltaPosition = 1.0 / h->rate;
    const unsigned long adjustRate = (int) (h->rate) / 2;

    unsigned long playPosition = h->index + h->bufferSize / 2;
    if (playPosition >= h->bufferSize)
        playPosition -= h->bufferSize;

    unsigned long s;
    for (s = 0; s < samples; s++) {
        //on start increase buffer size
        if (h->size < h->bufferSize)
            h->size++;

        int i = 0;
        for (i = 0; i < maxChannels; i++) {
            struct Channel* channel = h->channels[i];

            // sum up left and right values from input
            channel->sum -= channel->data[h->index];
            if (channel->in != NULL) {
                channel->data[h->index] = channel->in[s];
            } else {
                channel->data[h->index] = 0;
            }
            channel->sum += channel->data[h->index];

            double offset = channel->sum / h->size;
            if ((offset > -offsetLimit) && (offset < offsetLimit))
                offset = 0.0;

            // direct value
            //double ampFactor = channel->amplification;

            // interpolate with shifted adjust position
            double ampFactor = interpolateAmplification(channel->amplification, channel->oldAmplification, h->adjustPosition, adjustRate);

            // average
            //double smoothRate= (h->rate) / 5.;
            //double ampFactor2 = (  (smoothRate-1) * channel->oldAmplificationSmoothed + channel->amplification) / smoothRate;
            //channel->oldAmplificationSmoothed = ampFactor2;
            //ampFactor = (ampFactor + ampFactor2 ) / 2.;

            // read with delay, amplify and limit
            double value = (channel->data[playPosition] - offset) * ampFactor;
            value = limit(value);

            // output delayed value
            if (channel->out != NULL) {
                channel->out[s] = (LADSPA_Data) value;
            }

            // sum value RMS over index
            value = channel->data[h->index];
            channel->sumSquare -= channel->square[h->index];
            channel->square[h->index] = value * value;
            channel->sumSquare += channel->square[h->index];

            // calculate amplification from RMS values
            if (h->adjustPosition == 0) {
                double oldAmp = channel->amplification;
                channel->oldAmplification = channel->amplification;
                double rms = getRmsValue(channel->sumSquare, h->size);
                channel->amplification = getAmplification(rms, channel->rms, channel->amplification);

                if (channel->amplification > oldAmp + maxAmpChangeUp) {
                    channel->amplification = oldAmp + maxAmpChangeUp;
                }
                channel->rms = rms;
                if (debug == 1) {
                    fprintf(stderr, "%lu\t%2.3f\t%2.3f\t%2.3f\n", (unsigned long) h->position, rms, (TARGET_RMS - rms),
                            channel->amplification);
                }
            }
        }

        // increase position counters
        playPosition++;
        if (playPosition >= h->bufferSize)
            playPosition -= h->bufferSize;

        h->index++;
        if (h->index >= h->bufferSize)
            h->index -= h->bufferSize;

        h->adjustPosition++;
        if (h->adjustPosition >= adjustRate)
            h->adjustPosition -= adjustRate;

        h->position += deltaPosition;

    }
}

static const char * c_port_names[4] = { "Input - Left Channel", "Input - Right Channel", "Output - Left Channel", "Output - Right Channel" };

static LADSPA_PortDescriptor c_port_descriptors[4] = { LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
        LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT };

static const LADSPA_PortRangeHint psPortRangeHints[4] = {
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 }
};

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b3e5, .Label = "rms_leveler_10s", .Name =
        "RMS leveler -20dBFS, stereo, 10 seconds window", .Maker = "Milan Chrobok", .Copyright = "GPL 2 or 3", .PortCount = 4,
        .PortDescriptors = c_port_descriptors, .PortNames = c_port_names, .instantiate = instantiate, .connect_port = connect_port, .run =
                run, .cleanup = cleanup, .PortRangeHints = psPortRangeHints };

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0)
        return &c_ladspa_descriptor;
    return 0;
}

