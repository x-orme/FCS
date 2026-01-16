#include "fcs_math.h"
#include <math.h>
#include <string.h>

// [1] Internal Math Helpers
static double deg2rad(double deg) { return deg * DEG_TO_RAD; }
static double rad2deg(double rad) { return rad * RAD_TO_DEG; }

// =========================================================================================
// [SIMULATED FIRING TABLE DATABASE]
// Weapon: K105A1 (105mm), M1 HE Projectile
// Charges: 1 to 7 (Tables approximated based on standard 105mm ballistics)
// =========================================================================================
// Error Codes for Elevation
#define FCS_ERR_RANGE   -1.0f // Exceeds Weapon Max Range
#define FCS_ERR_CHARGE  -2.0f // Exceeds Current Charge Limit

typedef struct {
    float range_m;
    float elev_mil;
    float drift_mil; // Right Drift
    float c_factor;  // Site Factor (mil/10m)
} FiringTable_Row_t;

// ... (Table Data Updates - Keeping structure but refining ranges) ...

// --- Charge 1 (1.0 ~ 3.0 km) ---
static const FiringTable_Row_t FT_Ch1[] = {
  {  1000.0f,  100.0f,  0.5f,  6.0f },
  {  2000.0f,  320.0f,  2.1f,  3.8f },
  {  3000.0f,  800.0f,  5.2f,  2.8f } // Max 3.0km (~800mil)
};
#define SZ_Ch1 (sizeof(FT_Ch1)/sizeof(FT_Ch1[0]))

// --- Charge 2 (1.5 ~ 4.0 km) ---
static const FiringTable_Row_t FT_Ch2[] = {
  {  1500.0f,  110.0f,  1.0f,  4.5f },
  {  3000.0f,  400.0f,  3.2f,  3.0f },
  {  4000.0f,  800.0f,  5.8f,  2.2f } // Max 4.0km
};
#define SZ_Ch2 (sizeof(FT_Ch2)/sizeof(FT_Ch2[0]))

// --- Charge 3 (2.0 ~ 5.5 km) ---
static const FiringTable_Row_t FT_Ch3[] = {
  {  2000.0f,  120.0f,  1.2f,  4.8f },
  {  4000.0f,  420.0f,  4.0f,  3.2f },
  {  5500.0f,  800.0f,  6.0f,  2.0f } // Max 5.5km
};
#define SZ_Ch3 (sizeof(FT_Ch3)/sizeof(FT_Ch3[0]))

// --- Charge 4 (3.0 ~ 7.0 km) ---
static const FiringTable_Row_t FT_Ch4[] = {
  {  3000.0f,  150.0f,  2.0f,  3.8f },
  {  5000.0f,  450.0f,  4.8f,  2.3f },
  {  7000.0f,  800.0f,  9.5f,  1.8f } // Max 7.0km
};
#define SZ_Ch4 (sizeof(FT_Ch4)/sizeof(FT_Ch4[0]))

// --- Charge 5 (4.0 ~ 8.5 km) ---
static const FiringTable_Row_t FT_Ch5[] = {
  {  4000.0f,  180.0f,  2.5f,  3.2f },
  {  6500.0f,  480.0f,  5.0f,  2.4f },
  {  8500.0f,  800.0f,  9.0f,  1.7f } // Max 8.5km
};
#define SZ_Ch5 (sizeof(FT_Ch5)/sizeof(FT_Ch5[0]))

// --- Charge 6 (5.0 ~ 10.0 km) ---
static const FiringTable_Row_t FT_Ch6[] = {
  {  5000.0f,  190.0f,  2.8f,  2.9f },
  {  7500.0f,  510.0f,  5.8f,  2.0f },
  { 10000.0f,  800.0f, 11.5f,  1.3f } // Max 10.0km
};
#define SZ_Ch6 (sizeof(FT_Ch6)/sizeof(FT_Ch6[0]))

// --- Charge 7 (6.0 ~ 11.3 km) ---
static const FiringTable_Row_t FT_Ch7[] = {
  {  6000.0f,  195.0f,  2.9f,  2.2f },
  {  9000.0f,  530.0f,  5.2f,  1.7f },
  { 11300.0f,  800.0f, 12.0f,  1.1f } // Max 11.3km
};
#define SZ_Ch7 (sizeof(FT_Ch7)/sizeof(FT_Ch7[0]))
// ... (rest of file) ...


// Table Registry
// Index 0 is dummy, Index 1-7 corresponds to Charge #
static const FiringTable_Row_t* FT_DB[8] = {
  NULL, FT_Ch1, FT_Ch2, FT_Ch3, FT_Ch4, FT_Ch5, FT_Ch6, FT_Ch7
};
static const int FT_Sizes[8] = {
  0, SZ_Ch1, SZ_Ch2, SZ_Ch3, SZ_Ch4, SZ_Ch5, SZ_Ch6, SZ_Ch7
};

// Linear Interpolation
static float L_Interp(float x1, float y1, float x2, float y2, float x) {
  if (x2 == x1) return y1;
  return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

// =========================================================================================
// [2] UTM to Lat/Lon Conversion (WGS84)`
// =========================================================================================
void FCS_UTM_To_LatLon(UTM_Coord_t *utm, double *lat, double *lon) {
  double e = sqrt(1 - (WGS84_B * WGS84_B) / (WGS84_A * WGS84_A));
  double e_sq = e * e;
    
  double x = utm->easting - 500000.0;
  double y = utm->northing;
    
  // Southern Hemisphere Adjust (Not needed for Korea)
  // if (utm->band < 'N') y -= 10000000.0; 

  double m = y / UTM_K0;
  double mu = m / (WGS84_A * (1 - e_sq / 4 - 3 * e_sq * e_sq / 64)); 
    
  double e1 = (1 - sqrt(1 - e_sq)) / (1 + sqrt(1 - e_sq));
    
  double J1 = (3*e1/2 - 27*e1*e1*e1/32);
  double J2 = (21*e1*e1/16 - 55*e1*e1*e1*e1/32);
  double J3 = (151*e1*e1*e1/96);
    
  double phi1 = mu + J1*sin(2*mu) + J2*sin(4*mu) + J3*sin(6*mu);

  double C1 = e_sq * cos(phi1) * cos(phi1) / (1 - e_sq);
  double T1 = tan(phi1) * tan(phi1);
  double N1 = WGS84_A / sqrt(1 - e_sq * sin(phi1) * sin(phi1));
  double R1 = WGS84_A * (1 - e_sq) / pow(1 - e_sq * sin(phi1) * sin(phi1), 1.5);
  double D = x / (N1 * UTM_K0);

  double lat_rad = phi1 - (N1 * tan(phi1) / R1) * (D * D / 2 - (5 + 3 * T1 + 10 * C1 - 4 * C1 * C1 - 9 * e_sq) * D * D * D * D / 24);
    
  double lon_rad = (D - (1 + 2 * T1 + C1) * D * D * D / 6 + (5 - 2 * C1 + 28 * T1 - 3 * C1 * C1 + 8 * e_sq + 24 * T1 * T1) * D * D * D * D * D / 120) / cos(phi1);
    
  double lon_origin = (utm->zone - 1) * 6 - 180 + 3; 
    
  *lat = rad2deg(lat_rad);
  *lon = lon_origin + rad2deg(lon_rad);
}

// =========================================================================================
// [3] Lat/Lon to UTM Conversion
// =========================================================================================
void FCS_LatLon_To_UTM(double lat, double lon, uint8_t force_zone, UTM_Coord_t *utm) {
  if (force_zone == 0) {
    utm->zone = (uint8_t)((lon + 180.0) / 6.0) + 1;
  } else {
    utm->zone = force_zone;
  }
    
  if (lat >= 40.0) utm->band = 'T'; 
  else utm->band = 'S';             

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

  utm->easting = easting;
  utm->northing = northing;
}

// =========================================================================================
// [4] Main Ballistic Logic (Standard Artillery Procedures)
// =========================================================================================

// =========================================================================================
// [4] Main Ballistic Logic (Standard Artillery Procedures)
// =========================================================================================

void FCS_Calculate_FireData(FCS_System_t *sys) {
  UTM_Coord_t *batt = &sys->user_pos;
  UTM_Coord_t *tgt  = &sys->tgt_pos;
    
  // --- Step 1: Map Data Calculation (Geodetic) ---
  UTM_Coord_t tgt_proj; 
    
  // Zone Reprojection Logic
  if (batt->zone == tgt->zone) {
    tgt_proj = *tgt;
  } else {
    double lat, lon;
    FCS_UTM_To_LatLon(tgt, &lat, &lon);
    FCS_LatLon_To_UTM(lat, lon, batt->zone, &tgt_proj);
  }
    
  double dx = tgt_proj.easting - batt->easting;
  double dy = tgt_proj.northing - batt->northing;
  double map_dist_m = sqrt(dx*dx + dy*dy);
    
  double az_rad = atan2(dx, dy); 
  double map_az_deg = rad2deg(az_rad);
  if (map_az_deg < 0) map_az_deg += 360.0;
    
  float map_az_mil = (float)(map_az_deg * (6400.0 / 360.0));
  float vi_m = tgt->altitude - batt->altitude; // Vertical Interval (+: Up, -: Down)

  // --- Step 2: Corrections (Met + Velocity + Rotation) ---
  // [Placeholder Logic for Met/Vel Corrections]
  // Standard Temp: 15C (288K), Standard Pressure: 1013hPa
  // Standard Prop Temp: 21C
    
  float corr_dist_m = 0.0f;
    
  // A. Met Correction (Air Density)
  // Low density (High Temp/Low Press) -> Shell goes further -> Need to AIM SHORTER (Corr is Negative to range? No, Range increases, so we assume target is closer? 
  // Wait. "Corrected Range" is the range entry we look up in the table.
  // If shell flies better (+eff), we look up a SHORTER range entry to hit the actual target.
  // So: High Temp -> Range Efficiency > 100% -> Corrected Range < Map Range.
    
  float temp_diff = sys->env.air_temp - 15.0f;
  // Approx: +10C -> +1% Range specific effect -> -1% Correction
  corr_dist_m -= (map_dist_m * 0.01f * (temp_diff / 10.0f));
    
  // B. Propellant Temp Correction
  float prop_diff = sys->env.prop_temp - 21.0f;
  // Approx: +10C -> +2% Range -> -2% Correction
  corr_dist_m -= (map_dist_m * 0.02f * (prop_diff / 10.0f));

  // C. Wind Correction (Head/Tail) 
  // Data from BMP280 does NOT include wind. 
  // Assuming 0 wind speed unless manually input (TODO for future update)
  // float wind_eff = 0.0f; 
    
  // (Simplification: Just applying to Corrected Range var)
  // [Adjustment Applied Here]
  // Add user adjustment (range_m) to the calculated Map Range
  float final_lookup_range = map_dist_m + corr_dist_m + (float)sys->adj.range_m;
    
  // Clamp Range
  if (final_lookup_range < 0) final_lookup_range = 100.0f;

  // --- Coriolis Effect Correction (Earth Rotation) ---
  // Depends on Latitude, Azimuth, and Time of Flight (TOF)
  // 1. Get Latitude (phi)
  double lat_c, lon_c;
  FCS_UTM_To_LatLon(tgt, &lat_c, &lon_c);
  double lat_rad = deg2rad(lat_c);
    
  // 2. Approx Time of Flight (tof)
  // Heuristic: TOF ~ Range / Avg_Velocity (Assume ~300m/s for indirect fire arc)
  double tof = map_dist_m / 300.0; 
    
  // 3. Earth Rotation Rate (Omega) - Unused directly in simplified formula
  // double omega = 0.00007292; // rad/s
    
  // 4. Calculate Corrections (Standard Approx Formulas)
  // Range Effect: Delta X = -2 * Omega * TOF * V_x * sin(Lat) * sin(Az) ... complicated 
  // Simplified Table-like Factors for 105mm:
  // Range Corr (m) ~= 0.02 * Range(km) * TOF * sin(Lat) * sin(Az) (Just a heuristic model)
  // Azimuth Corr (mil) ~= (2 * Omega * TOF * (cos(Lat) * tan(Elev) * sin(Az) - sin(Lat))) ...
    
  // We will use a simplified "Linear" model often used in training simulators:
  // Range: + depends on firing East (+), West (-)
  // Azimuth: Right drift in N.Hemisphere
    
  // (A) Range Correction due to Rotation (lag_range)
  // Firing East(90deg) -> Target moves away -> Fall short -> Need + Correction? 
  // Actually, Earth rotates East. Target (East) moves away. Shell (Incr Inertia) moves East too.
  // Let's use standard table approximation: 
  // Factor ~ 0.5 m / km / sec_tof * sin(Lat) * sin(Az)
  double cor_range = 0.5 * (map_dist_m/1000.0) * tof * sin(lat_rad) * sin(az_rad);
  // Apply to Corrected Range (Inverse sign: if current falls short, we look up shorter range? No, we need more range)
  // Here we just add to the geometric range for lookup.
  final_lookup_range += cor_range;

  // (B) Azimuth Correction (Coriolis Drift)
  // N.Hemisphere: Deflects Simple Right (Clockwise)
  // Factor: ~ 0.1 mil * TOF * sin(Lat)
  double cor_az_mil = 0.1 * tof * sin(lat_rad);
  // Add to map azimuth (Right deflection means we need to aim Left? No, Deflection is added to Azimuth to hit)
  // If shell drifts Right, we must aim Left. So Azimuth -= Correction.
  // Wait, "Drift" in step 5 is added. Drift is usually Right (Spin). We correct by aiming Left? 
  // Standard Firing Data: Deflection = Azimuth - TotalDrift. 
  // But here sys->fire.azimuth means "Command Azimuth" (Firing Azimuth).
  // If drift moves projectile Right, we must aim Left. So Command = MapAz - Drift.
  // Let's assume Drift variable in table is "Correction Value" (already inverted?) 
  // Usually Drift Table = "Right". Correction = "-Drift".
  // For now, let's execute standard logic: Final Az = Map Az - (SpinDrift + Coriolis)
  // But Code at Step 5 says: azimuth = map_az + drift. 
  // This implies 'drift' is the CORRECTION value (Left). 
  // We will follow that convention.
  double cor_az_correction = -cor_az_mil; // Aim Left to correct Right drift

  // --- Step 3: Firing Table Lookup (Interpolation) ---
  // Select Table based on user Input Charge
  int chg_idx = sys->fire.charge;
  if (chg_idx < 1) chg_idx = 1;
  if (chg_idx > 7) chg_idx = 7;
    
  // Safety Update input if clamped
  sys->fire.charge = chg_idx;

  const FiringTable_Row_t *current_ft = FT_DB[chg_idx];
  int current_size = FT_Sizes[chg_idx];
    
  // [Error Check 1] Absolute Max Range Check (Weapon Limit)
  float abs_max_range = FT_DB[7][FT_Sizes[7]-1].range_m;
  if (final_lookup_range > abs_max_range) {
    sys->fire.elevation = FCS_ERR_RANGE;
    return;
  }

  // [Error Check 2] Current Charge Range Check
  float chg_min = current_ft[0].range_m;
  float chg_max = current_ft[current_size-1].range_m;
    
  if (final_lookup_range < chg_min || final_lookup_range > chg_max) {
    sys->fire.elevation = FCS_ERR_CHARGE;
    return;
  }

  float base_elev = 0.0f;
  float drift = 0.0f;
  float c_factor = 0.0f;
    
  // Find interval
  int idx = -1;
  for (int i = 0; i < current_size - 1; i++) {
    if (final_lookup_range >= current_ft[i].range_m && final_lookup_range <= current_ft[i+1].range_m) {
      idx = i;
      break;
    }
  }
    
  if (idx != -1) {
    // Interpolate
    float r1 = current_ft[idx].range_m;
    float r2 = current_ft[idx+1].range_m;
        
    base_elev = L_Interp(r1, current_ft[idx].elev_mil, r2, current_ft[idx+1].elev_mil, final_lookup_range);
    drift     = L_Interp(r1, current_ft[idx].drift_mil, r2, current_ft[idx+1].drift_mil, final_lookup_range);
    c_factor  = L_Interp(r1, current_ft[idx].c_factor, r2, current_ft[idx+1].c_factor, final_lookup_range);
  } else {
    // Should be covered by error checks, but fallback safety
    sys->fire.elevation = FCS_ERR_CHARGE;
    return;
  }

  // --- Step 4: Site Correction (Vertical Interval) ---
  // Formula: Site = VI / R * C_Factor (Conceptually) OR Site = VI * C_Factor (if factor is per meter)
  // Our C_Factor in table is "mil per 10m height"
  float site_corr = (vi_m / 10.0f) * c_factor;
    
  // --- Step 5: Final Data Assembly ---
  sys->fire.distance_km = (float)(map_dist_m / 1000.0); // Display Map Range or Corrected? Usually Map is useful reference.
  sys->fire.elevation = base_elev + site_corr;
    
  // [Adjustment Applied Here] 
  // Logic: Gun_Correction = Observed_Deviation * (OT_Dist/1000) / (GT_Dist/1000)
  // Assumption: OT_Dist = 1000m -> OT_Factor = 1.0
  // Equation: Gun_Correction = Input_Mil / GT_Factor
  float gt_factor = sys->fire.distance_km; // GT Distance in km
  if (gt_factor < 0.1f) gt_factor = 0.1f; // Div by Zero Protection
    
  float correction_mil = (float)sys->adj.az_mil / gt_factor;

  sys->fire.azimuth = map_az_mil + drift + cor_az_correction + correction_mil; 
    
  // [Validation Data] Save Intermediate Values
  sys->fire.map_azimuth = map_az_mil;
  sys->fire.map_distance = (float)map_dist_m;
  sys->fire.height_diff = vi_m; 
  // sys->fire.charge is already set above
  // sys->fire.rounds is managed by UI Knob inputs, do not overwrite here.

  // Save Mask Angle Check (Optional)
  // if (sys->fire.elevation < sys->mask_angle) ... warning
}
