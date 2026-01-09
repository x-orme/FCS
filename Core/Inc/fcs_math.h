#ifndef FCS_MATH_H
#define FCS_MATH_H

#include "fcs_common.h" // For UTM_Coord_t, FCS_System_t

// WGS84 Constants
#define WGS84_A         6378137.0
#define WGS84_F         (1.0 / 298.257223563)
#define WGS84_B         (WGS84_A * (1.0 - WGS84_F))
#define UTM_K0          0.9996

// Math Constants
#define PI              3.14159265358979323846
#define RAD_TO_DEG      (180.0 / PI)
#define DEG_TO_RAD      (PI / 180.0)

// Function Prototypes
void FCS_Math_Init(void);
void FCS_Calculate_FireData(FCS_System_t *sys);
void FCS_UTM_To_LatLon(UTM_Coord_t *utm, double *lat, double *lon);
void FCS_LatLon_To_UTM(double lat, double lon, uint8_t force_zone, UTM_Coord_t *utm);

#endif
