#pragma once

int detectModeA(uint16_t *m, struct modesMessage *mm);

int ModeAToModeC(unsigned int ModeA);

void decodeModeAMessage(struct modesMessage *mm, int ModeA);
