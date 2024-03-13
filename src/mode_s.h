#pragma once

void detectModeS(uint16_t *m, uint32_t mlen);

void decodeModesMessage(struct modesMessage *mm, unsigned char *msg);

void useModesMessage(struct modesMessage *mm);

void computeMagnitudeVector(uint16_t *pData);

int decodeCPR(struct aircraft *a, int fflag, int surface);

int decodeCPRrelative(struct aircraft *a, int fflag, int surface);

void modesInitErrorInfo(void);