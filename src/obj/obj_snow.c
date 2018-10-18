#include "obj_snow.h"

#include "../graphics.h"
#include "../event.h"
#include "../mem.h"

/* perspective tricks
 * render one half of the snow above the world and the other half below it. */

/* making the above-world snow consistently collide with the ground uniformly is actually kinda a tough problem */

/* for this we can have an interesting algorithm:
 * for each snowflake select a y-coordinate to "drop" at
 * when we cross the y-coord we only drop if we're over land */

#define OBJ_SNOW_NUM_PARTS 32

typedef struct {
    int x, y, dx, dy;
    float rot, drot, alpha;
    int type;
    int y_drop, dropped, still;
} obj_snow_flake;

typedef struct {
    ib_graphics_texture* flakes[3];
    obj_snow_flake parts[OBJ_SNOW_NUM_PARTS];
} obj_snow;

static int obj_snow_evt(ib_event* e, void* d);

void obj_snow_init(ib_object* p) {
    obj_snow* d = p->d = ib_malloc(sizeof *d);

    d->flakes[0] = ib_graphics_get_texture(IB_GRAPHICS_TEXFILE("snowflake1"));
    d->flakes[1] = ib_graphics_get_texture(IB_GRAPHICS_TEXFILE("snowflake2"));
    d->flakes[2] = ib_graphics_get_texture(IB_GRAPHICS_TEXFILE("snowflake3"));

    int cx, cy;
    ib_graphics_get_camera(&cx, &cy);

    for (int i = 0; i < OBJ_SNOW_NUM_PARTS; ++i) {
        d->parts[i].x = rand() % IB_GRAPHICS_WIDTH + cx;
        d->parts[i].y = rand() % IB_GRAPHICS_HEIGHT + cy;
        d->parts[i].type = rand() % 3;
        d->parts[i].dx = rand() % 2 - 1;
        d->parts[i].dy = rand() % 2 + 1;
        d->parts[i].rot = ((rand() % 100) / 99.0f) * 3.141f * 2.0f;
        d->parts[i].drot = ((rand() % 100) / 99.0f) * 30.0f - 15.0f;
        d->parts[i].alpha = (rand() % 100) / 140.0f;
        d->parts[i].y_drop = cy + (rand() % IB_GRAPHICS_HEIGHT);
        d->parts[i].dropped = 0;
        d->parts[i].still = 0;
    }

    ib_event_subscribe(IB_EVT_DRAW, obj_snow_evt, d);
    ib_event_subscribe(IB_EVT_UPDATE, obj_snow_evt, d);
}

int obj_snow_evt(ib_event* e, void* ed) {
    obj_snow* d = ed;

    int cx, cy, vpw = IB_GRAPHICS_WIDTH, vph = IB_GRAPHICS_HEIGHT;
    ib_graphics_get_camera(&cx, &cy);

    switch (e->type) {
    case IB_EVT_UPDATE:
        {
            for (int i = 0; i < OBJ_SNOW_NUM_PARTS; ++i) {
                if (d->parts[i].still) {
                    if (!--d->parts[i].still) {
                        /* reset dropped particle */
                        d->parts[i].y = -10;
                        d->parts[i].y_drop = cy + rand() % vph;
                        d->parts[i].dropped = 0;
                        d->parts[i].x = rand() % vph;
                    }
                    continue;
                }

                d->parts[i].x += d->parts[i].dx;
                d->parts[i].y += d->parts[i].dy;
                d->parts[i].rot += d->parts[i].drot;

                if (d->parts[i].x + d->flakes[d->parts[i].type]->size.x < cx) d->parts[i].x += vpw;
                if (d->parts[i].x > cx + vpw) d->parts[i].x -= vpw;

                if ((d->parts[i].y > d->parts[i].y_drop) && !d->parts[i].dropped) {
                    ib_graphics_point p;
                    p.x = d->parts[i].x;
                    p.y = d->parts[i].y;

                    if (ib_world_aabb(p, d->flakes[d->parts[i].type]->size)) {
                        d->parts[i].still = 60;
                    }
                }

                if (d->parts[i].y > cy + vph) {
                    d->parts[i].y = cy - 10;
                    d->parts[i].x = cx + rand() % vpw;
                    d->parts[i].y_drop = cy + rand() % vph;
                }
            }
        }
        break;
    case IB_EVT_DRAW:
            ib_graphics_set_space(IB_GRAPHICS_WORLDSPACE);

            for (int i = 0; i < OBJ_SNOW_NUM_PARTS; ++i) {
                ib_graphics_point pos;

                pos.x = d->parts[i].x;
                pos.y = d->parts[i].y;

                ib_graphics_texture* t = d->flakes[d->parts[i].type];
                ib_graphics_draw_texture_ex(t, pos, t->size, d->parts[i].rot, 0, 0, d->parts[i].alpha);
            }
        break;
    }

    return 0;
}

void obj_snow_destroy(ib_object* p) {
    obj_snow* d = p->d;

    for (int i = 0; i < 3; ++i) {
        ib_graphics_drop_texture(d->flakes[i]);
    }
}
