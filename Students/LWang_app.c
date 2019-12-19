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
#define REACH_DST 10

#define MIN_DST 80
#define MIN_DST_2 40

#define SAFE_DST 200
#define ESCAPE_DST 150

#define WARN_TAN 0.2

#define angleRatio 0.1
#define anticRate 2.4
/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

typedef struct {
    OS_MUTEX mtx;
    Tank *tank;
    float nearMis_prv;
    float nearMis_cur;

    float threatDir_prv;
    float threatDir_cur;

    int friendId;
    float friendDir;
    float collidDir;

    int enemyId;
    float enemyDir;
    float enemyDir_prv;

    float heading;
    Point pos;

    Point corners[4];
    // int prevThreat;
} TankInfo;

OS_ERR err_tes;
CPU_TS ts_tes;

TankInfo tankInfoA;
TankInfo tankInfoB;

TankInfo tankInfoC;
TankInfo tankInfoD;

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/


static void test(void *param);
static void detect (void *param);
static void drive(void *param);

float relToabs(float relAngle);
bool shoot(TankInfo *tankInfo);
float dist(Point a, Point b);

float avoidFriend(TankInfo *tankInfo);
float avoidThreat(TankInfo *tankInfo);
float toCorner(TankInfo *tankInfo, Point cornerPos);
float avoidEdge (TankInfo *tankInfo);


// Student initialization function
void LWang_init(char teamNumber)
{
    // Initialize any system constructs (such as mutexes) here
    
    // Perform any user-required initializations here
    tankInfoA.tank = create_tank(teamNumber, "hahaha");
    tankInfoB.tank = create_tank(teamNumber, "hehehehe");

    int idA = initialize_tank(tankInfoA.tank);
    int idB = initialize_tank(tankInfoB.tank);
    // Register user functions here
    OSMutexCreate(&tankInfoA.mtx, "Amtx",  &err_tes);
    OSMutexCreate(&tankInfoB.mtx, "Bmtx",  &err_tes);

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

    // tankInfoA.prevThreat = 0;
    // tankInfoB.prevThreat = 0;

    tankInfoA.friendId = idB;
    tankInfoB.friendId = idA;

    tankInfoA.corners[0].x = tankInfoB.corners[0].x = CORNER_L;
    tankInfoA.corners[0].y = tankInfoB.corners[0].y = CORNER_U;
    tankInfoA.corners[1].x = tankInfoB.corners[1].x = CORNER_R;
    tankInfoA.corners[1].y = tankInfoB.corners[1].y = CORNER_U;
    tankInfoA.corners[2].x = tankInfoB.corners[2].x = CORNER_R;
    tankInfoA.corners[2].y = tankInfoB.corners[2].y = CORNER_D;
    tankInfoA.corners[3].x = tankInfoB.corners[3].x = CORNER_L;
    tankInfoA.corners[3].y = tankInfoB.corners[3].y = CORNER_D;

    register_user(&detect, 5, &tankInfoA);
    register_user(&detect, 5, &tankInfoB);
    register_user(&drive, 6, &tankInfoA);
    register_user(&drive, 6, &tankInfoB); 
}

void LWang_test_init(char teamNumber)
{
    // Initialize any system constructs (such as mutexes) here
    
    // Perform any user-required initializations here
    tankInfoC.tank = create_tank(teamNumber, "hahaha");
    tankInfoD.tank = create_tank(teamNumber, "hehehehe");

    int idC = initialize_tank(tankInfoC.tank);
    int idD = initialize_tank(tankInfoD.tank);
    // Register user functions here

    OSMutexCreate(&tankInfoC.mtx, "Cmtx",  &err_tes);
    OSMutexCreate(&tankInfoD.mtx, "Dmtx",  &err_tes);

    tankInfoC.nearMis_prv = -1;
    tankInfoC.nearMis_cur = -1;
    tankInfoD.nearMis_prv = -1;
    tankInfoD.nearMis_cur = -1;

    tankInfoC.enemyId = -1;
    tankInfoD.enemyId = -1;

    tankInfoC.threatDir_prv = NONE_DIR;
    tankInfoC.threatDir_cur = NONE_DIR;
    tankInfoD.threatDir_prv = NONE_DIR;
    tankInfoD.threatDir_cur = NONE_DIR;

    // tankInfoC.prevThreat = 0;
    // tankInfoD.prevThreat = 0;

    tankInfoC.friendId = idD;
    tankInfoD.friendId = idC;

    tankInfoC.corners[0].x = tankInfoD.corners[0].x = CORNER_L;
    tankInfoC.corners[0].y = tankInfoD.corners[0].y = CORNER_U;
    tankInfoC.corners[1].x = tankInfoD.corners[1].x = CORNER_R;
    tankInfoC.corners[1].y = tankInfoD.corners[1].y = CORNER_U;
    tankInfoC.corners[2].x = tankInfoD.corners[2].x = CORNER_R;
    tankInfoC.corners[2].y = tankInfoD.corners[2].y = CORNER_D;
    tankInfoC.corners[3].x = tankInfoD.corners[3].x = CORNER_L;
    tankInfoC.corners[3].y = tankInfoD.corners[3].y = CORNER_D;

    // register_user(&detect, 5, &tankInfoC);
    // register_user(&detect, 5, &tankInfoD);
    register_user(&test, 6, &tankInfoC);
    register_user(&test, 6, &tankInfoD); 
}



static void detect (void *param) {

    TankInfo *tankInfo = (TankInfo*)param;
    RadarData radarData;

    while (1) {
        
        OSMutexPend(&tankInfo->mtx, 0, OS_OPT_PEND_BLOCKING, &ts_tes, &err_tes);
        poll_radar(tankInfo->tank, &radarData);
        tankInfo->nearMis_prv = tankInfo->nearMis_cur;
        tankInfo->threatDir_prv = tankInfo->threatDir_cur;
/* detect nearest missile */
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
            if (tankInfo->threatDir_cur < 0) {
                tankInfo->threatDir_cur = tankInfo->threatDir_cur + D_PI;
            }
        }

/* detect nearest enemy + friend */
        float enemyDist = 999999;
        int targetId = -1;
        tankInfo->friendDir = NONE_DIR;
        tankInfo->enemyDir_prv = tankInfo->enemyDir;
        tankInfo->enemyDir = NONE_DIR;
        tankInfo->collidDir = NONE_DIR;

        if (radarData.numTanks == 0) {
            tankInfo->enemyId = -1;
        }
        else {
            for (int i = 0; i < radarData.numTanks; i++) {
                // if (radarData.tankIDs[i] == tankInfo->friendId) {
                //     if (radarData.tankDistances[i] < SAFE_DST/2) {
                //         tankInfo->friendDir = radarData.tankBearings[i];
                //     }
                //     continue;
                // }

                 if (radarData.tankDistances[i] < SAFE_DST/2 && 
                    (radarData.tankBearings[i] < PI_2 || radarData.tankBearings[i] > D_PI - PI_2)) {
                    tankInfo->collidDir = radarData.tankBearings[i];
                }

                if (radarData.tankIDs[i] == tankInfo->friendId) {

                    tankInfo->friendDir = radarData.tankBearings[i];
                    continue;
                }

                // if (radarData.tankIDs[i] == tankInfo->enemyId) {
                //     targetId = radarData.tankIDs[i];
                //     tankInfo->enemyDir = radarData.tankBearings[i];
                //     enemyDist = radarData.tankDistances[i];
                //     break;
                // }

                if (radarData.tankDistances[i] < enemyDist) {
                    targetId = radarData.tankIDs[i];
                    tankInfo->enemyDir = radarData.tankBearings[i];
                    enemyDist = radarData.tankDistances[i];
                }
            }

            if (targetId != tankInfo->enemyId) {
                tankInfo->enemyDir_prv = NONE_DIR;
            }

            tankInfo->enemyId = targetId;

        }

        OSMutexPost(&tankInfo->mtx, OS_OPT_POST_1, &err_tes);   
        // BSP_LED_On(2);
        OSTimeDlyHMSM(0, 0, 0, 50, OS_OPT_TIME_HMSM_STRICT,  &err_tes);
        // BSP_LED_Off(2);
    }
}


static void drive(void *param) {

    Point curPos;
    int count = 0;
    float steering;
    TankInfo *tankInfo = (TankInfo*)param;
    create_turret(tankInfo->tank);

    int light = 0;
    while (1) {
        OSMutexPost(&tankInfo->mtx, OS_OPT_POST_1, &err_tes);
        // OSSched();
        BSP_LED_On(light);
        // OSTimeDlyHMSM(0, 0, 0, 30, OS_OPT_TIME_HMSM_STRICT,  &err_tes);
        OSSched();
        BSP_LED_Off(light);

        OSMutexPend(&tankInfo->mtx, 0, OS_OPT_PEND_BLOCKING, &ts_tes, &err_tes);

        shoot(tankInfo);

        accelerate(tankInfo->tank, ACCELERATE_LIMIT);

        tankInfo->pos = get_position(tankInfo->tank);
        tankInfo->heading = get_heading(tankInfo->tank);

        if (dist(tankInfo->pos, tankInfo->corners[count]) < REACH_DST) {
            count = (count + 1) % 4;
        }
        
        steering = avoidEdge(tankInfo);
        if (steering < NONE_DIR) {
            set_steering(tankInfo->tank, steering);
            light = 1;
            continue;
        }

        steering = avoidFriend(tankInfo);
        if (steering < NONE_DIR) {
            set_steering(tankInfo->tank, steering);
            continue;
        }

        steering = avoidThreat(tankInfo);
        if (steering < NONE_DIR) {
            set_steering(tankInfo->tank, steering);
            // tankInfo->prevThreat = 1;
            light = 2;
            continue;
        }
        // else if (tankInfo->nearMis_cur > ESCAPE_DST || tankInfo->nearMis_cur > tankInfo->nearMis_prv){
        //     tankInfo->prevThreat = 0;
        // }

        steering = toCorner(tankInfo, tankInfo->corners[count]);
        set_steering(tankInfo->tank, steering);
        light = 3;
    }
}

float relToabs(float relAngle) {
    if (relAngle < PI + PI_2 && relAngle > PI_2) {
        float base = relAngle - PI;
        return relAngle + base*angleRatio;
    }
    else {
        if (relAngle <= PI) {
            return relAngle*(1 - angleRatio);
        }
        else {
            return D_PI - (D_PI - relAngle)*(1 - angleRatio);
        }
    }
}

bool shoot(TankInfo *tankInfo) {
    // return fire(tankInfo->tank);
    if (tankInfo->enemyId == -1 || tankInfo->enemyDir_prv >= NONE_DIR) {
        return FALSE;
    }
    
    float friendTrd = NONE_DIR;

    float prvAngle = relToabs(tankInfo->enemyDir_prv);
    float curAngle = relToabs(tankInfo->enemyDir);
    float predAngle = curAngle + (curAngle - prvAngle) * angleRatio;
    if (predAngle < 0) {
        predAngle = predAngle + D_PI;
    }
    if (predAngle > D_PI) {
        predAngle = predAngle - D_PI;
    }

    if (tankInfo->friendDir < NONE_DIR) {
        friendTrd = predAngle - tankInfo->friendDir;
    }

    if (friendTrd < PI_4/2 && friendTrd > - PI_4/2) {
        return FALSE;
    }

    set_turret_angle(tankInfo->tank, predAngle);
    return fire(tankInfo->tank);
}


float dist(Point a, Point b) {
    return sqrt((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y));
}


float avoidFriend(TankInfo *tankInfo) {
    if (tankInfo->collidDir >= NONE_DIR) {
        return NONE_DIR;
    }
    if (tankInfo->collidDir < PI_2) {
        return -STEER_LIMIT;
    }
    // if (tankInfo->friendDir < PI_2) {
    //     return tankInfo->friendDir - PI_2;
    // }

    // if (tankInfo->friendDir > D_PI - PI_4) {
    //     return STEER_LIMIT;
    // }
    if (tankInfo->collidDir > D_PI - PI_2) {
        // return tankInfo->friendDir - D_PI + PI_2;
        return STEER_LIMIT;
    }

    return NONE_DIR;
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

        OSTimeDlyHMSM(0, 0, 0, 20, OS_OPT_TIME_HMSM_STRICT,  &err_tes);

        BSP_LED_Off(0);
    }
}