#include "fcs_core.h"
#include "fcs_math.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// [1] 표준 설정 함수 (API)
void FCS_Set_Battery(FCS_System_t *sys, int zone, char band, double e, double n, float alt) {
  sys->user_pos.zone = zone;
  sys->user_pos.band = band;
  sys->user_pos.easting = e;
  sys->user_pos.northing = n;
  sys->user_pos.altitude = alt;
}

void FCS_Set_Target(FCS_System_t *sys, int zone, char band, double e, double n, float alt) {
  sys->tgt_pos.zone = zone;
  sys->tgt_pos.band = band;
  sys->tgt_pos.easting = e;
  sys->tgt_pos.northing = n;
  sys->tgt_pos.altitude = alt;
}

// [2] 명령어 처리기 (Verify & Bluetooth Core)
// CMD Format: "TGT:52,S,333712,4132894,100"
int FCS_Process_Command(FCS_System_t *sys, char *cmd, char *resp) {
  
  // 1. Target Input Command
  if (strncmp(cmd, "TGT:", 4) == 0) {
    int z;
    char b;
    double e, n;
    float a;
    
    // Parse: TGT:52,S,123.0,456.0,100.0
    // Using Integers to avoid %f/%lf issues in minimal printf/scanf implementations
    long e_int, n_int;
    int a_int;
    
    // Format: TGT:52,S,333712,4132894,105
    int count = sscanf(cmd + 4, "%d,%c,%ld,%ld,%d", &z, &b, &e_int, &n_int, &a_int);
    
    if (count == 5) {
      // Cast to Double/Float
      e = (double)e_int;
      n = (double)n_int;
      a = (float)a_int;
      
      // Update System
      FCS_Set_Target(sys, z, b, e, n, a);
      
      // Calculate Ballistics Immediately
      FCS_Calculate_FireData(sys);
      
      // Generate Response (Validation Output)
      // Workaround: Split floats into integers manually because %f is not active
      int az_i = (int)sys->fire.azimuth;
      int az_d = (int)((sys->fire.azimuth - az_i) * 10); if(az_d<0) az_d = -az_d;
      
      int el_i = (int)sys->fire.elevation;
      int el_d = (int)((sys->fire.elevation - el_i) * 10); if(el_d<0) el_d = -el_d;
      
      int maz_i = (int)sys->fire.map_azimuth;
      int maz_d = (int)((sys->fire.map_azimuth - maz_i) * 10); if(maz_d<0) maz_d = -maz_d;
      
      // Expanded: F_AZ, F_EL, F_DIST | M_AZ, M_DIST, VI
      sprintf(resp, "OK:AZ%d.%d,EL%d.%d,D%d|MAZ%d.%d,MD%d,VI%d", 
              az_i, az_d, el_i, el_d, (int)(sys->fire.distance_km * 1000),
              maz_i, maz_d, (int)sys->fire.map_distance, (int)sys->fire.height_diff);
      
      // Update UI State Context if needed
      sys->state = UI_FIRE_DATA; 
      
      return 1; // Success
    } else {
      sprintf(resp, "ERR:ParseFail(%d)", count);
      return -1;
    }
  }
  
  return 0; // Unknown Command
}
