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

#include "dump1090.h"
#include "interactive.h"
#include "modes/mode_ac.h"
#include "modes/mode_s.h"
#include "connect.h"
#include "utils/stats.h"

// ============================= Utility functions ==========================
static uint64_t mstime(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

//========================== Validate ====================================
// Check if aircraft invalid
int invalid(struct aircraft *a) {
    return a->altitude < MIN_ALT || a->altitude > MAX_ALT
           || a->track < MIN_HDG || a->track > MAX_HDG
           || a->lat < MIN_LAT || a->lat > MAX_LAT
           || a->lon < MIN_LON || a->lon > MAX_LON
           || a->speed < MIN_SPD;
}

//======================== Receive data (by mode_s) =================================
struct aircraft *findAircraft(uint32_t addr);

struct aircraft *createAircraft(struct modesMessage *mm);

void createDF(struct aircraft *a, struct modesMessage *mm);

// Receive new messages and populate the interactive mode with more info
struct aircraft *interactiveReceiveData(struct modesMessage *mm) {
    // Return if (checking crc) AND (not crcok) AND (not fixed)
    if (Modes.check_crc && (mm->crcok == 0) && (mm->correctedbits == 0)) return NULL;
    // Lookup our aircraft or create a new one
    struct aircraft *a = findAircraft(mm->addr);
    if (!a) { // If it's a currently unknown aircraft, create a new record for it
        a = createAircraft(mm);
        a->next = Modes.aircrafts; // ... and put it at the head of the list
        Modes.aircrafts = a;
    }
    a->signalLevel[a->messages & 7] = mm->signalLevel;// replace the 8th oldest signal strength
    a->seen = time(NULL);
    a->timestamp = mm->timestampMsg;
    a->messages++;
    // If a (new) CALLSIGN has been received, copy it to the aircraft structure
    if (mm->bFlags & MODES_ACFLAGS_CALLSIGN_VALID) memcpy(a->flight, mm->flight, sizeof(a->flight));
    // If a (new) ALTITUDE has been received, copy it to the aircraft structure
    if (mm->bFlags & MODES_ACFLAGS_ALTITUDE_VALID) {
        // if we've a modeCcount already and altitude has changed
        int altitude = (int) (mm->altitude / 3.2828);
        if ((a->modeCcount) && (a->altitude != altitude)) {
            a->modeCcount = 0;               // ... zero the hit count
            a->modeACflags &= ~MODEAC_MSG_MODEC_HIT;
        }
        a->altitude = altitude;
        a->modeC = (mm->altitude + 49) / 100;
    }
    // If a (new) SQUAWK has been received, copy it to the aircraft structure
    if (mm->bFlags & MODES_ACFLAGS_SQUAWK_VALID) {
        if (a->modeA != mm->modeA) {
            a->modeAcount = 0; // Squawk has changed, so zero the hit count
            a->modeACflags &= ~MODEAC_MSG_MODEA_HIT;
        }
        a->modeA = mm->modeA;
    }
    // If a (new) HEADING has been received, copy it to the aircraft structure
    if (mm->bFlags & MODES_ACFLAGS_HEADING_VALID) a->track = mm->heading;
    // If a (new) SPEED has been received, copy it to the aircraft structure
    if (mm->bFlags & MODES_ACFLAGS_SPEED_VALID) a->speed = (int) (mm->velocity * 1.852);
    // If a (new) Vertical Descent rate has been received, copy it to the aircraft structure
    if (mm->bFlags & MODES_ACFLAGS_VERTRATE_VALID) a->vert_rate = mm->vert_rate;
    // if the Aircraft has landed or taken off since the last message, clear the even/odd CPR flags
    if ((mm->bFlags & MODES_ACFLAGS_AOG_VALID) && ((a->bFlags ^ mm->bFlags) & MODES_ACFLAGS_AOG)) {
        a->bFlags &= ~(MODES_ACFLAGS_LLBOTH_VALID | MODES_ACFLAGS_AOG);
    }
    // If we've got a new cprlat or cprlon
    if (mm->bFlags & MODES_ACFLAGS_LLEITHER_VALID) {
        int location_ok = 0;
        if (mm->bFlags & MODES_ACFLAGS_LLODD_VALID) {
            a->odd_cprlat = mm->raw_latitude;
            a->odd_cprlon = mm->raw_longitude;
            a->odd_cprtime = mstime();
        } else {
            a->even_cprlat = mm->raw_latitude;
            a->even_cprlon = mm->raw_longitude;
            a->even_cprtime = mstime();
        }
        // If we have enough recent data, try global CPR: LAT and LON
        if (((mm->bFlags | a->bFlags) & MODES_ACFLAGS_LLEITHER_VALID) == MODES_ACFLAGS_LLBOTH_VALID
            && abs((int) (a->even_cprtime - a->odd_cprtime)) <= 10000) {
            if (decodeCPR(a, (mm->bFlags & MODES_ACFLAGS_LLODD_VALID), (mm->bFlags & MODES_ACFLAGS_AOG)) == 0) {
                location_ok = 1;
            }
        }
        // Otherwise try relative CPR.
        if (!location_ok &&
            decodeCPRrelative(a, (mm->bFlags & MODES_ACFLAGS_LLODD_VALID), (mm->bFlags & MODES_ACFLAGS_AOG)) == 0) {
            location_ok = 1;
        }
        //If we successfully decoded, back copy the results to mm so that we can print them in list output
        if (location_ok) {
            mm->bFlags |= MODES_ACFLAGS_LATLON_VALID;
            mm->fLat = a->lat;
            mm->fLon = a->lon;
        }
    }
    // Update aircraft a->bFlags to reflect the newly received mm->bFlags;
    a->bFlags |= mm->bFlags;
    if (mm->msgtype == 32) {
        int flags = a->modeACflags;
        if ((flags & (MODEAC_MSG_MODEC_HIT | MODEAC_MSG_MODEC_OLD)) == MODEAC_MSG_MODEC_OLD) {
            // This Mode-C doesn't currently hit any known Mode-S, but it used to because MODEAC_MSG_MODEC_OLD is
            // set  So the aircraft it used to match has either changed altitude, or gone out of our receiver range
            //
            // We've now received this Mode-A/C again, so it must be a new aircraft. It could be another aircraft
            // at the same Mode-C altitude, or it could be a new airctraft with a new Mods-A squawk.
            //
            // To avoid masking this aircraft from the interactive display, clear the MODEAC_MSG_MODES_OLD flag
            // and set messages to 1;
            a->modeACflags = flags & ~MODEAC_MSG_MODEC_OLD;
            a->messages = 1;
        }
    }
    // If we are Logging DF's, and it's not a Mode A/C
    if ((Modes.bEnableDFLogging) && (mm->msgtype < 32)) createDF(a, mm);
    return (a);
}

// Return the aircraft with the specified address, or NULL if no aircraft exists with this address.
struct aircraft *findAircraft(uint32_t addr) {
    struct aircraft *a = Modes.aircrafts;
    while (a) {
        if (a->addr == addr) return a;
        a = a->next;
    }
    return NULL;
}

// Return a new aircraft structure for the interactive mode linked list of aircraft
struct aircraft *createAircraft(struct modesMessage *mm) {
    struct aircraft *a = (struct aircraft *) malloc(sizeof(*a));
    memset(a, 0, sizeof(*a)); // Default everything to 0/NULL
    a->addr = mm->addr; // Initialise things that should not be 0/NULL to their defaults
    a->lat = a->lon = 0.0;
    memset(a->signalLevel, mm->signalLevel, 8); // First time, initialise everything to the first signal strength
    // mm->msgtype 32 is used to represent Mode A/C. These values can never change, so
    // set them once here during initialisation, and don't bother to set them every
    // time this ModeA/C is received again in the future
    if (mm->msgtype == 32) {
        int modeC = ModeAToModeC(mm->modeA | mm->fs);
        a->modeACflags = MODEAC_MSG_FLAG;
        if (modeC < -12) {
            a->modeACflags |= MODEAC_MSG_MODEA_ONLY;
        } else {
            mm->altitude = modeC * 100;
            mm->bFlags |= MODES_ACFLAGS_ALTITUDE_VALID;
        }
    }
    return a;
}

// Add a new DF structure to the interactive mode linked list
void createDF(struct aircraft *a, struct modesMessage *mm) {
    struct stDF *pDF = (struct stDF *) malloc(sizeof(*pDF));
    if (pDF) {
        memset(pDF, 0, sizeof(*pDF)); // Default everything to zero/NULL
        pDF->seen = a->seen; // Now initialise things
        pDF->llTimestamp = mm->timestampMsg;
        pDF->addr = mm->addr;
        pDF->pAircraft = a;
        memcpy(pDF->msg, mm->msg, MODES_LONG_MSG_BYTES);
        if (!pthread_mutex_lock(&Modes.pDF_mutex)) {
            if ((pDF->pNext = Modes.pDF)) Modes.pDF->pPrev = pDF;
            Modes.pDF = pDF;
            pthread_mutex_unlock(&Modes.pDF_mutex);
        } else {
            free(pDF);
        }
    }
}

//=============================== Update ===================================
// We have received a Mode A or C response.
void interactiveUpdateAircraftModeA(struct aircraft *a) {
    struct aircraft *b = Modes.aircrafts;
    while (b) {
        if ((b->modeACflags & MODEAC_MSG_FLAG) == 0) {// skip any fudged ICAO records
            // If both (a) and (b) have valid squawks...
            if ((a->bFlags & b->bFlags) & MODES_ACFLAGS_SQUAWK_VALID) {
                // ...check for Mode-A == Mode-S Squawk matches
                if (a->modeA == b->modeA) { // If a 'real' Mode-S ICAO exists using this Mode-A Squawk
                    b->modeAcount = a->messages;
                    b->modeACflags |= MODEAC_MSG_MODEA_HIT;
                    a->modeACflags |= MODEAC_MSG_MODEA_HIT;
                    if ((b->modeAcount > 0) &&
                        ((b->modeCcount > 1)
                         || (a->modeACflags &
                             MODEAC_MSG_MODEA_ONLY))) // Allow Mode-A only matches if this Mode-A is invalid Mode-C
                    { a->modeACflags |= MODEAC_MSG_MODES_HIT; }    // flag this ModeA/C probably belongs to a known Mode S
                }
            }
            // If both (a) and (b) have valid altitudes...
            if ((a->bFlags & b->bFlags) & MODES_ACFLAGS_ALTITUDE_VALID) {
                // ... check for Mode-C == Mode-S Altitude matches
                if ((a->modeC == b->modeC)     // If a 'real' Mode-S ICAO exists at this Mode-C Altitude
                    || (a->modeC == b->modeC + 1)     //          or this Mode-C - 100 ft
                    || (a->modeC + 1 == b->modeC)) { //          or this Mode-C + 100 ft
                    b->modeCcount = a->messages;
                    b->modeACflags |= MODEAC_MSG_MODEC_HIT;
                    a->modeACflags |= MODEAC_MSG_MODEC_HIT;
                    if ((b->modeAcount > 0) &&
                        (b->modeCcount > 1)) {
                        a->modeACflags |= (MODEAC_MSG_MODES_HIT | MODEAC_MSG_MODEC_OLD);
                    } // flag this ModeA/C probably belongs to a known Mode S
                }
            }
        }
        b = b->next;
    }
}

void interactiveUpdateAircraftModeS(void) {
    struct aircraft *a = Modes.aircrafts;
    while (a) {
        int flags = a->modeACflags;
        if (flags & MODEAC_MSG_FLAG) { // find any fudged ICAO records
            // clear the current A,C and S hit bits ready for this attempt
            a->modeACflags = flags & ~(MODEAC_MSG_MODEA_HIT | MODEAC_MSG_MODEC_HIT | MODEAC_MSG_MODES_HIT);
            interactiveUpdateAircraftModeA(a);  // and attempt to match them with Mode-S
        }
        a = a->next;
    }
}

//===========================================================================
// Check if aircraft address has already been spotted
unsigned char isNewAddr(uint32_t addr) {
    static uint32_t addresses[CACHE_LEN];
    static int index = 0;
    for (int i = 0; i < CACHE_LEN; i++) if (addr == addresses[i]) return 0;
    if (index == CACHE_LEN) index = 0;
    addresses[index] = addr;
    index++;
    return 1;
}

//======================= Start interactive =================================
// Show the currently captured interactive data on screen.
void interactiveShowData(struct aircraft *a) {
    // Refresh screen every (MODES_INTERACTIVE_REFRESH_TIME) milliseconds
    if ((mstime() - Modes.interactive_last_update) < MODES_INTERACTIVE_REFRESH_TIME) return;
    Modes.interactive_last_update = mstime();
    // Attempt to reconcile any ModeA/C with known Mode-S
    // We can't condition on Modes.modeac because ModeA/C could be coming
    // in from a raw input port which we can't turn off.
    interactiveUpdateAircraftModeS();

    printf("\x1b[H\x1b[2J");    // Clear the screen
    time_t now = time(NULL);
    printStats(now);
    char spinner[4] = "|/-\\";
    char progress = spinner[now % 4];
    printf("------------------------------------------------------------------------------------\n");
    printf("ICAO    Mode  Sqwk  Callsign   Alt  Spd  Hdg   Lat       Lon    Sig  Msgs   Ti  Til%c\n", progress);
    printf("------------------------------------------------------------------------------------\n");

    int count = 0;
    while (a && (count < Modes.interactive_rows)) {
        if ((now - a->seen) < Modes.interactive_display_ttl) {
            if (invalid(a)) {
                a = a->next;
                continue;
            }
            if (strlen(a->flight) != 0 && a->lon < SET_LON && isNewAddr(a->addr)) {
                increaseTotal();
                char json[JSON_MAX_LEN];
                aircraftToJson(a, json);
                writeJsonToFile(json, now);
#ifdef __arm__ // raspi
                takePhoto(a, now);
#endif
            }
            long msgs = a->messages;
            int flags = a->modeACflags;
            if ((((flags & (MODEAC_MSG_FLAG)) == 0)) ||
                (((flags & (MODEAC_MSG_MODES_HIT | MODEAC_MSG_MODEA_ONLY)) == MODEAC_MSG_MODEA_ONLY) && (msgs > 4))
                || (((flags & (MODEAC_MSG_MODES_HIT | MODEAC_MSG_MODEC_OLD)) == 0) && (msgs > 127))) {

                char strMode[5] = " ";
                if ((flags & MODEAC_MSG_FLAG) == 0) strMode[0] = 'S';
                else if (flags & MODEAC_MSG_MODEA_ONLY) strMode[0] = 'A';
                if (flags & MODEAC_MSG_MODEA_HIT) strMode[2] = 'a';
                if (flags & MODEAC_MSG_MODEC_HIT) strMode[3] = 'c';

                char strSquawk[5] = " ";
                if (a->bFlags & MODES_ACFLAGS_SQUAWK_VALID) snprintf(strSquawk, 5, "%04x", a->modeA);

                char strAlt[6] = " ";
                if (a->bFlags & MODES_ACFLAGS_AOG) snprintf(strAlt, 6, " grnd");
                else if (a->bFlags & MODES_ACFLAGS_ALTITUDE_VALID) snprintf(strAlt, 6, "%5d", a->altitude);

                char strSpd[5] = " ";
                if (a->bFlags & MODES_ACFLAGS_SPEED_VALID) snprintf(strSpd, 5, "%3d", a->speed);

                char strHdg[5] = " ";
                if (a->bFlags & MODES_ACFLAGS_HEADING_VALID) snprintf(strHdg, 5, "%03d", a->track);

                char strLat[8] = " ";
                char strLon[9] = " ";
                if (a->bFlags & MODES_ACFLAGS_LATLON_VALID) {
                    snprintf(strLat, 8, "%7.03f", a->lat);
                    snprintf(strLon, 9, "%8.03f", a->lon);
                }
                unsigned char *pSig = a->signalLevel;
                unsigned int signalAverage =
                        (pSig[0] + pSig[1] + pSig[2] + pSig[3] + pSig[4] + pSig[5] + pSig[6] + pSig[7] + 3) >> 3;
                if (msgs > 99999) msgs = 99999;
                int ti = (int) (now - a->seen);
                int til = (int) (now - a->seenLatLon);
                printf("%06X  %-4s  %-4s  %-8s %5s  %3s  %3s  %7s %8s  %3d %5ld   %2d   %2d\n",
                       a->addr, strMode, strSquawk, a->flight, strAlt, strSpd, strHdg,
                       strLat, strLon, signalAverage, msgs, ti, til);
                count++;
            }
        }
        a = a->next;
    }
}

//========================= Remove =======================================
// Remove stale DF's from the interactive mode linked list
// Only fiddle with the DF list if we gain possession of the mutex
// If we fail to get the mutex we'll get another chance to tidy the
// DF list in a second or so.
void interactiveRemoveStaleDF(time_t now) {
    if (pthread_mutex_trylock(&Modes.pDF_mutex)) return;
    struct stDF *prev = NULL;
    struct stDF *pDF = Modes.pDF;
    while (pDF) {
        if ((now - pDF->seen) > Modes.interactive_delete_ttl) {
            if (Modes.pDF == pDF) Modes.pDF = NULL;
            else if (prev != NULL) prev->pNext = NULL;
            // All DF's in the list from here onwards will be time expired, so delete them all
            while (pDF) {
                prev = pDF;
                pDF = pDF->pNext;
                free(prev);
                prev = NULL;
            }
        } else {
            prev = pDF;
            pDF = pDF->pNext;
        }
    }
    pthread_mutex_unlock(&Modes.pDF_mutex);
}

// When in interactive mode If we don't receive new messages within
// MODES_INTERACTIVE_DELETE_TTL seconds we remove the aircraft from the list.
void interactiveRemoveStaleAircraft(struct aircraft *a) {
    time_t now = time(NULL);
    if (Modes.last_cleanup_time == now) return; // Only do cleanup once per second
    struct aircraft *prev = NULL;
    Modes.last_cleanup_time = now;
    interactiveRemoveStaleDF(now);
    while (a) {
        if ((now - a->seen) > Modes.interactive_delete_ttl) {
            // Remove the element from the linked list, with care
            // if we are removing the first element
            if (!prev) {
                Modes.aircrafts = a->next;
                free(a);
                a = Modes.aircrafts;
            } else {
                prev->next = a->next;
                free(a);
                a = prev->next;
            }
        } else {
            prev = a;
            a = a->next;
        }
    }
}
