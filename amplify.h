#ifndef amplify_h
#define amplify_h

#include <stdlib.h>
#include <ladspa.h>
#include <stdio.h>
#include <math.h>
#include "leveler.h"

const double maxChannels = 2.0;
// target loudness, should be -20 DB
const double TARGET_LOUDNESS = -20.0;
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

void calcWindowAmplification(struct Window* window, const int is_leveler) {
    window->oldRms = window->rms;
    window->rms = getRmsValue(window->sumSquare, window->size);

    window->oldAmplification = window->amplification;
    window->amplification = getAmplification(window->rms, window->oldRms, window->amplification);

    if (!is_leveler && window->amplification > 1.0 ) window->amplification = 1.0;
    double limit = window->oldAmplification + window->maxAmpChange;
    if (window->amplification > limit) {
        window->amplification = limit;
    }
}

inline double getWindowDcOffset(struct Window* window) {
    double dcOffset = window->sum / window->size;
    if ((dcOffset > -dcOffsetLimit) && (dcOffset < dcOffsetLimit))
        return 0.0;
    return dcOffset;
}

void printWindow(struct Window* window, int isLast) {
	fprintf(stderr, "%.1f\t%2.3f\t%2.3f\t%2.3f", window->position, window->rms, (TARGET_LOUDNESS - window->rms), window->amplification);
	if (isLast) {
		fprintf(stderr, "\n");
	} else {
		fprintf(stderr, "  ");
	}
}

#endif
