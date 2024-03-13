#pragma once

// http://bboxfinder.com/#51.320000,6.810000,51.390000,7.010000
#define MIN_LAT 51.32
#define MAX_LAT 51.39

#define MIN_LON 6.81
#define MAX_LON 7.01
// https://www.movable-type.co.uk/scripts/latlong.html
// Point 1: 51.382, 6.964 (PLANE_EXAMPLE), Point2: 51.381, 6.961 (CAM)
// Distance: 236m / 3 => 0.001°=78.667=~75m (OR ~80m?)
#define SET_LON 6.966

#define MIN_HDG 210
#define MAX_HDG 250

#define MIN_ALT 600
#define MAX_ALT 1500

#define MIN_SPD 180

#define CACHE_LEN 20

void interactiveShowData(struct aircraft *a);

struct aircraft *interactiveReceiveData(struct modesMessage *mm);

void interactiveRemoveStaleAircraft(struct aircraft *a);
