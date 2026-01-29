//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef amplify_h
#define amplify_h

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// target loudness, should be -20 DB
const double TARGET_LOUDNESS = -20.0;
// how often update statistics
const double ADJUST_RATE = 0.333;
// compression start = -3dB
const double compressionStart = 0.707945784;
// maximum level = -1dB
const double MAX_LEVEL = 0.891250938;
// minimum loudness = -40dB =0.01
const double MIN_LOUDNESS = -40.0;
// max amplication change per second
const double MAX_CHANGE = 0.7;

const double DB3 = sqrt(2);

inline double getDb(double a) {
    if (a == 0.0) a = 0.0000000000001;
    return 20.0 * log10(sqrt(a));
}

inline double getRmsValue(const double rmsSum, const double size) {
    double sum = rmsSum / size;
    return getDb(sum);
}

inline double getAmplification(const double loudness, const double oldLoudness, const double oldAmp) {
    if (loudness < MIN_LOUDNESS) return oldAmp;
    // get target factor from loudness delta
    double amp = pow(10., (TARGET_LOUDNESS - loudness) / 20.0);
    if (isnan(amp)) amp = 1.;
    if (amp == 0.) amp = 1.;
    // on decreasing loudness do not increase amplification
    if ((amp > 1.0) && (loudness <= oldLoudness)) amp = oldAmp;
    return amp;
}

inline double interpolateAmplification(const double amp, const double oldAmp, double pos, const double maxPos) {
    if (pos > maxPos) pos = maxPos;
    if (pos < 0)      pos = 0;
    if (amp == oldAmp || pos == maxPos) return amp;
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
    if (value < 0.0) amplitude = -amplitude;
    if (amplitude > compressionStart)
        amplitude = compressionStart + compress(amplitude - compressionStart);
    if (value < 0.0) amplitude = -amplitude;

    if (amplitude > MAX_LEVEL)  amplitude = MAX_LEVEL;
    if (amplitude < -MAX_LEVEL) amplitude = -MAX_LEVEL;
    return amplitude;
}

#endif
