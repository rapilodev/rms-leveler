//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef window_h
#define window_h

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
    double adjustRate;
    double maxAmpChange;
    double amplification;
    double oldAmplification;
};

void freeWindow(struct Window* window) {
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

int initWindow(struct Window* window, int look_ahead, double duration, double rate, double max_change, double adjust_rate) {
    window->look_ahead = look_ahead;
	if (duration) {
		window->active = 1;
		window->duration = duration;
		window->dataSize = (unsigned long) (duration * rate);
		window->data = (LADSPA_Data*) calloc(window->dataSize, sizeof(LADSPA_Data));
		if (window->data == NULL) {
		    freeWindow(window);
		    return 0;
		}
		window->square = (double*) calloc(window->dataSize, sizeof(double));
        if (window->square == NULL) {
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

inline void prepareWindow(struct Window* window) {
	if (!window->active) return;
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
	if (!window->active) return;

	window->index += 1;
    if (window->index >= window->dataSize)
        window->index -= window->dataSize;

    window->playPosition += 1;
    if (window->playPosition >= window->dataSize)
        window->playPosition -= window->dataSize;

    window->adjustPosition += 1;
    if (window->adjustPosition >= window->adjustRate)
        window->adjustPosition -= window->adjustRate;

    window->position += window->deltaPosition;
}

inline double getWindowDcOffset(struct Window* window) {
    double dcOffset = window->sum / window->size;
    if ((dcOffset > -dcOffsetLimit) && (dcOffset < dcOffsetLimit))
        return 0.0;
    return dcOffset;
}

#endif
