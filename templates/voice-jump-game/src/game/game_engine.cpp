#include "game_engine.h"
#include <stdlib.h>

void gameInit(GameState *game)
{
    game->princess.x = PRINCESS_X;
    game->princess.y = GROUND_Y - PRINCESS_SIZE;
    game->princess.vy = 0;
    game->princess.jumping = false;
    game->princess.onGround = true;

    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        game->obstacles[i].type = OBSTACLE_NONE;
        game->obstacles[i].active = false;
    }

    game->scrollOffset = 0;
    game->distance = 0;
    game->speed = GAME_SPEED_BASE;
    game->gameOver = false;
    game->deathTime = 0;
    game->nextObstacleDistance = 50;
}

void gameReset(GameState *game)
{
    gameInit(game);
}

void gameJump(GameState *game)
{
    if (game->princess.onGround && !game->gameOver)
    {
        game->princess.vy = -JUMP_VELOCITY;
        game->princess.jumping = true;
        game->princess.onGround = false;
    }
}

void gameUpdate(GameState *game)
{
    if (game->gameOver)
    {
        if (millis() - game->deathTime > 2000)
        {
            gameReset(game);
        }
        return;
    }

    game->princess.vy += GRAVITY;
    game->princess.y += game->princess.vy;

    if (game->princess.y >= GROUND_Y - PRINCESS_SIZE)
    {
        game->princess.y = GROUND_Y - PRINCESS_SIZE;
        game->princess.vy = 0;
        game->princess.jumping = false;
        game->princess.onGround = true;
    }

    game->scrollOffset += game->speed;
    if (game->scrollOffset >= 20)
    {
        game->scrollOffset = 0;
        game->distance++;
    }

    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        if (game->obstacles[i].active)
        {
            game->obstacles[i].x -= game->speed;

            if (game->obstacles[i].x + game->obstacles[i].w < 0)
            {
                game->obstacles[i].active = false;
                game->obstacles[i].type = OBSTACLE_NONE;
            }

            if (checkCollision(&game->princess, &game->obstacles[i]))
            {
                game->gameOver = true;
                game->deathTime = millis();
                return;
            }
        }
    }

    if (game->distance >= game->nextObstacleDistance)
    {
        spawnObstacle(game);
        game->nextObstacleDistance = game->distance + random(OBSTACLE_SPACING_MIN, OBSTACLE_SPACING_MAX) / 10;
    }

    if (game->distance % 500 == 0 && game->distance > 0)
    {
        game->speed++;
    }
}

bool checkCollision(const Princess *p, const Obstacle *o)
{
    if (!o->active || o->type == OBSTACLE_NONE)
        return false;

    int px = p->x - 6;
    int py = p->y - 12;
    int pw = 12;
    int ph = 12;

    if (px < o->x + o->w &&
        px + pw > o->x &&
        py < o->y + o->h &&
        py + ph > o->y)
    {
        return true;
    }

    return false;
}

void spawnObstacle(GameState *game)
{
    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        if (!game->obstacles[i].active)
        {
            int type = random(0, 2);

            if (type == 0)
            {
                game->obstacles[i].type = OBSTACLE_BLOCK;
                game->obstacles[i].x = 260;
                game->obstacles[i].y = GROUND_Y - random(30, 70);
                game->obstacles[i].w = random(20, 40);
                game->obstacles[i].h = GROUND_Y - game->obstacles[i].y;
            }
            else
            {
                game->obstacles[i].type = OBSTACLE_PIT;
                game->obstacles[i].x = 260;
                game->obstacles[i].y = GROUND_Y;
                game->obstacles[i].w = random(40, 80);
                game->obstacles[i].h = 30;
            }

            game->obstacles[i].active = true;
            break;
        }
    }
}

bool isVoiceTriggered(int16_t *audioBuf, int samples)
{
    static unsigned long lastJumpTime = 0;
    unsigned long now = millis();

    if (now - lastJumpTime < VOICE_COOLDOWN_MS)
    {
        return false;
    }

    int32_t sum = 0;
    for (int i = 0; i < samples; i++)
    {
        sum += abs(audioBuf[i]);
    }
    int avg = sum / samples;

    if (avg > VOICE_THRESHOLD)
    {
        lastJumpTime = now;
        return true;
    }

    return false;
}
