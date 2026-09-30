// SPDX-License-Identifier: Apache-2.0
/* Authored variations: one lower route always remains available. */
enum { SL_RECOVERY,SL_DRAINAGE,SL_SURVEY,SL_MISSIONS };
enum { SL_WRENCH=1,SL_INSULATOR=2,SL_DECOY=4,SL_BATTERY=8 };
typedef struct {
    uint8_t closed_side,fault_x,target_side;
    uint16_t patrol_offset;
    const char *name;
    uint8_t patrol_mode;
} sl_site_t;
static const sl_site_t s_sites[]={
    {0,0,0,0,"STANDARD INSPECTION",0},
    {1,4,1,180,"WEST SHUTTER / WEST ARC",1},
    {2,12,0,360,"EAST SHUTTER / EAST ARC",2},
    {0,4,1,90,"WEST LIVE FLOOR",1},
    {1,12,0,270,"WEST SHUTTER / EAST ARC",2},
    {2,4,1,540,"EAST SHUTTER / WEST ARC",0},
};
static const char *const s_patrol_names[]={"SOUTH SHUTTLE","INNER LOOP","STOP AND SCAN"};
#define SL_SITES (sizeof(s_sites)/sizeof(s_sites[0]))
static const uint8_t s_kits[]={3,5,9,6,10,12};
static const char *const s_kit_names[]={"WRENCH + INSULATOR","WRENCH + DECOY",
    "WRENCH + BATTERY","INSULATOR + DECOY","INSULATOR + BATTERY","DECOY + BATTERY"};
static const char *const s_mission_names[]={"RECORDER RECOVERY","DRAINAGE REPAIR","MISSING SERVICE LOG"};
