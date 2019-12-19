#include <includes.h>
#include <string.h>
#include <math.h>
#include "BattlefieldLib.h"

#define MAX_ENEMY 2

#define NONE_DIR 7.0f
#define D_PI 6.283185307f
#define PI 3.141592653f
#define PI_2 1.570796326f
#define PI_4 0.785398163f

#define CORNER_L 310
#define CORNER_D 460
#define CORNER_R 510
#define CORNER_U 260
#define REACH_DST 30

#define MIN_DST 80
#define MIN_DST_2 40

#define SAFE_DST 200
#define ESCAPE_DST 150

#define WARN_TAN 0.5
/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

typedef struct {
    Tank *tank;
    float nearMis_prv;
    float nearMis_cur;

    float threatDir_prv;
    float threatDir_cur;

    int friendId;

    int enemyId;
    float enemyDir;
    Point enemyPos_cur;
    Point enemyPos_prv;

    float heading;
    Point pos;

    int prevThreat;
} TankInfo;


TankInfo tankInfoA;
TankInfo tankInfoB;

static Point corners[4];

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/


static void test(void *param);
static void detect (void *param);
static void drive(void *param);

float dist(Point a, Point b);
float avoidThreat(TankInfo *tankInfo);
float toCorner(TankInfo *tankInfo, Point cornerPos);
float avoidEdge (TankInfo *tankInfo);
bool shoot(TankInfo *tankInfo);

// Student initialization function
void LWang_test_init(char teamNumber)
{
    // Initialize any system constructs (such as mutexes) here
    
    // Perform any user-required initializations here
    tankInfoA.tank = create_tank(teamNumber, "hahaha");
    tankInfoB.tank = create_tank(teamNumber, "hehehehe");

    int idA = initialize_tank(tankInfoA.tank);
    int idB = initialize_tank(tankInfoB.tank);
    // Register user functions here

    tankInfoA.nearMis_prv = -1;
    tankInfoA.nearMis_cur = -1;
    tankInfoB.nearMis_prv = -1;
    tankInfoB.nearMis_cur = -1;

    tankInfoA.enemyId = -1;
    tankInfoB.enemyId = -1;

    tankInfoA.threatDir_prv = NONE_DIR;
    tankInfoA.threatDir_cur = NONE_DIR;
    tankInfoB.threatDir_prv = NONE_DIR;
    tankInfoB.threatDir_cur = NONE_DIR;

    tankInfoA.prevThreat = 0;
    tankInfoB.prevThreat = 0;

    tankInfoA.friendId = idB;
    tankInfoB.friendId = idA;

    corners[0].x = CORNER_L;
    corners[0].y = CORNER_U;
    corners[1].x = CORNER_R;
    corners[1].y = CORNER_U;
    corners[2].x = CORNER_R;
    corners[2].y = CORNER_D;
    corners[3].x = CORNER_L;
    corners[3].y = CORNER_D;

    register_user(&detect, 5, &tankInfoA);
    register_user(&detect, 5, &tankInfoB);
    register_user(&drive, 6, &tankInfoA);
    register_user(&drive, 6, &tankInfoB); 
}


static void detect (void *param) {
    OS_ERR err;

    TankInfo *tankInfo = (TankInfo*)param;
    RadarData radarData;

    while (1) {
        poll_radar(tankInfo->tank, &radarData);
        tankInfo->nearMis_prv = tankInfo->nearMis_cur;
        tankInfo->threatDir_prv = tankInfo->threatDir_cur;

        if (radarData.numMissiles == 0) {
            tankInfo->nearMis_cur = -1;
            tankInfo->threatDir_cur = NONE_DIR;
        }
        else {
            int threadIdx = 0;
            tankInfo->nearMis_cur = radarData.missileDistances[0];

            for (int i = 1; i < radarData.numMissiles; i++) {
                if (radarData.missileDistances[i] < tankInfo->nearMis_cur) {
                    tankInfo->nearMis_cur = radarData.missileDistances[i];
                    threadIdx = i;
                }
            }

            tankInfo->nearMis_cur = radarData.missileDistances[threadIdx];
            tankInfo->threatDir_cur = radarData.missileBearings[threadIdx];
        }

        float enemyDist = 999999;
        int targetId = -1;
        if (radarData.numTanks == 0) {
            tankInfo->enemyId = -1;
        }
        else {

            for (int i = 0; i < radarData.numTanks; i++) {
                if (radarData.tankIDs[i] == tankInfo->friendId) {
                    continue;
                }

                if (radarData.tankIDs[i] == tankInfo->enemyId) {
                    tankInfo->enemyDir = radarData.tankBearings[i];
                    enemyDist = radarData.tankDistances[i];
                    break;
                }

                if (radarData.tankDistances[i] < enemyDist) {
                    targetId = radarData.tankIDs[i];
                    tankInfo->enemyDir = radarData.tankBearings[i];
                    enemyDist = radarData.tankDistances[i];
                }
            }

            if (targetId != tankInfo->enemyId && targetId > 0) {
                tankInfo->enemyPos_prv.x = -1;
                tankInfo->enemyPos_prv.y = -1;
                targetId = -2;
            }

            if (targetId != -1) {
                float absDir = tankInfo->enemyDir + tankInfo->heading;
                if (absDir > D_PI) {
                    absDir = absDir - D_PI;
                }

                if (targetId == -2) {
                    tankInfo->enemyPos_prv = tankInfo->enemyPos_cur;
                }

                tankInfo->enemyPos_cur.x = tankInfo->pos.x + cos(absDir);
                tankInfo->enemyPos_cur.y = tankInfo->pos.x - sin(absDir);
            }

            tankInfo->enemyId = targetId;
            
        }
        // BSP_LED_On(2);
        OSTimeDlyHMSM(0, 0, 0, 50, OS_OPT_TIME_HMSM_STRICT, &err);
        // BSP_LED_Off(2);
    }
}


static void drive(void *param) {
    OS_ERR err;

    Point curPos;
    int count = 0;
    float steering;
    TankInfo *tankInfo = (TankInfo*)param;
    create_turret(tankInfo->tank);

    while (1) {
        BSP_LED_On(tankInfo->prevThreat + 1);
        OSTimeDlyHMSM(0, 0, 0, 20, OS_OPT_TIME_HMSM_STRICT, &err);
        BSP_LED_Off(tankInfo->prevThreat + 1);

        set_turret_angle(tankInfo->tank, PI + PI_2);
        shoot(tankInfo);

        accelerate(tankInfo->tank, ACCELERATE_LIMIT);

        tankInfo->pos = get_position(tankInfo->tank);
        tankInfo->heading = get_heading(tankInfo->tank);

        if (dist(tankInfo->pos, corners[count]) < REACH_DST) {
            count = (count + 1) % 4;
        }

        steering = avoidEdge(tankInfo);

        if (steering < NONE_DIR) {
            set_steering(tankInfo->tank, steering);
            continue;
        }

        steering = avoidThreat(tankInfo);

        if (steering < NONE_DIR) {
            set_steering(tankInfo->tank, steering);
            tankInfo->prevThreat = 1;
            continue;
        }
        else if (tankInfo->nearMis_cur > ESCAPE_DST || tankInfo->nearMis_cur > tankInfo->nearMis_prv){
            tankInfo->prevThreat = 0;
        }

        if (tankInfo->prevThreat == 0) {
            steering = toCorner(tankInfo, corners[count]);
            set_steering(tankInfo->tank, steering);
        }
        
    }
}


bool shoot(TankInfo *tankInfo) {
  
    if (tankInfo->enemyId == -1 || tankInfo->enemyPos_prv.x == -1) {
        return FALSE;
    }

    set_turret_angle(tankInfo->tank, tankInfo->enemyDir);
    return fire(tankInfo->tank);
}


float dist(Point a, Point b) {
    return sqrt((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y));
}

float avoidThreat(TankInfo *tankInfo) {
    if ((tankInfo->nearMis_cur > SAFE_DST) 
        || (tankInfo->nearMis_cur == -1) 
        || (tankInfo->nearMis_cur > tankInfo->nearMis_prv && tankInfo->nearMis_prv > -1)) {
        return NONE_DIR;
    }

    if (tankInfo->nearMis_prv == -1) {
        if (tankInfo->threatDir_cur < PI_4) {
            return -STEER_LIMIT;
        }
        if (tankInfo->threatDir_cur < PI_2) {
            return tankInfo->threatDir_cur - PI_2;
        }
        if (tankInfo->threatDir_cur > D_PI - PI_4) {
            return STEER_LIMIT;
        }
        if (tankInfo->threatDir_cur > D_PI - PI_2) {
            return tankInfo->threatDir_cur - D_PI + PI_2;
        }

        return NONE_DIR;
    }

    float angleDst = tankInfo->threatDir_cur - tankInfo->threatDir_prv;
    if (fabsf(angleDst) > PI_4 && fabsf(angleDst) < D_PI - PI_4) {
        return NONE_DIR;
    }

    if (sin(fabsf(angleDst))*tankInfo->nearMis_cur / (tankInfo->nearMis_prv - tankInfo->nearMis_cur) < WARN_TAN) {
        
        if (tankInfo->threatDir_cur > PI_2 && tankInfo->threatDir_cur < PI + PI_2) {
            if (angleDst > 0) {
                return STEER_LIMIT;
            }
            else {
                return -STEER_LIMIT;
            }
        }
        else {
            if (angleDst > 0) {
                return -STEER_LIMIT;
            }
            else {
                return STEER_LIMIT;
            }
        }

        return NONE_DIR;
    }
    
}


float toCorner(TankInfo *tankInfo, Point cornerPos) {
    if (cornerPos.x == tankInfo->pos.x) {
        return 0;
    }

    float angle = atan((float)(tankInfo->pos.y - cornerPos.y) / (float)(cornerPos.x - tankInfo->pos.x));
    if (cornerPos.x < tankInfo->pos.x) {
        angle = angle + PI;
    }
    if (angle < 0) {
        angle = angle + D_PI;
    }

    float rotate = angle - tankInfo->heading;
    if (rotate > 0) {
        if (rotate < PI) {
            if (rotate < PI_4) {
                return rotate;
            }
            return STEER_LIMIT;
        }
        else {
            if (rotate > D_PI - PI_4) {
                return rotate - D_PI;
            }
            return -STEER_LIMIT;
        }
    }
    else {
        if (rotate > -PI) {
            if (rotate > -PI_4) {
                return rotate;
            }
            return -STEER_LIMIT;
        }
        else {
            if (rotate < PI_4 - D_PI) {
                return rotate + D_PI;
            }
            return STEER_LIMIT;
        }
    }
}


float avoidEdge (TankInfo *tankInfo) {
    Point pos = get_position(tankInfo->tank);

    int up = pos.y - MINY;
    int left = pos.x - MINX;
    int down = MAXY - pos.y;
    int right = MAXX - pos.x;

    if (up < MIN_DST && tankInfo->heading < PI) {
        
        if (left < MIN_DST && tankInfo->heading > PI_2) {
            if (tankInfo->heading < PI_2 + PI_4) {
                return -STEER_LIMIT;
            }
            else {
                return STEER_LIMIT;
            }
        }

        if (right < MIN_DST && tankInfo->heading <= PI_2) {
            if (tankInfo->heading < PI_4) {
                return -STEER_LIMIT;
            }
            else {
                return STEER_LIMIT;
            }
        }

        if (up < MIN_DST_2) {
            if (tankInfo->heading > PI_2) {
                return STEER_LIMIT;
            }
            else {
                return -STEER_LIMIT;
            }
        }
    }

    else if (down < MIN_DST && tankInfo->heading >= PI) {

        if (left < MIN_DST && tankInfo->heading < PI + PI_2) {
            if (tankInfo->heading < PI + PI_4) {
                return -STEER_LIMIT;
            }
            else {
                return STEER_LIMIT;
            }
        }

        if (right < MIN_DST && tankInfo->heading >= PI + PI_2) {
            if (tankInfo->heading < D_PI - PI_4) {
                return -STEER_LIMIT;
            }
            else {
                return STEER_LIMIT;
            }
        }

        if (down < MIN_DST_2) {
            if (tankInfo->heading > PI + PI_2) {
                return STEER_LIMIT;
            }
            else {
                return -STEER_LIMIT;
            }
        }
    }

    if (left < MIN_DST_2 && tankInfo->heading > PI_2 && tankInfo->heading < PI + PI_2) {
        if (tankInfo->heading < PI) {
            return -STEER_LIMIT;
        }
        else {
            return STEER_LIMIT;
        }
    }
    else if (right < MIN_DST_2 && (tankInfo->heading < PI_2 || tankInfo->heading > PI + PI_2)) {
        if (tankInfo->heading < PI_2) {
            return STEER_LIMIT;
        }
        else {
            return -STEER_LIMIT;
        }
    }

    return NONE_DIR;
}



static void test(void *param) {
    TankInfo* tankInfo = (TankInfo*)param;  
    OS_ERR err;
    bool fired;

    create_turret(tankInfo->tank);
    while (1) {
        accelerate(tankInfo->tank, ACCELERATE_LIMIT);
        set_steering(tankInfo->tank, STEER_LIMIT);
        fired = fire(tankInfo->tank);
        if (fired = TRUE) {
            BSP_LED_On(1);
        }
        else {
            BSP_LED_On(2);
        }

        OSTimeDlyHMSM(0, 0, 0, 50, OS_OPT_TIME_HMSM_STRICT, &err);

        BSP_LED_Off(0);
    }
}