#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include "dump1090.h"
#include "connect.h"
#include "interactive.h"
#include "modes/mode_s.h"
#include "utils/stats.h"

void showHelp(void) {
    printf("-----------------------------------------------------------------------------\n"
           "|                        dump1090 ModeS Receiver         Ver : " MODES_DUMP1090_VERSION " |\n"
           "-----------------------------------------------------------------------------\n"
           "--gain <db>              Set gain (default: max gain. Use -10 for auto-gain)\n"
           "--enable-agc             Enable the Automatic Gain Control (default: off)\n"
           "--quiet                  Disable output to stdout. Use for daemon applications\n"
           "--help                   Show this help\n"
    );
}

void handleArguments(int argc, char **argv) {
    for (int j = 1; j < argc; j++) { // Parse the command line options
        int more = j + 1 < argc; // There are more arguments
        if (!strcmp(argv[j], "--gain") && more) {
            char *error;
            Modes.gain = (int) (strtol(argv[++j], &error, 10));
        } else if (!strcmp(argv[j], "--enable-agc")) {
            Modes.enable_agc++;
        } else if (!strcmp(argv[j], "--help")) {
            showHelp();
            exit(0);
        } else if (!strcmp(argv[j], "--quiet")) {
            Modes.quiet = 1;
        } else {
            fprintf(stderr, "Unknown or not enough arguments for option '%s'.\n\n", argv[j]);
            exit(1);
        }
    }
}

void initializeComponents(void) {
    modesInitConfig();
    signal(SIGINT, sigintHandler); // Define Ctrl/C handler (exit program)
    signal(SIGWINCH, (__sighandler_t) sigWinchCallback); // Setup for SIGWINCH for handling lines
    modesInit(); // memory & mutex
    modesInitRTLSDR(); // device
    pthread_create(&Modes.reader_thread, NULL, readerThreadEntryPoint,
                   NULL); // Create thread that will read the data from the device
    Modes.metric = 1;
    Modes.interactive = 1;
    curl_global_init(CURL_GLOBAL_ALL);
}

void processData(void) {
    pthread_mutex_lock(&Modes.data_mutex);
    while (Modes.exit == 0) {
        if (Modes.iDataReady == 0) {
            // This unlocks Modes.data_mutex, and waits for Modes.data_cond
            // Once (Modes.data_cond) occurs, it locks Modes.data_mutex
            pthread_cond_wait(&Modes.data_cond, &Modes.data_mutex);
            continue;
        }
        // Modes.data_mutex is Locked, and (Modes.iDataReady != 0)
        if (Modes.iDataReady) { // Check we have new data, just in case!!
            Modes.iDataOut &= (MODES_ASYNC_BUF_NUMBER - 1); // Just incase
            // Translate the next lot of I/Q samples into Modes.magnitude
            computeMagnitudeVector(Modes.pData[Modes.iDataOut]);

            Modes.stSystemTimeBlk = Modes.stSystemTimeRTL[Modes.iDataOut];
            // Update the input buffer pointer queue
            Modes.iDataOut = (MODES_ASYNC_BUF_NUMBER - 1) & (Modes.iDataOut + 1);
            Modes.iDataReady = (MODES_ASYNC_BUF_NUMBER - 1) & (Modes.iDataIn - Modes.iDataOut);
            // If we lost some blocks, correct the timestamp
            if (Modes.iDataLost) {
                Modes.timestampBlk += (MODES_ASYNC_BUF_SAMPLES * 6 * Modes.iDataLost);
                Modes.stat_blocks_dropped += Modes.iDataLost;
                Modes.iDataLost = 0;
            }
            // It's safe to release the lock now
            pthread_cond_signal(&Modes.data_cond);
            pthread_mutex_unlock(&Modes.data_mutex);
            // Process data after releasing the lock, so that the capturing
            // thread can read data while we perform computationally expensive
            // stuff at the same time.
            detectModeS(Modes.magnitude, MODES_ASYNC_BUF_SAMPLES);
            // Update the timestamp ready for the next block
            Modes.timestampBlk += (MODES_ASYNC_BUF_SAMPLES * 6);
            Modes.stat_blocks_processed++;
        } else {
            pthread_cond_signal(&Modes.data_cond);
            pthread_mutex_unlock(&Modes.data_mutex);
        }
        if (Modes.aircrafts) interactiveRemoveStaleAircraft(Modes.aircrafts);
        interactiveShowData(Modes.aircrafts);
        pthread_mutex_lock(&Modes.data_mutex);

        time_t now = time(NULL);
        struct tm *date_time = localtime(&now);
        if (date_time->tm_hour == 0 && date_time->tm_min == 0) {
            Modes.flag_send = 0;
            Modes.flag_photos_clean_up = 0;
            resetStatsTotal();
        }
#ifdef __arm__
        if (!Modes.flag_photos_clean_up && date_time->tm_hour == 0 && date_time->tm_min == 30){
            Modes.flag_photos_clean_up = 1;
            cleanUpPhotos();
        }
#endif
        if (!Modes.flag_send && date_time->tm_hour == 5 && date_time->tm_min == 20) {
            char *json = readFromFile(now);
            unIdleServer();
            Data *data = httpPostJson(json, now);
            free(json);
            if (data != NULL && data->len > 0) httpPostPhotos(data, now);
            free(data);
            Modes.flag_send = 1;
            printf("\x1b[H\x1b[2J"); // Clear the screen
        }
    }
}

void closeComponents(void) {
    curl_global_cleanup();
    rtlsdr_cancel_async(Modes.dev); // Cancel rtlsdr_read_async will cause data input thread to terminate cleanly
    rtlsdr_close(Modes.dev);
    pthread_cond_destroy(&Modes.data_cond); // Thread cleanup
    pthread_mutex_destroy(&Modes.data_mutex);
    pthread_join(Modes.reader_thread, NULL); // Wait on reader thread exit
    pthread_exit(0);
}

int main(int argc, char **argv) {
    handleArguments(argc, argv);
    initializeComponents();
    atexit(closeComponents);
    setStatsStartTime();
    processData();
    closeComponents();
}
