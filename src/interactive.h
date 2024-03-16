#pragma once

#define CACHE_LEN 20

void interactiveShowData(struct aircraft *a);

struct aircraft *interactiveReceiveData(struct modesMessage *mm);

void interactiveRemoveStaleAircraft(struct aircraft *a);
