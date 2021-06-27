#include <stdlib.h> 
#include <ladspa.h>

#include <stdio.h>
#include <math.h>

/*
 * LADSPA plugin to level stereo audio to -20dBFS with -1dBFS head.
 * use a 3 second lookahead window.
 * level both channels independently.
 * limit amplification at -40dB
 * do not increase amplification on decreasing loudness
 * interpolate amplification
 * test with audacity or
 * liquidsoap 'out(ladspa.rms_leveler2(mksafe(audio_to_stereo(input.http("http://ice.rosebud-media.de:8000/88vier-low")))))'
 * Milan Chrobok, 2016
 */

//#define debug
// target rms level, should be -20 DB
const int TARGET_RMS = -20.0;

// long term measurement window
const unsigned int BUFFER_DURATION = 6;

// compression start = -3dB
const double compressionStart = 0.707945784;

// maximum level = -1dB
const double MAX_LEVEL = 0.891250938;

// amplitude limit to what DC offset is not removed
const double offsetLimit = 0.0001;

// minimum level = -36dB
const double MIN_RMS = -40;

// channel indexes
#define IN_LEFT    0
#define IN_RIGHT   1
#define OUT_LEFT   2
#define OUT_RIGHT  3

// define our handler type
typedef struct {
    LADSPA_Data* ports[4];

    LADSPA_Data* left;
    LADSPA_Data* right;

    double* leftSquare;
    double* rightSquare;

    double amplificationLeft;
    double amplificationRight;

    double oldAmplificationLeft;
    double oldAmplificationRight;

    double sumSquareLeft;
    double sumSquareRight;

    double sumLeft;
    double sumRight;

    double rmsLeft;
    double rmsRight;

    unsigned long size;
    double position;
    unsigned long index;
    unsigned long adjustPosition;
    unsigned long rate;
    unsigned long bufferSize;
} leveler;

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    leveler * h = malloc(sizeof(leveler));

    //fprintf(stderr, "instantiate\n");

    h->size = 0;
    h->rate = rate;
    h->bufferSize = BUFFER_DURATION * h->rate;
    h->position = 0.0;
    h->index = 0;
    h->adjustPosition = 0;

    h->sumSquareLeft = 0;
    h->sumSquareRight = 0;

    h->sumLeft = 0;
    h->sumRight = 0;

    h->amplificationLeft = 1.0;
    h->amplificationRight = 1.0;

    h->oldAmplificationLeft = 1.0;
    h->oldAmplificationRight = 1.0;

    h->rmsLeft = 0.0;
    h->rmsRight = 0.0;

    h->left = (LADSPA_Data*) calloc(h->bufferSize, sizeof(LADSPA_Data));
    h->right = (LADSPA_Data*) calloc(h->bufferSize, sizeof(LADSPA_Data));

    h->leftSquare = (double*) calloc(h->bufferSize, sizeof(double));
    h->rightSquare = (double*) calloc(h->bufferSize, sizeof(double));

    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    //fprintf(stderr, "cleanup\n");
    leveler * h = (leveler *) handle;
    free(h->left);
    free(h->right);
    free(h->leftSquare);
    free(h->rightSquare);
    free(handle);
}

static void connect_port(const LADSPA_Handle handle, unsigned long num, LADSPA_Data * port) {
    leveler * h = (leveler *) handle;
    h->ports[num] = port;
}

inline double getRmsValue(const double rmsSum, const double size) {
    double sum = rmsSum / size;
    if (sum == 0.0) sum = 0.0000000000001;
    return 20.0 * log10(sqrt(sum));
}

inline double getAmplification(const double rms, const double oldRms, const double oldAmp) {
    //if (rms < MIN_RMS) rms = MIN_RMS;
    if (rms < MIN_RMS) return oldAmp;

    // get target factor from rms delta
    double amp = pow(10., (TARGET_RMS - rms) / 20.0);
    if (isnan(amp)) amp = 1.;
    if (amp == 0.) amp = 1.;

    // on decreasing loudness do not increase amplification
    if ((amp > 1.0) && (rms <= oldRms)) amp = oldAmp;
    //if (rms <= oldRms) amp = oldAmp;

    return amp;
}

inline double interpolateAmplification(const double amp, const double oldAmp, unsigned long pos,
    const unsigned long maxPos) {
    if (pos > maxPos) pos = maxPos;
    if (pos < 0) pos = 0;
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
    if (value < 0.0) amplitude = -amplitude;
    if (amplitude > compressionStart) {
        amplitude = compressionStart + compress(amplitude - compressionStart);
    }
    if (value < 0.0) amplitude = -amplitude;

    if (amplitude > MAX_LEVEL) amplitude = MAX_LEVEL;
    if (amplitude < -MAX_LEVEL) amplitude = -MAX_LEVEL;
    return amplitude;
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    leveler * h = (leveler *) handle;

    //if (debug==1)
    //fprintf(stderr, "run %lu samples, buffer=%d, rate=%lu\n", samples, h->bufferSize, h->rate);

    const double deltaPosition = 1.0 / h->rate;
    const unsigned long adjustRate = (int) (h->rate) / 3;

    unsigned long playPosition = h->index + h->bufferSize / 2;
    if (playPosition >= h->bufferSize) playPosition -= h->bufferSize;

    unsigned long s;
    for (s = 0; s < samples; s++) {
        //on start increase buffer size
        if (h->size < h->bufferSize) h->size++;

        // sum up left and right values from input
        h->sumLeft -= h->left[h->index];
        h->left[h->index] = h->ports[IN_LEFT][s];
        h->sumLeft += h->left[h->index];

        h->sumRight -= h->right[h->index];
        h->right[h->index] = h->ports[IN_RIGHT][s];
        h->sumRight += h->right[h->index];

        double offsetLeft = h->sumLeft / h->size;
        if ((offsetLeft > -offsetLimit) && (offsetLeft < offsetLimit)) offsetLeft = 0.0;

        double offsetRight = h->sumRight / h->size;
        if ((offsetRight > -offsetLimit) && (offsetRight < offsetLimit)) offsetRight = 0.0;

        //if (h->index % 10000 == 0)
        //    fprintf(stderr, "pos=%lu\tindex=%lu\tsize=%lu\tsumLeft=%f\tsumRight=%f\toffsetLeft=%f\toffsetRight=%f\n",
        //        (unsigned long) h->position, (unsigned long) h->index, (unsigned long) h->size, h->sumLeft, h->sumRight,
        //        offsetLeft, offsetRight);

        // interpolate with shifted adjust position
        double ampFactorLeft = interpolateAmplification(h->amplificationLeft, h->oldAmplificationLeft,
            h->adjustPosition, adjustRate);
        // read with delay, amplify and limit
        double left = (h->left[playPosition] - offsetLeft) * ampFactorLeft;
        left = limit(left);

        // interpolate with shifted adjust position
        double ampFactorRight = interpolateAmplification(h->amplificationRight, h->oldAmplificationRight,
            h->adjustPosition, adjustRate);
        // read with delay, amplify and limit
        double right = (h->right[playPosition] - offsetRight) * ampFactorRight;
        right = limit(right);

        // output delayed value
        h->ports[OUT_LEFT][s] = (LADSPA_Data) left;
        h->ports[OUT_RIGHT][s] = (LADSPA_Data) right;

        // sum left RMS over index
        left = h->left[h->index]; // * h->amplificationLeft;
        h->sumSquareLeft -= h->leftSquare[h->index];
        h->leftSquare[h->index] = left * left;
        h->sumSquareLeft += h->leftSquare[h->index];

        // sum right RMS over index
        right = h->right[h->index]; // * h->amplificationRight;
        h->sumSquareRight -= h->rightSquare[h->index];
        h->rightSquare[h->index] = right * right;
        h->sumSquareRight += h->rightSquare[h->index];

        //fprintf(stderr, "%lu %lu %lu %lu\n", (long)h->position, h->index, s , playPosition);

        // calculate amplification from RMS values
        if (h->adjustPosition == 0) {
            h->oldAmplificationLeft = h->amplificationLeft;
            double rmsLeft = getRmsValue(h->sumSquareLeft, h->size);
            h->amplificationLeft = getAmplification(rmsLeft, h->rmsLeft, h->amplificationLeft);
            if (h->amplificationLeft > 1.0) h->amplificationLeft = 1.0;
            h->rmsLeft = rmsLeft;

            h->oldAmplificationRight = h->amplificationRight;
            double rmsRight = getRmsValue(h->sumSquareRight, h->size);
            h->amplificationRight = getAmplification(rmsRight, h->rmsRight, h->amplificationRight);
            if (h->amplificationRight > 1.0) h->amplificationRight = 1.0;
            h->rmsRight = rmsRight;

            //if (debug == 1)
#ifdef debug
            fprintf(stderr, "%lu\t%2.3f %2.3f\t%2.3f %2.3f\t%2.3f %2.3f\n", (unsigned long) h->position, rmsLeft,
                rmsRight, (TARGET_RMS - rmsLeft), (TARGET_RMS - rmsRight), h->amplificationLeft, h->amplificationRight);
#endif

        }

        // increase position counters

        playPosition++;
        if (playPosition >= h->bufferSize) playPosition -= h->bufferSize;

        h->index++;
        if (h->index >= h->bufferSize) h->index -= h->bufferSize;

        h->adjustPosition++;
        if (h->adjustPosition >= adjustRate) h->adjustPosition -= adjustRate;

        h->position += deltaPosition;

    }

    //setLevels(h);
}

static const char * c_port_names[4] = { "Input - Left Channel", "Input - Right Channel", "Output - Left Channel",
    "Output - Right Channel" };

static LADSPA_PortDescriptor c_port_descriptors[4] = { LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT, LADSPA_PORT_AUDIO
    | LADSPA_PORT_INPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT };

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b3e5, .Label = "rms_limiter_6s", .Name =
    "RMS limiter -20dBFS, stereo, 6 seconds window", .Maker = "Milan Chrobok", .Copyright = "GPL 2 or 3",
    .PortCount = 4, .PortDescriptors = c_port_descriptors, .PortNames = c_port_names, .instantiate = instantiate,
    .connect_port = connect_port, .run = run, .cleanup = cleanup };

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0) return &c_ladspa_descriptor;
    return 0;
}

