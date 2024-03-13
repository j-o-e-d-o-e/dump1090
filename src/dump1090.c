// dump1090, a Mode S messages decoder for RTLSDR devices.
//
// Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//  *  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//
//  *  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#include "dump1090.h"
#include "interactive.h"
#include "mode_s.h"

struct programState Modes;

// ============================= Utility functions ==========================
void sigintHandler(int dummy) {
    MODES_NOTUSED(dummy);
    signal(SIGINT, SIG_DFL);     // reset signal handler - bit extra safety
    Modes.exit = 1;                         // Signal to threads that we are done
}

// =============================== Terminal handling ========================
// Get the number of rows after the terminal changes size.
int getTermRows(void) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return (w.ws_row);
}

// Handle resizing terminal
void sigWinchCallback(void) {
    signal(SIGWINCH, SIG_IGN);
    Modes.interactive_rows = getTermRows();
    interactiveShowData(Modes.aircrafts);
    signal(SIGWINCH, (__sighandler_t) sigWinchCallback);
}

// =============================== Initialization ===========================
void modesInitConfig(void) {
    memset(&Modes, 0, sizeof(Modes)); // Default everything to zero/NULL
    // Now initialise things that should not be 0/NULL to their defaults
    Modes.gain = MODES_MAX_GAIN;
    Modes.freq = MODES_DEFAULT_FREQ;
    Modes.ppm_error = MODES_DEFAULT_PPM;
    Modes.check_crc = 1;
    Modes.interactive_rows = getTermRows();
    Modes.interactive_delete_ttl = MODES_INTERACTIVE_DELETE_TTL;
    Modes.interactive_display_ttl = MODES_INTERACTIVE_DISPLAY_TTL;
    Modes.fUserLat = MODES_USER_LATITUDE_DFLT;
    Modes.fUserLon = MODES_USER_LONGITUDE_DFLT;
}

void modesInit(void) {
    pthread_mutex_init(&Modes.pDF_mutex, NULL);
    pthread_mutex_init(&Modes.data_mutex, NULL);
    pthread_cond_init(&Modes.data_cond, NULL);
    // Allocate the various buffers used by Modes
    Modes.icao_cache = (uint32_t *) malloc(sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2);
    Modes.pFileData = (uint16_t *) malloc(MODES_ASYNC_BUF_SIZE);
    Modes.magnitude = (uint16_t *) malloc(MODES_ASYNC_BUF_SIZE + MODES_PREAMBLE_SIZE + MODES_LONG_MSG_SIZE);
    Modes.maglut = (uint16_t *) malloc(sizeof(uint16_t) * 256 * 256);
    if (Modes.icao_cache == NULL || Modes.pFileData == NULL || Modes.magnitude == NULL || Modes.maglut == NULL) {
        fprintf(stderr, "Out of memory allocating data buffer.\n");
        exit(1);
    }
    // Clear the buffers that have just been allocated, just in-case
    memset(Modes.icao_cache, 0, sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2);
    memset(Modes.pFileData, 127, MODES_ASYNC_BUF_SIZE);
    memset(Modes.magnitude, 0, MODES_ASYNC_BUF_SIZE + MODES_PREAMBLE_SIZE + MODES_LONG_MSG_SIZE);

    Modes.bUserFlags &= ~MODES_USER_LATLON_VALID;
    if ((Modes.fUserLat != 0.0) || (Modes.fUserLon != 0.0)) Modes.bUserFlags |= MODES_USER_LATLON_VALID;
    gettimeofday(&Modes.stSystemTimeBlk, NULL); // Initialise the Block Timers to something half sensible
    for (int i = 0; i < MODES_ASYNC_BUF_NUMBER; i++) { Modes.stSystemTimeRTL[i] = Modes.stSystemTimeBlk; }
    for (int i = 0; i <= 255; i++) {
        for (int q = 0; q <= 255; q++) {
            int mag, mag_i, mag_q;
            mag_i = (i * 2) - 255;
            mag_q = (q * 2) - 255;
            mag = (int) round((sqrt((mag_i * mag_i) + (mag_q * mag_q)) * 258.433254) - 365.4798);
            Modes.maglut[(i * 256) + q] = (uint16_t) ((mag < 65535) ? mag : 65535);
        }
    }
    // Prepare error correction tables
    modesInitErrorInfo();
}

/* RTLSDR handling */
void modesInitRTLSDR(void) {
    uint32_t device_count = rtlsdr_get_device_count();
    if (!device_count) {
        fprintf(stderr, "No supported RTLSDR devices found.\n");
        exit(1);
    }
    char vendor[256], product[256], serial[256];
    rtlsdr_get_device_usb_strings(0, vendor, product, serial);
    if (rtlsdr_open(&Modes.dev, Modes.dev_index) < 0) {
        fprintf(stderr, "Error opening the RTLSDR device: %s\n", strerror(errno));
        exit(1);
    }
    // Set gain, frequency, sample rate, and reset the device
    rtlsdr_set_tuner_gain_mode(Modes.dev, (Modes.gain == MODES_AUTO_GAIN) ? 0 : 1);
    if (Modes.gain != MODES_AUTO_GAIN) {
        if (Modes.gain == MODES_MAX_GAIN) { // Find the maximum gain available
            int gains[100];
            int numgains = rtlsdr_get_tuner_gains(Modes.dev, gains);
            Modes.gain = gains[numgains - 1];
            // fprintf(stderr, "Max available gain is: %.2f\n", Modes.gain / 10.0);
        }
        rtlsdr_set_tuner_gain(Modes.dev, Modes.gain);
        // fprintf(stderr, "Setting gain to: %.2f\n", Modes.gain / 10.0);
    } else {
        fprintf(stderr, "Using automatic gain control.\n");
    }
    rtlsdr_set_freq_correction(Modes.dev, Modes.ppm_error);
    if (Modes.enable_agc) rtlsdr_set_agc_mode(Modes.dev, 1);
    rtlsdr_set_center_freq(Modes.dev, Modes.freq);
    rtlsdr_set_sample_rate(Modes.dev, MODES_DEFAULT_RATE);
    rtlsdr_reset_buffer(Modes.dev);
    // fprintf(stderr, "Gain reported by device: %.2f\n", rtlsdr_get_tuner_gain(Modes.dev) / 10.0);
}

// =========================== Threads ==============================
/* RTLSDR callback:
 We use a thread reading data in background, while the main thread
 handles decoding and visualization of data to the user.
 The reading thread calls the RTLSDR API to read data asynchronously, and
 uses a callback to populate the data buffer.

 A Mutex is used to avoid races with the decoding thread. */
void rtlsdrCallback(unsigned char *buf, uint32_t len, void *ctx) {
    MODES_NOTUSED(ctx);
    // Lock the data buffer variables before accessing them
    pthread_mutex_lock(&Modes.data_mutex);
    Modes.iDataIn &= (MODES_ASYNC_BUF_NUMBER - 1); // Just incase!!!
    gettimeofday(&Modes.stSystemTimeRTL[Modes.iDataIn], NULL); // Get the system time for this block
    if (len > MODES_ASYNC_BUF_SIZE) { len = MODES_ASYNC_BUF_SIZE; }
    // Queue the new data
    Modes.pData[Modes.iDataIn] = (uint16_t *) buf;
    Modes.iDataIn = (MODES_ASYNC_BUF_NUMBER - 1) & (Modes.iDataIn + 1);
    Modes.iDataReady = (MODES_ASYNC_BUF_NUMBER - 1) & (Modes.iDataIn - Modes.iDataOut);
    if (Modes.iDataReady == 0) {
        // Ooooops. We've just received the MODES_ASYNC_BUF_NUMBER'th outstanding buffer
        // This means that RTLSDR is currently overwriting the MODES_ASYNC_BUF_NUMBER+1
        // buffer, but we haven't yet processed it, so we're going to lose it. There
        // isn't much we can do to recover the lost data, but we can correct things to
        // avoid any additional problems.
        Modes.iDataOut = (MODES_ASYNC_BUF_NUMBER - 1) & (Modes.iDataOut + 1);
        Modes.iDataReady = (MODES_ASYNC_BUF_NUMBER - 1);
        Modes.iDataLost++;
    }
    // Signal to the other thread that new data is ready, and unlock
    pthread_cond_signal(&Modes.data_cond);
    pthread_mutex_unlock(&Modes.data_mutex);
}

/* We read data using a thread, so the main thread only handles decoding without caring about data acquisition */
void *readerThreadEntryPoint(void *arg) {
    MODES_NOTUSED(arg);
    rtlsdr_read_async(Modes.dev, rtlsdrCallback, NULL, MODES_ASYNC_BUF_NUMBER, MODES_ASYNC_BUF_SIZE);
    // Signal to the other thread that new data is ready - dummy really so threads don't mutually lock
    pthread_cond_signal(&Modes.data_cond);
    pthread_mutex_unlock(&Modes.data_mutex);
    pthread_exit(NULL);
}
