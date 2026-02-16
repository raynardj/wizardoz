#pragma once

#include <Arduino.h>

#define MAX_OBSTACLES 8
#define OBSTACLE_SPACING_MIN 40
#define OBSTACLE_SPACING_MAX 100
#define GROUND_Y 210
#define PRINCESS_X 60
#define PRINCESS_SIZE 12
#define JUMP_VELOCITY 11
#define GRAVITY 1.2
#define GAME_SPEED_BASE 5
#define VOICE_THRESHOLD 800
#define VOICE_COOLDOWN_MS 300

typedef enum {
    OBSTACLE_NONE,
    OBSTACLE_BLOCK,
    OBSTACLE_PIT
} ObstacleType;

typedef struct {
    ObstacleType type;
    int x;
    int y;
    int w;
    int h;
    bool active;
} Obstacle;

typedef struct {
    int x;
    int y;
    float vy;
    bool jumping;
    bool onGround;
} Princess;

typedef struct {
    Princess princess;
    Obstacle obstacles[MAX_OBSTACLES];
    int scrollOffset;
    int distance;
    int speed;
    bool gameOver;
    unsigned long deathTime;
    int nextObstacleDistance;
} GameState;

void gameInit(GameState *game);
void gameUpdate(GameState *game);
void gameJump(GameState *game);
void gameReset(GameState *game);
bool checkCollision(const Princess *p, const Obstacle *o);
void spawnObstacle(GameState *game);
bool isVoiceTriggered(int16_t *audioBuf, int samples);
