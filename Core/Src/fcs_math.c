#include "fcs_math.h"
#include <math.h>
#include <string.h>

// [1] Internal Math Helpers
static double deg2rad(double deg) { return deg * DEG_TO_RAD; }
static double rad2deg(double rad) { return rad * RAD_TO_DEG; }

// [2] UTM to Lat/Lon Conversion
// Reference: TM Projection Formulas (WGS84)
void FCS_UTM_To_LatLon(UTM_Coord_t *utm, double *lat, double *lon) {
    double e = sqrt(1 - (WGS84_B * WGS84_B) / (WGS84_A * WGS84_A));
    double e_sq = e * e;
    
    double x = utm->easting - 500000.0;
    double y = utm->northing;
    
    // Southern Hemisphere Adjust (Not needed for Korea, but for completeness)
    // if (utm->band < 'N') y -= 10000000.0; 

    // M: Meridian Distance
    double m = y / UTM_K0;
    double mu = m / (WGS84_A * (1 - e_sq / 4 - 3 * e_sq * e_sq / 64)); // Approx
    
    // Footprint Latitude (phi1)
    double phi1 = mu + (3 * e / 2 - 27 * e * e * e / 32) * sin(2 * mu); // Simplified
    
    // ... For full accuracy, standard TM inverse series should be used.
    // For portfolio demo, we assume relatively small range or Use High Precision logic if needed.
    // Here we implement standard series expansion for robustness.
    
    // Re-calc e1 for series
    double e1 = (1 - sqrt(1 - e_sq)) / (1 + sqrt(1 - e_sq));
    
    // Calculate Footprint Latitude (Accurate)
    double J1 = (3*e1/2 - 27*e1*e1*e1/32);
    double J2 = (21*e1*e1/16 - 55*e1*e1*e1*e1/32);
    double J3 = (151*e1*e1*e1/96);
    
    phi1 = mu + J1*sin(2*mu) + J2*sin(4*mu) + J3*sin(6*mu);

    double C1 = e_sq * cos(phi1) * cos(phi1) / (1 - e_sq);
    double T1 = tan(phi1) * tan(phi1);
    double N1 = WGS84_A / sqrt(1 - e_sq * sin(phi1) * sin(phi1));
    double R1 = WGS84_A * (1 - e_sq) / pow(1 - e_sq * sin(phi1) * sin(phi1), 1.5);
    double D = x / (N1 * UTM_K0);

    // Calculate Latitude
    double lat_rad = phi1 - (N1 * tan(phi1) / R1) * (D * D / 2 - (5 + 3 * T1 + 10 * C1 - 4 * C1 * C1 - 9 * e_sq) * D * D * D * D / 24);
    
    // Calculate Longitude
    double lon_rad = (D - (1 + 2 * T1 + C1) * D * D * D / 6 + (5 - 2 * C1 + 28 * T1 - 3 * C1 * C1 + 8 * e_sq + 24 * T1 * T1) * D * D * D * D * D / 120) / cos(phi1);
    
    // Zone CM (Central Meridian)
    // Zone 52 = 129E, Zone 51 = 123E
    double lon_origin = (utm->zone - 1) * 6 - 180 + 3; 
    
    *lat = rad2deg(lat_rad);
    *lon = lon_origin + rad2deg(lon_rad);
}

// [3] Lat/Lon to UTM Conversion (Supports Forced Zone)
void FCS_LatLon_To_UTM(double lat, double lon, uint8_t force_zone, UTM_Coord_t *utm) {
    // 1. Determine Zone if not forced
    if (force_zone == 0) {
        utm->zone = (uint8_t)((lon + 180.0) / 6.0) + 1;
    } else {
        utm->zone = force_zone;
    }
    
    // Latitude Band (Simple Check)
    if (lat >= 40.0) utm->band = 'T'; // Korea T band (North)
    else utm->band = 'S';             // Korea S band (South)

    // 2. Math Setup
    double lat_rad = deg2rad(lat);
    double lon_rad = deg2rad(lon);
    
    double lon_origin = (utm->zone - 1) * 6 - 180 + 3; 
    double lon_origin_rad = deg2rad(lon_origin);

    double e = sqrt(1 - (WGS84_B * WGS84_B) / (WGS84_A * WGS84_A));
    double e_sq = e * e;

    double N = WGS84_A / sqrt(1 - e_sq * sin(lat_rad) * sin(lat_rad));
    double T = tan(lat_rad) * tan(lat_rad);
    double C = e_sq * cos(lat_rad) * cos(lat_rad) / (1 - e_sq);
    double A = (lon_rad - lon_origin_rad) * cos(lat_rad);

    double M = WGS84_A * ((1 - e_sq / 4 - 3 * e_sq * e_sq / 64 - 5 * e_sq * e_sq * e_sq / 256) * lat_rad
                - (3 * e_sq / 8 + 3 * e_sq * e_sq / 32 + 45 * e_sq * e_sq * e_sq / 1024) * sin(2 * lat_rad)
                + (15 * e_sq * e_sq / 256 + 45 * e_sq * e_sq * e_sq / 1024) * sin(4 * lat_rad)
                - (35 * e_sq * e_sq * e_sq / 3072) * sin(6 * lat_rad));

    double easting = UTM_K0 * N * (A + (1 - T + C) * A * A * A / 6
                    + (5 - 18 * T + T * T + 72 * C - 58 * e_sq) * A * A * A * A * A / 120) + 500000.0;

    double northing = UTM_K0 * (M + N * tan(lat_rad) * (A * A / 2
                    + (5 - T + 9 * C + 4 * C * C) * A * A * A * A / 24
                    + (61 - 58 * T + T * T + 600 * C - 330 * e_sq) * A * A * A * A * A * A / 720));

    // Southern Hemisphere Check (Not for Korea, but standard)
    // if (lat < 0) northing += 10000000.0;

    utm->easting = easting;
    utm->northing = northing;
}

// [4] Main Calculation Logic (Cross-Zone Support)
void FCS_Calculate_FireData(FCS_System_t *sys) {
    UTM_Coord_t *batt = &sys->user_pos;
    UTM_Coord_t *tgt  = &sys->tgt_pos;
    
    // Internal Target Coordinate (Projected to Battery Zone)
    UTM_Coord_t tgt_proj; 
    
    // 1. Zone Check
    if (batt->zone == tgt->zone) {
        // Same Zone: Direct Copy
        tgt_proj = *tgt;
    } else {
        // Diff Zone: Reproject Target to Battery Zone (Key Algo)
        double lat, lon;
        // 1) Convert Target(original) -> Lat/Lon
        FCS_UTM_To_LatLon(tgt, &lat, &lon);
        // 2) Convert Lat/Lon -> UTM(Battery Zone)
        FCS_LatLon_To_UTM(lat, lon, batt->zone, &tgt_proj);
        // Now tgt_proj has weird easting (e.g., -5000 or 1050000) but it is mathematically aligned with Battery.
    }
    
    // 2. Calculate Distance (2D Euclidean in Projected Plane)
    double dx = tgt_proj.easting - batt->easting;
    double dy = tgt_proj.northing - batt->northing;
    double dist_m = sqrt(dx*dx + dy*dy);
    sys->fire.distance_km = (float)(dist_m / 1000.0);
    
    // 3. Calculate Azimuth (Grid Azimuth)
    // atan2 returns radians between -PI and PI
    double az_rad = atan2(dx, dy); 
    double az_deg = rad2deg(az_rad);
    if (az_deg < 0) az_deg += 360.0;
    
    // Convert Degree to Mil (6400 mil = 360 deg)
    // 1 deg = 17.777... mil
    sys->fire.azimuth = (float)(az_deg * (6400.0 / 360.0));
    
    // 4. Calculate Elevation (Basic Ballistics - Vacuum Model for now)
    // Need a real table later. For now, using Simple Parabolic approx:
    // R = (v^2 * sin(2*theta)) / g
    // sin(2*theta) = (R * g) / v^2
    // Let's assume M109A6 equiv: V0 ~ 560 m/s (Charge 4)
    double g = 9.81;
    double v0 = 560.0; // m/s
    
    double val = (dist_m * g) / (v0 * v0);
    if (val > 1.0) val = 1.0; // Out of range clamp
    
    double el_rad = 0.5 * asin(val);
    double el_deg = rad2deg(el_rad);
    sys->fire.elevation = (float)(el_deg * (6400.0 / 360.0));
    
    // Alt Correction (Low precision approx)
    // +10m height diff ~= +1 mil (Rule of thumb)
    float dh = tgt->altitude - batt->altitude;
    sys->fire.elevation += (dh / 10.0f); 
}
