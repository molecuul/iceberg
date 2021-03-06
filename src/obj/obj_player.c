#include "obj_player.h"

#include "../graphics/graphics.h"
#include "../event.h"
#include "../mem.h"
#include "../log.h"
#include "../input.h"
#include "../util.h"
#include "../types.h"

#define OBJ_PLAYER_TEXTURE IB_TEXTURE_FILE("player")
#define OBJ_PLAYER_SPEED_Y 2 /* !! 3 dimensions! */
#define OBJ_PLAYER_SPEED_X 3
#define OBJ_PLAYER_BASE_HEIGHT 12 /* height of the collision box for world movement, base at the bottom of the player sprite */
#define OBJ_PLAYER_BASE_WIDTH_MARGIN 2 /* shorten the cbox horizontally as well (from both sides) */
#define OBJ_PLAYER_CAMERA_FACTOR 16.0f /* increase for slower camera movement. 1.0f <=> camera will always be on player */

#define OBJ_PLAYER_BLINK_STEPS 10
#define OBJ_PLAYER_BLINK_COOLDOWN 20
#define OBJ_PLAYER_BLINK_DIST_X 10
#define OBJ_PLAYER_BLINK_DIST_Y 7.5

typedef struct {
    ib_sprite* spr;
    int in_blink, in_blink_cd;

    ib_ivec2 base_pos, base_size; /* temporary base positions */
    int collision_result; /* intermediate to store collision results */
} obj_player;

static int obj_player_collide_cb(ib_object* nc, void* d);

/* init */
void obj_player_init(ib_object* p) {
    obj_player* self = p->d = ib_malloc(sizeof *self);

    ib_object_subscribe(p, IB_EVT_DRAW);
    ib_object_subscribe(p, IB_EVT_UPDATE);
    ib_object_subscribe(p, IB_EVT_INPUT);

    self->spr = ib_sprite_alloc(OBJ_PLAYER_TEXTURE, 32, 32, 0);
    self->in_blink = 0;
    self->in_blink_cd = 0;

    /* self->base_size is effetively constant, base_pos is updated whenever we need to do calculations/collisions with it */
    self->base_size.x = p->size.x - 2 * OBJ_PLAYER_BASE_WIDTH_MARGIN;
    self->base_size.y = OBJ_PLAYER_BASE_HEIGHT;

    if (p->size.x != 32 || p->size.y != 32) ib_warn("your map player size is weird and I don't understand it (%dx%d)", p->size.x, p->size.y);
}

/* Event handling */
void obj_player_evt(ib_event* e, ib_object* obj) {
    obj_player* self = obj->d;

    ib_ivec2 cpos, csize;
    ib_graphics_get_camera(&cpos, &csize);

    switch (e->type) {
    case IB_EVT_UPDATE:
    {
        self->base_pos = obj->pos;

        int dir_x = ib_input_get_key(SDL_SCANCODE_RIGHT) - ib_input_get_key(SDL_SCANCODE_LEFT); /* sneaky logic */
        int dir_y = ib_input_get_key(SDL_SCANCODE_DOWN) - ib_input_get_key(SDL_SCANCODE_UP);

        int move_x = dir_x * OBJ_PLAYER_SPEED_X;
        int move_y = dir_y * OBJ_PLAYER_SPEED_Y;

        if (self->in_blink) {
            /* extra movement if we're blinking */
            ib_world_create_object("player_trail", NULL, NULL, obj->pos, obj->size, 0.0f, 1);
            move_x += dir_x * OBJ_PLAYER_BLINK_DIST_X;
            move_y += dir_y * OBJ_PLAYER_BLINK_DIST_Y;
            self->in_blink--;
        }

        if (self->in_blink_cd)	{
            /* decrement blink cooldown */
            self->in_blink_cd--;
        }

        self->base_pos.x += move_x + OBJ_PLAYER_BASE_WIDTH_MARGIN;
        self->base_pos.y += obj->size.y - OBJ_PLAYER_BASE_HEIGHT;

        /* try and integrate horizontal position */
        self->collision_result = 0;
        ib_world_object_foreach_by_type("noclip", obj_player_collide_cb, self); /* will set collision_result if anything happens */

        if (!self->collision_result) {
            obj->pos.x += move_x;
        } else {
            self->base_pos.x -= move_x;
        }

        self->base_pos.y += move_y;

        self->collision_result = 0;
        ib_world_object_foreach_by_type("noclip", obj_player_collide_cb, self);

        if (!self->collision_result) {
            obj->pos.y += move_y;
        } else {
            self->base_pos.y -= move_y;
        }

        /* check for player death */
        if (!self->in_blink && !ib_world_aabb(self->base_pos, self->base_size)) {
            ib_world_destroy_object(obj); // lol
            //TODO: Death Sequence
            return;
        }

        /* update camera position */

        float target_cx = obj->pos.x + obj->size.x / 2 - csize.x / 2;
        float target_cy = obj->pos.y + obj->size.y / 2 - csize.y / 2;

        cpos.x = (float) cpos.x + (target_cx - (float) cpos.x) / OBJ_PLAYER_CAMERA_FACTOR;
        cpos.y = (float) cpos.y + (target_cy - (float) cpos.y) / OBJ_PLAYER_CAMERA_FACTOR;

        ib_graphics_set_camera(cpos, csize);
    }
    break;
    case IB_EVT_DRAW:
        ib_graphics_opt_reset();
        ib_graphics_tex_draw_sprite(self->spr, obj->pos);
        break;
    case IB_EVT_INPUT:
    {
        ib_input_event* ie = e->evt;

        if (ie->type == IB_INPUT_EVT_KEYDOWN && ie->scancode == SDL_SCANCODE_SPACE) {
            ib_world_create_object("grenade", NULL, NULL, obj->pos, obj->size, 0.0f, 1);
        }

        /* input case that binds the lshift key */
        if (ie->type == IB_INPUT_EVT_KEYDOWN && ie->scancode == SDL_SCANCODE_LSHIFT && !self->in_blink && !self->in_blink_cd) {
            /* when we blink start the blink steps and cooldown */
            self->in_blink = OBJ_PLAYER_BLINK_STEPS;
            self->in_blink_cd = OBJ_PLAYER_BLINK_COOLDOWN;
        }
    }
    break;
    }
}

/* Destroy func */
void obj_player_destroy(ib_object* p) {
    obj_player* self = p->d;
    ib_sprite_free(self->spr);

    ib_free(self);
}

int obj_player_collide_cb(ib_object* nc, void* d) {
    obj_player* self = d;

    if (ib_util_col_aabb(nc->pos, nc->size, self->base_pos, self->base_size)) {
        self->collision_result = 1;
        return 1;
    }

    return 0;
}
