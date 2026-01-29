//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef window_h
#define window_h
#include <ladspa.h>
#include "amplify.h"

static inline unsigned long wrap_at(unsigned long val, unsigned long limit) {
    while (val >= limit) val -= limit;
    return val;
}
// amplitude limit to what DC offset is not removed
const double dcOffsetLimit = 0.005;

struct Window {
    int active;
    int look_ahead;
    double duration;
    unsigned long size;
    unsigned long dataSize;
    LADSPA_Data* data;
    double* square;
    double sumSquare;
    double sum;
    double loudness;
    double oldLoudness;
    double position;
    double deltaPosition;
    unsigned long index;
    unsigned long adjustPosition;
    unsigned long playPosition;
    unsigned long adjustRate;
    double maxAmpChange;
    double amplification;
    double oldAmplification;
};

static inline void freeWindow(struct Window* window) {
    if (window == NULL) return;
    if (window->data != NULL) {
        free(window->data);
        window->data = NULL;
    }
    if (window->square != NULL) {
        free(window->square);
        window->square = NULL;
    }
}

static int inline initWindow(struct Window* window, int look_ahead, double duration, double rate, double max_change, double adjust_rate) {
    if (window == NULL) return 0;
    freeWindow(window);
    window->look_ahead = look_ahead;
    window->data = NULL;
    window->square = NULL;
    if (duration > 0) {
        window->active = 1;
        window->duration = duration;
        window->dataSize = (unsigned long) (duration * rate);
        window->data = (LADSPA_Data*) calloc(window->dataSize, sizeof(LADSPA_Data));
        window->square = (double*) calloc(window->dataSize, sizeof(double));
        if (!window->data || !window->square) {
            freeWindow(window);
            return 0;
        }
    }
    window->sum = 0;
    window->sumSquare = 0;
    window->loudness = 0.0;
    window->oldLoudness = 0.0;
    window->size = 0;
    window->position = 0.0;
    window->index = 0;
    window->adjustPosition = 0;
    window->adjustRate = (int) ( rate * adjust_rate ) ;
    window->maxAmpChange = max_change * adjust_rate;
    window->deltaPosition = 1.0 / rate;
    window->amplification = 1.0;
    window->oldAmplification = 1.0;
    return 1;
}

inline void addWindowData(struct Window* window, LADSPA_Data value) {
    if (!window->active) return;
    window->sum -= window->data[window->index];
    window->data[window->index] = value;
    window->sum += window->data[window->index];
}

inline void sumWindowData(struct Window* window) {
    if (!window->active) return;
    double value = window->data[window->index];
    window->sumSquare -= window->square[window->index];
    window->square[window->index] = value * value;
    window->sumSquare += window->square[window->index];
}

inline static void prepareWindow(struct Window* window) {
    if (!window->active) return;
    //increase buffer size on start
    if (window->size < window->dataSize)
        window->size++;

    window->playPosition = wrap_at(window->index + window->dataSize / 2, window->dataSize);
    window->index = wrap_at(window->index, window->dataSize);
}

inline static void moveWindow(struct Window* window) {
    if (!window->active) return;

    window->index          = wrap_at(++window->index, window->dataSize);
    window->playPosition   = wrap_at(++window->playPosition, window->dataSize);
    window->adjustPosition = wrap_at(++window->adjustPosition, window->adjustRate);
#ifdef DEBUG
    window->position += window->deltaPosition;
#endif
}

inline double getWindowDcOffset(struct Window* window) {
    if (window->size == 0) return 0.0;
    double dcOffset = window->sum / window->size;
    if ((dcOffset > -dcOffsetLimit) && (dcOffset < dcOffsetLimit))
        return 0.0;
    return dcOffset;
}

void calcWindowAmplification(struct Window* window, double loudness, const int IS_LEVELER, const double input_gain) {
    window->oldLoudness = window->loudness;
    window->loudness = loudness;
    window->oldAmplification = window->amplification;
    const int IS_LIMITER = !IS_LEVELER;
    if (IS_LIMITER) {
        if(window->loudness <= TARGET_LOUDNESS - 3) {
            // Compensate 3dB if limiter is idle
            window->amplification = DB3 * 1.0 / input_gain;
        } else {
            // Active Leveling/Limiting
            window->amplification = getAmplification(window->loudness, window->oldLoudness, window->amplification);
            // Hard Ceiling for Limiter mode
            if (IS_LIMITER && window->amplification > 1.0 ) {
                window->amplification = 1.0;
            }
        }
    }
    if (IS_LEVELER) {
        if (window->loudness < MIN_LOUDNESS) {
            // Compensate 3dB if leveler is gated
            window->amplification = DB3 * 1.0 / input_gain;
        } else {
            // Active Leveling/Limiting
            window->amplification = getAmplification(window->loudness, window->oldLoudness, window->amplification);
            // Constraints (Slew Rate / Max Change)
            double maxAllowed = window->oldAmplification + window->maxAmpChange;
            if (window->amplification > maxAllowed) {
                window->amplification = maxAllowed;
            }
        }
    }
}

void printWindow(struct Window* window, int isLast) {
    fprintf(stderr, "%.1f\t%2.3f\t%2.3f\t%2.3f", window->position, window->loudness, (TARGET_LOUDNESS - window->loudness), window->amplification);
    if (isLast) {
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr, "  ");
    }
}


#endif
