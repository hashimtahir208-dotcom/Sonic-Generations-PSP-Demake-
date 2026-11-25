#include <pspctrl.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <stdio.h>
#include <stdlib.h>
#include <GLES/egl.h>
#include <GLES/gl.h>
#include "glchk.h"

#include <math.h>

#include <stdint.h>
#include <string.h>
#include "sonic-audio.h"
#include "classic-rig-renderer.h"
#include "title-screen-texture.h"
#include "start_button_texture.h"
#include "press_start_texture.h"
#include "green_hill_title_card.h"

static const EGLint attrib_list[] = {
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_ALPHA_SIZE, 0,
    EGL_DEPTH_SIZE, 16,
    EGL_NONE
};

static GLuint id_grass;
static GLuint id_checker;
static GLuint id_title_screen;
static GLuint id_start_button;
static GLuint id_press_start;
static GLuint id_gh_title_card;
static GLuint id_act1_cloud;

static void upload_title_screen_texture(void)
{
    glGenTextures(1, &id_title_screen);
    glBindTexture(GL_TEXTURE_2D, id_title_screen);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 256, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, title_screen_pixels);
}

static void upload_rgba_title_texture(GLuint *id, int width, int height,
                                      const unsigned char *pixels)
{
    glGenTextures(1, id);
    glBindTexture(GL_TEXTURE_2D, *id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

static void upload_gh_title_card_texture(void)
{
    glGenTextures(1, &id_gh_title_card);
    glBindTexture(GL_TEXTURE_2D, id_gh_title_card);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GH_TITLE_TEX_WIDTH,
                 GH_TITLE_TEX_HEIGHT, 0, GL_RGB,
                 GL_UNSIGNED_SHORT_5_6_5, gh_title_card_pixels);
}




#define ACT1_CLOUD_W 64
#define ACT1_CLOUD_H 32

static unsigned char act1_cloud_pixels[
    ACT1_CLOUD_W * ACT1_CLOUD_H * 4
];

static void build_act1_cloud_texture(void)
{
    int x, y;
    for (y = 0; y < ACT1_CLOUD_H; ++y) {
        for (x = 0; x < ACT1_CLOUD_W; ++x) {
            float nx = ((float)x / 63.0f) * 2.0f - 1.0f;
            float ny = ((float)y / 31.0f) * 2.0f - 1.0f;

            float a = (nx * nx) / 0.72f
                    + ((ny + 0.05f) * (ny + 0.05f)) / 0.78f;
            float b = ((nx + 0.43f) * (nx + 0.43f)) / 0.24f
                    + ((ny + 0.08f) * (ny + 0.08f)) / 0.58f;
            float c = ((nx - 0.43f) * (nx - 0.43f)) / 0.24f
                    + ((ny + 0.08f) * (ny + 0.08f)) / 0.58f;

            float d = a;
            float alpha;
            int i = (y * ACT1_CLOUD_W + x) * 4;

            if (b < d) d = b;
            if (c < d) d = c;

            alpha = (1.08f - d) * 3.4f;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;

            act1_cloud_pixels[i + 0] = 238;
            act1_cloud_pixels[i + 1] = 246;
            act1_cloud_pixels[i + 2] = 255;
            act1_cloud_pixels[i + 3] = (unsigned char)(alpha * 255.0f);
        }
    }
}

static void upload_act1_cloud_texture(void)
{
    glGenTextures(1, &id_act1_cloud);
    glBindTexture(GL_TEXTURE_2D, id_act1_cloud);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 ACT1_CLOUD_W, ACT1_CLOUD_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, act1_cloud_pixels);
}

/* Procedural, seamless 128x128 Green Hill-inspired grass. */
#define GRASS_TEX_SIZE 128

static unsigned char grass_hd_texture[
    GRASS_TEX_SIZE * GRASS_TEX_SIZE * 3
];

static float grass_hash(int x, int y, int cells)
{
    unsigned int n;

    x = (x % cells + cells) % cells;
    y = (y % cells + cells) % cells;

    n = (unsigned int)x * 374761393u;
    n += (unsigned int)y * 668265263u;
    n += (unsigned int)cells * 1442695041u;
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= n >> 16;

    return (float)(n & 0x00ffffffu) / 16777215.0f;
}

static float grass_noise(float px, float py, int cells)
{
    float gx, gy, fx, fy;
    float a, b, c, d;
    float sx, sy;
    int x, y;

    gx = px * (float)cells / 128.0f;
    gy = py * (float)cells / 128.0f;

    x = (int)floorf(gx);
    y = (int)floorf(gy);

    fx = gx - (float)x;
    fy = gy - (float)y;

    sx = fx * fx * (3.0f - 2.0f * fx);
    sy = fy * fy * (3.0f - 2.0f * fy);

    a = grass_hash(x,     y,     cells);
    b = grass_hash(x + 1, y,     cells);
    c = grass_hash(x,     y + 1, cells);
    d = grass_hash(x + 1, y + 1, cells);

    return (a * (1.0f - sx) + b * sx) * (1.0f - sy)
         + (c * (1.0f - sx) + d * sx) * sy;
}

static void build_grass_texture(void)
{
    int x, y, i;
    float broad, medium, fine, grain;
    float blades, blades2, detail;
    float green, red, blue;
    float value;
    float phase1, phase2;

    for (y = 0; y < 128; y++) {
        for (x = 0; x < 128; x++) {

            broad = grass_noise((float)x, (float)y, 4);
            medium = grass_noise((float)x, (float)y, 16);
            fine = grass_noise((float)x, (float)y, 64);
            grain = grass_hash(x, y, 128);

            phase1 = 6.2831853f *
                (11.0f * (float)x + 3.0f * (float)y) / 128.0f;

            phase2 = 6.2831853f *
                (7.0f * (float)x - 5.0f * (float)y) / 128.0f;

            blades = 0.5f + 0.5f * sinf(phase1);
            blades2 = 0.5f + 0.5f * sinf(phase2);

            detail = (broad - 0.5f) * 0.42f
                   + (medium - 0.5f) * 0.30f
                   + (fine - 0.5f) * 0.16f
                   + (grain - 0.5f) * 0.08f;

            value = 0.53f + detail
                  + (blades - 0.5f) * 0.045f
                  + (blades2 - 0.5f) * 0.035f;

            red   = 18.0f + value * 29.0f;
            green = 75.0f + value * 105.0f;
            blue  = 12.0f + value * 20.0f;

            /* Fine highlights and darker blade separation. */
            if (blades > 0.985f && grain > 0.28f) {
                green += 13.0f;
                red += 2.0f;
            }

            if (blades2 > 0.99f && grain > 0.55f)
                green += 9.0f;

            if (grain < 0.12f) {
                green -= 9.0f;
                red -= 2.0f;
            }

            if (red < 0.0f) red = 0.0f;
            if (red > 255.0f) red = 255.0f;
            if (green < 0.0f) green = 0.0f;
            if (green > 255.0f) green = 255.0f;
            if (blue < 0.0f) blue = 0.0f;
            if (blue > 255.0f) blue = 255.0f;

            i = (y * 128 + x) * 3;

            grass_hd_texture[i]     = (unsigned char)red;
            grass_hd_texture[i + 1] = (unsigned char)green;
            grass_hd_texture[i + 2] = (unsigned char)blue;
        }
    }
}

static void upload_grass_hd_texture(GLuint *id)
{
    glGenTextures(1, id);
    glBindTexture(GL_TEXTURE_2D, *id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 128, 128, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, grass_hd_texture);
}


/* Classic Sonic 1 Green Hill brown checkerboard. */
static unsigned char ghz_checker_texture[64 * 64 * 3];

static void build_ghz_checker_texture(void)
{
    int x, y, i;
    int dark;

    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            dark = (((x / 16) + (y / 16)) & 1);
            i = (y * 64 + x) * 3;

            if (dark) {
                ghz_checker_texture[i]     = 76;
                ghz_checker_texture[i + 1] = 36;
                ghz_checker_texture[i + 2] = 17;
            } else {
                ghz_checker_texture[i]     = 133;
                ghz_checker_texture[i + 1] = 77;
                ghz_checker_texture[i + 2] = 36;
            }
        }
    }
}

static void upload_ghz_checker_texture(GLuint *id)
{
    glGenTextures(1, id);
    glBindTexture(GL_TEXTURE_2D, *id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, ghz_checker_texture);
}

static float approach_value(float current, float target, float step)
{
    if (current < target) {
        current += step;
        if (current > target) current = target;
    } else if (current > target) {
        current -= step;
        if (current < target) current = target;
    }

    return current;
}

/* Green Hill: narrow grass route, brown cliffs and surrounding water. */

struct TerrainVertex {
    GLfloat x, y, z, u, v;
};

static const struct TerrainVertex grass_vertices[] = {
    {-3.12f,-1.02f,-14.0f, 0.0f,0.0f},
    { 3.12f,-1.02f,-14.0f, 4.0f,0.0f},
    { 3.12f,-1.02f,  8.0f, 4.0f,10.0f},
    {-3.12f,-1.02f,-14.0f, 0.0f,0.0f},
    { 3.12f,-1.02f,  8.0f, 4.0f,10.0f},
    {-3.12f,-1.02f,  8.0f, 0.0f,10.0f}
};



static void draw_wall(GLfloat inner, GLfloat outer)
{
    const GLfloat y0 = -1.02f;
    const GLfloat y1 = -0.47f;
    const GLfloat z0 = -14.0f;
    const GLfloat z1 = 8.0f;

    struct TerrainVertex wall[] = {
        /* Inner face */
        {inner,y0,z0, 0.0f,0.5f},
        {inner,y1,z0, 0.0f,0.0f},
        {inner,y1,z1, 5.5f,0.0f},
        {inner,y0,z0, 0.0f,0.5f},
        {inner,y1,z1, 5.5f,0.0f},
        {inner,y0,z1, 5.5f,0.5f},

        /* Outer face */
        {outer,y0,z0, 0.0f,0.5f},
        {outer,y1,z1, 5.5f,0.0f},
        {outer,y1,z0, 0.0f,0.0f},
        {outer,y0,z0, 0.0f,0.5f},
        {outer,y0,z1, 5.5f,0.5f},
        {outer,y1,z1, 5.5f,0.0f},

        /* Top surface */
        {inner,y1,z0, 0.0f,0.5f},
        {outer,y1,z0, 0.0f,0.5f},
        {outer,y1,z1, 5.5f,0.5f},
        {inner,y1,z0, 0.0f,0.5f},
        {outer,y1,z1, 5.5f,0.5f},
        {inner,y1,z1, 5.5f,0.5f}
    };

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, id_checker);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glVertexPointer(3, GL_FLOAT, sizeof(struct TerrainVertex),
                    &wall[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct TerrainVertex),
                      &wall[0].u);

    glDrawArrays(GL_TRIANGLES, 0, 18);
}

static void draw_valley_side(GLfloat side)
{
    GLfloat water_near[] = {
        side*3.42f,-1.44f,-14.0f,
        side*7.0f,-1.44f,-14.0f,
        side*7.0f,-1.44f,  8.0f,
        side*3.42f,-1.44f,-14.0f,
        side*7.0f,-1.44f,  8.0f,
        side*3.42f,-1.44f,  8.0f
    };

    GLfloat water_mid[] = {
        side*7.0f,-1.45f,-14.0f,
        side*11.0f,-1.45f,-14.0f,
        side*11.0f,-1.45f,  8.0f,
        side*7.0f,-1.45f,-14.0f,
        side*11.0f,-1.45f,  8.0f,
        side*7.0f,-1.45f,  8.0f
    };

    GLfloat water_far[] = {
        side*11.0f,-1.46f,-14.0f,
        side*18.0f,-1.46f,-14.0f,
        side*18.0f,-1.46f,  8.0f,
        side*11.0f,-1.46f,-14.0f,
        side*18.0f,-1.46f,  8.0f,
        side*11.0f,-1.46f,  8.0f
    };

    glVertexPointer(3, GL_FLOAT, 0, water_near);
    glColor4f(0.025f, 0.38f, 0.55f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glVertexPointer(3, GL_FLOAT, 0, water_mid);
    glColor4f(0.035f, 0.27f, 0.46f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glVertexPointer(3, GL_FLOAT, 0, water_far);
    glColor4f(0.055f, 0.18f, 0.36f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}


/* Textured original Sonic 1 GHZ-inspired hexagonal pillars. */
static void draw_ghz_pillar(GLfloat cx, GLfloat cz,
                            GLfloat height, GLfloat radius)
{
    struct TerrainVertex sides[36];
    struct TerrainVertex top[18];

    const GLfloat base_y = -1.42f;
    const GLfloat top_y = base_y + height;
    const GLfloat pi = 3.14159265f;

    int i, n = 0, t = 0;

    for (i = 0; i < 6; ++i) {
        GLfloat a0 = 2.0f * pi * (GLfloat)i / 6.0f;
        GLfloat a1 = 2.0f * pi * (GLfloat)(i + 1) / 6.0f;

        GLfloat x0 = cx + cosf(a0) * radius;
        GLfloat z0 = cz + sinf(a0) * radius;
        GLfloat x1 = cx + cosf(a1) * radius;
        GLfloat z1 = cz + sinf(a1) * radius;

        GLfloat u0 = 0.0f;
        GLfloat u1 = 1.0f;

        struct TerrainVertex quad[6] = {
            {x0,base_y,z0,u0,1.0f},
            {x1,base_y,z1,u1,1.0f},
            {x1,top_y,z1,u1,0.0f},
            {x0,base_y,z0,u0,1.0f},
            {x1,top_y,z1,u1,0.0f},
            {x0,top_y,z0,u0,0.0f}
        };

        int k;
        for (k = 0; k < 6; ++k)
            sides[n++] = quad[k];

        top[t++] = (struct TerrainVertex){
            cx,top_y,cz,0.5f,0.5f
        };
        top[t++] = (struct TerrainVertex){
            x0,top_y,z0,
            0.5f + 0.5f*cosf(a0),
            0.5f + 0.5f*sinf(a0)
        };
        top[t++] = (struct TerrainVertex){
            x1,top_y,z1,
            0.5f + 0.5f*cosf(a1),
            0.5f + 0.5f*sinf(a1)
        };
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, id_checker);
    glColor4f(1.0f,1.0f,1.0f,1.0f);

    glVertexPointer(3, GL_FLOAT, sizeof(struct TerrainVertex),
                    &sides[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct TerrainVertex),
                      &sides[0].u);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glVertexPointer(3, GL_FLOAT, sizeof(struct TerrainVertex),
                    &top[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct TerrainVertex),
                      &top[0].u);
    glDrawArrays(GL_TRIANGLES, 0, 18);
}

static void draw_ghz_pillars(void)
{
    /* Staggered GHZ pillars, alternating outside the left and right walls. */
    draw_ghz_pillar(-4.55f, -12.0f, 0.62f, 0.25f);
    draw_ghz_pillar( 4.65f,  -8.0f, 0.76f, 0.30f);
    draw_ghz_pillar(-4.85f,  -2.0f, 0.94f, 0.36f);
    draw_ghz_pillar( 5.05f,   4.0f, 1.08f, 0.42f);
}

/* Low-poly rounded cloud puffs. */
static void draw_cloud_puff(GLfloat cx, GLfloat cy, GLfloat cz,
                            GLfloat rx, GLfloat ry, GLfloat rz)
{
    GLfloat vertices[8 * 4 * 6 * 3];
    int n = 0;
    int lat, lon;

    const GLfloat pi = 3.14159265f;

    for (lat = 0; lat < 4; ++lat) {
        GLfloat p0 = -pi*0.5f + pi*(GLfloat)lat/4.0f;
        GLfloat p1 = -pi*0.5f + pi*(GLfloat)(lat+1)/4.0f;

        for (lon = 0; lon < 8; ++lon) {
            GLfloat t0 = 2.0f*pi*(GLfloat)lon/8.0f;
            GLfloat t1 = 2.0f*pi*(GLfloat)(lon+1)/8.0f;

            GLfloat p[4][3];

            p[0][0]=cx+rx*cosf(p0)*cosf(t0);
            p[0][1]=cy+ry*sinf(p0);
            p[0][2]=cz+rz*cosf(p0)*sinf(t0);

            p[1][0]=cx+rx*cosf(p1)*cosf(t0);
            p[1][1]=cy+ry*sinf(p1);
            p[1][2]=cz+rz*cosf(p1)*sinf(t0);

            p[2][0]=cx+rx*cosf(p1)*cosf(t1);
            p[2][1]=cy+ry*sinf(p1);
            p[2][2]=cz+rz*cosf(p1)*sinf(t1);

            p[3][0]=cx+rx*cosf(p0)*cosf(t1);
            p[3][1]=cy+ry*sinf(p0);
            p[3][2]=cz+rz*cosf(p0)*sinf(t1);

            {
                int order[6] = {0,1,2,0,2,3};
                int k;
                for (k = 0; k < 6; ++k) {
                    int q = order[k];
                    vertices[n++] = p[q][0];
                    vertices[n++] = p[q][1];
                    vertices[n++] = p[q][2];
                }
            }
        }
    }

    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLES, 0, n/3);
}

static void draw_clouds(void)
{
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.91f, 0.96f, 1.0f, 1.0f);

    draw_cloud_puff(-5.0f, 7.20f, -10.0f, 1.05f, 0.42f, 0.48f);
    draw_cloud_puff(-5.9f, 7.05f, -10.0f, 0.72f, 0.34f, 0.42f);
    draw_cloud_puff(-4.2f, 7.05f, -10.0f, 0.75f, 0.35f, 0.43f);

    draw_cloud_puff(5.3f, 7.50f, -7.0f, 1.15f, 0.44f, 0.52f);
    draw_cloud_puff(4.4f, 7.35f, -7.0f, 0.76f, 0.34f, 0.43f);
    draw_cloud_puff(6.2f, 7.32f, -7.0f, 0.78f, 0.36f, 0.44f);

    draw_cloud_puff(0.0f, 8.10f, -15.0f, 1.2f, 0.43f, 0.48f);
    draw_cloud_puff(-0.9f, 7.95f, -15.0f, 0.75f, 0.34f, 0.40f);
    draw_cloud_puff(1.0f, 7.95f, -15.0f, 0.75f, 0.34f, 0.40f);
}


/* Append one textured quad as two triangles. */
static void stage_add_quad(struct TerrainVertex *v, int *n,
                           const GLfloat a[3], const GLfloat b[3],
                           const GLfloat c[3], const GLfloat d[3])
{
    v[(*n)++] = (struct TerrainVertex){a[0],a[1],a[2],0.0f,1.0f};
    v[(*n)++] = (struct TerrainVertex){b[0],b[1],b[2],0.0f,0.0f};
    v[(*n)++] = (struct TerrainVertex){c[0],c[1],c[2],2.0f,0.0f};
    v[(*n)++] = (struct TerrainVertex){a[0],a[1],a[2],0.0f,1.0f};
    v[(*n)++] = (struct TerrainVertex){c[0],c[1],c[2],2.0f,0.0f};
    v[(*n)++] = (struct TerrainVertex){d[0],d[1],d[2],2.0f,1.0f};
}

/* Checkerboard cliff with a separate grass-covered top. */
static void draw_ghz_cliff_v2(GLfloat side, GLfloat z0,
                              GLfloat z1, GLfloat top_y)
{
    GLfloat x0=side*5.1f, x1=side*9.0f;
    GLfloat y0=-1.45f, y1=top_y;
    GLfloat a[3],b[3],c[3],d[3];
    struct TerrainVertex v[36];
    struct TerrainVertex cap[6];
    int n=0;

    a[0]=x0;a[1]=y0;a[2]=z0;
    b[0]=x0;b[1]=y1;b[2]=z0;
    c[0]=x0;c[1]=y1;c[2]=z1;
    d[0]=x0;d[1]=y0;d[2]=z1;
    stage_add_quad(v,&n,a,b,c,d);

    a[0]=x1;a[1]=y0;a[2]=z0;
    b[0]=x1;b[1]=y0;b[2]=z1;
    c[0]=x1;c[1]=y1;c[2]=z1;
    d[0]=x1;d[1]=y1;d[2]=z0;
    stage_add_quad(v,&n,a,b,c,d);

    a[0]=x0;a[1]=y0;a[2]=z0;
    b[0]=x1;b[1]=y0;b[2]=z0;
    c[0]=x1;c[1]=y1;c[2]=z0;
    d[0]=x0;d[1]=y1;d[2]=z0;
    stage_add_quad(v,&n,a,b,c,d);

    a[0]=x0;a[1]=y0;a[2]=z1;
    b[0]=x0;b[1]=y1;b[2]=z1;
    c[0]=x1;c[1]=y1;c[2]=z1;
    d[0]=x1;d[1]=y0;d[2]=z1;
    stage_add_quad(v,&n,a,b,c,d);

    a[0]=x0;a[1]=y0;a[2]=z0;
    b[0]=x0;b[1]=y0;b[2]=z1;
    c[0]=x1;c[1]=y0;c[2]=z1;
    d[0]=x1;d[1]=y0;d[2]=z0;
    stage_add_quad(v,&n,a,b,c,d);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,id_checker);
    glColor4f(1,1,1,1);
    glVertexPointer(3,GL_FLOAT,sizeof(struct TerrainVertex),&v[0].x);
    glTexCoordPointer(2,GL_FLOAT,sizeof(struct TerrainVertex),&v[0].u);
    glDrawArrays(GL_TRIANGLES,0,n);

    cap[0]=(struct TerrainVertex){x0,y1,z0,0,0};
    cap[1]=(struct TerrainVertex){x1,y1,z0,3,0};
    cap[2]=(struct TerrainVertex){x1,y1,z1,3,2};
    cap[3]=(struct TerrainVertex){x0,y1,z0,0,0};
    cap[4]=(struct TerrainVertex){x1,y1,z1,3,2};
    cap[5]=(struct TerrainVertex){x0,y1,z1,0,2};

    glBindTexture(GL_TEXTURE_2D,id_grass);
    glVertexPointer(3,GL_FLOAT,sizeof(struct TerrainVertex),&cap[0].x);
    glTexCoordPointer(2,GL_FLOAT,sizeof(struct TerrainVertex),&cap[0].u);
    glDrawArrays(GL_TRIANGLES,0,6);

    /* Waterfall strips on the inward-facing cliff surface. */
    {
        GLfloat wz=z0+(z1-z0)*0.42f;
        GLfloat wx=x0;
        GLfloat width=(z1-z0)*0.16f;
        GLfloat water[18]={
            wx,y1-0.18f,wz,
            wx,y0+0.08f,wz+width,
            wx,y0+0.08f,wz,
            wx,y1-0.18f,wz,
            wx,y1-0.18f,wz+width,
            wx,y0+0.08f,wz+width
        };
        glDisable(GL_TEXTURE_2D);
        glColor4f(0.16f,0.78f,0.96f,1.0f);
        glVertexPointer(3,GL_FLOAT,0,water);
        glDrawArrays(GL_TRIANGLES,0,6);

        water[2]+=0.02f; water[5]+=0.02f;
        water[8]+=0.02f; water[11]+=0.02f;
        water[14]+=0.02f; water[17]+=0.02f;
        glColor4f(0.65f,0.93f,1.0f,1.0f);
        glVertexPointer(3,GL_FLOAT,0,water);
        glDrawArrays(GL_TRIANGLES,0,6);
        glEnable(GL_TEXTURE_2D);
    }

    /* Bright grass lip along the visible inner edge of the cliff. */
    {
        GLfloat lip[18] = {
            x0 - side * 0.035f, y1 - 0.16f, z0,
            x0 - side * 0.035f, y1 - 0.035f, z0,
            x0 - side * 0.035f, y1 - 0.035f, z1,
            x0 - side * 0.035f, y1 - 0.16f, z0,
            x0 - side * 0.035f, y1 - 0.035f, z1,
            x0 - side * 0.035f, y1 - 0.16f, z1
        };

        glDisable(GL_TEXTURE_2D);
        glColor4f(0.22f, 0.78f, 0.16f, 1.0f);
        glVertexPointer(3, GL_FLOAT, 0, lip);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_TEXTURE_2D);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

/* Layered checkerboard formations on both sides of the route. */
static void draw_ghz_cliffs_v2(void)
{
    /* Longer, stepped checkerboard walls framing the route. */
    draw_ghz_cliff_v2(-1.0f,-17.0f,-8.0f,1.55f);
    draw_ghz_cliff_v2( 1.0f,-17.0f,-8.0f,1.95f);
    draw_ghz_cliff_v2(-1.0f,-8.0f, 0.0f,0.85f);
    draw_ghz_cliff_v2( 1.0f,-8.0f, 0.0f,1.25f);
    draw_ghz_cliff_v2(-1.0f, 0.0f, 9.0f,1.35f);
    draw_ghz_cliff_v2( 1.0f, 0.0f, 9.0f,1.65f);
}

/* Rounded distant green hills reuse the existing low-poly sphere mesh. */

/* GHZ Act 2 tropical palm scenery. */
static void draw_ghz_palm_tree(GLfloat x, GLfloat y, GLfloat z,
                               GLfloat scale)
{
    GLfloat trunk[8 * 6 * 6 * 3];
    GLfloat leaves[8 * 6 * 3];
    const GLfloat pi = 3.14159265f;
    GLfloat top_y = y + 1.75f * scale;
    int i, j, n = 0, ln = 0;

    for (i = 0; i < 8; ++i) {
        GLfloat t0 = (GLfloat)i / 8.0f;
        GLfloat t1 = (GLfloat)(i + 1) / 8.0f;
        GLfloat y0 = y + 1.75f * scale * t0;
        GLfloat y1 = y + 1.75f * scale * t1;
        GLfloat r0 = scale * (0.105f - 0.035f * t0);
        GLfloat r1 = scale * (0.105f - 0.035f * t1);

        for (j = 0; j < 6; ++j) {
            GLfloat a0 = 2.0f*pi*(GLfloat)j/6.0f;
            GLfloat a1 = 2.0f*pi*(GLfloat)(j+1)/6.0f;
            GLfloat x0 = x + 0.08f*scale*t0 + cosf(a0)*r0;
            GLfloat z0 = z + sinf(a0)*r0;
            GLfloat x1 = x + 0.08f*scale*t0 + cosf(a1)*r0;
            GLfloat z1 = z + sinf(a1)*r0;
            GLfloat x2 = x + 0.08f*scale*t1 + cosf(a1)*r1;
            GLfloat z2 = z + sinf(a1)*r1;
            GLfloat x3 = x + 0.08f*scale*t1 + cosf(a0)*r1;
            GLfloat z3 = z + sinf(a0)*r1;
            GLfloat v[18] = {
                x0,y0,z0, x1,y0,z1, x2,y1,z2,
                x0,y0,z0, x2,y1,z2, x3,y1,z3
            };
            int k;
            for (k = 0; k < 18; ++k) trunk[n++] = v[k];
        }
    }

    glDisable(GL_TEXTURE_2D);
    glColor4f(0.43f,0.22f,0.09f,1.0f);
    glVertexPointer(3,GL_FLOAT,0,trunk);
    glDrawArrays(GL_TRIANGLES,0,n/3);

    for (i = 0; i < 8; ++i) {
        GLfloat a = 2.0f*pi*(GLfloat)i/8.0f;
        GLfloat dx = cosf(a), dz = sinf(a);
        GLfloat px = -dz*0.20f*scale;
        GLfloat pz =  dx*0.20f*scale;
        GLfloat bx = x+0.08f*scale, bz = z;
        GLfloat mx = bx+dx*0.62f*scale;
        GLfloat mz = bz+dz*0.62f*scale;
        GLfloat tx = bx+dx*1.30f*scale;
        GLfloat tz = bz+dz*1.30f*scale;
        GLfloat v[18] = {
            bx,top_y,bz, mx+px,top_y-0.16f*scale,mz+pz,
            tx,top_y-0.52f*scale,tz,
            bx,top_y,bz, tx,top_y-0.52f*scale,tz,
            mx-px,top_y-0.16f*scale,mz-pz
        };
        int k;
        for (k = 0; k < 18; ++k) leaves[ln++] = v[k];
    }

    glColor4f(0.12f,0.55f,0.13f,1.0f);
    glVertexPointer(3,GL_FLOAT,0,leaves);
    glDrawArrays(GL_TRIANGLES,0,ln/3);
}

static void draw_ghz_palm_trees(void)
{
    draw_ghz_palm_tree(-6.6f,1.35f,-12.0f,1.00f);
    draw_ghz_palm_tree( 6.6f,1.75f,-11.0f,1.10f);
    draw_ghz_palm_tree(-7.0f,1.35f, -7.0f,0.90f);
    draw_ghz_palm_tree( 7.0f,1.75f, -6.5f,0.95f);
    draw_ghz_palm_tree(-6.6f,0.65f, -2.0f,0.90f);
    draw_ghz_palm_tree( 6.6f,0.95f, -1.5f,1.00f);
    draw_ghz_palm_tree(-7.0f,0.65f,  2.5f,0.85f);
    draw_ghz_palm_tree( 7.0f,0.95f,  3.0f,0.90f);
}

static void draw_ghz_hills_v2(void)
{
    glDisable(GL_TEXTURE_2D);

    glColor4f(0.12f,0.55f,0.19f,1.0f);
    draw_cloud_puff(-9.0f,-0.45f,-13.0f,3.8f,2.0f,3.2f);
    draw_cloud_puff( 9.0f,-0.35f,-12.0f,4.0f,2.3f,3.5f);

    glColor4f(0.10f,0.43f,0.17f,1.0f);
    draw_cloud_puff(-14.0f,-0.75f,-10.0f,4.5f,2.5f,3.8f);
    draw_cloud_puff( 14.0f,-0.75f,-10.0f,4.5f,2.5f,3.8f);

    glColor4f(0.20f,0.68f,0.23f,1.0f);
    draw_cloud_puff(-10.0f,-0.65f,-5.0f,2.8f,1.6f,2.7f);
    draw_cloud_puff( 10.0f,-0.60f,-4.0f,3.0f,1.7f,2.8f);

    glColor4f(0.16f,0.52f,0.19f,1.0f);
    draw_cloud_puff(-13.0f,-0.85f,-1.0f,3.8f,2.1f,3.2f);
    draw_cloud_puff( 13.0f,-0.85f,-1.0f,3.8f,2.1f,3.2f);

    glEnable(GL_TEXTURE_2D);
}

static GLfloat ghz_ground_height_v2(GLfloat z)
{
    GLfloat d = z + 12.5f;
    return -1.02f
         + 0.42f * sinf(d * 0.48f)
         + 0.12f * sinf(d * 0.96f);
}


/* Act 1 Green Hill scenery: far-side layers follow the scrolling camera. */
static void draw_act1_green_hill_background(GLfloat center_z)
{
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, center_z + 12.0f);

    /* Distant rounded green hills, behind the checkerboard formations. */
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.10f, 0.39f, 0.17f, 1.0f);
    draw_cloud_puff(8.8f, -0.15f, -13.0f, 2.7f, 1.8f, 5.2f);
    draw_cloud_puff(9.0f, -0.20f,  -5.0f, 3.0f, 2.0f, 5.0f);
    glColor4f(0.16f, 0.53f, 0.20f, 1.0f);
    draw_cloud_puff(8.5f,  0.10f,   2.0f, 2.5f, 1.7f, 4.5f);

    /* Checkerboard formations only on the far side:
       the near side remains open so it cannot hide Classic Sonic. */
    draw_ghz_cliff_v2(1.0f, -17.0f, -8.0f, 1.95f);
    draw_ghz_cliff_v2(1.0f,  -8.0f,  0.0f, 1.25f);
    draw_ghz_cliff_v2(1.0f,   0.0f,  9.0f, 1.65f);

    /* Palm trees sit on the far-side cliff tops. */
    draw_ghz_palm_tree(6.6f, 1.95f, -13.0f, 1.00f);
    draw_ghz_palm_tree(7.0f, 1.25f,  -5.0f, 0.92f);
    draw_ghz_palm_tree(6.7f, 1.65f,   3.0f, 1.05f);

    /* High clouds complete the Green Hill skyline. */
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.91f, 0.96f, 1.0f, 1.0f);
    draw_cloud_puff(0.0f, 7.5f, -14.0f, 1.8f, 0.48f, 0.75f);
    draw_cloud_puff(4.0f, 7.9f,  -5.0f, 1.9f, 0.52f, 0.82f);
    draw_cloud_puff(1.5f, 8.1f,   5.0f, 1.7f, 0.46f, 0.72f);

    glPopMatrix();
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/* Small reusable grass blades and flower clusters beside the running lane. */
static GLfloat act1_ground_height(GLfloat z);

static void draw_act1_grass_and_flowers(GLfloat center_z)
{
    GLfloat blades[12 * 2 * 3 * 9];
    GLfloat flowers[12 * 2 * 4 * 9];
    int bn = 0, fn = 0;
    int i, side, j;

    for (i = 0; i < 12; ++i) {
        GLfloat z = center_z - 14.3f + (GLfloat)i * 2.55f;
        GLfloat y = act1_ground_height(z) + 0.025f;

        for (side = 0; side < 2; ++side) {
            GLfloat x = side ? 2.27f : -2.27f;
            GLfloat h = 0.13f + (GLfloat)((i * 7 + side * 3) % 5) * 0.025f;

            for (j = 0; j < 3; ++j) {
                GLfloat zz = z + ((GLfloat)j - 1.0f) * 0.105f;
                GLfloat tip = zz + ((j & 1) ? 0.07f : -0.07f);
                blades[bn++] = x; blades[bn++] = y;      blades[bn++] = zz - 0.055f;
                blades[bn++] = x; blades[bn++] = y;      blades[bn++] = zz + 0.055f;
                blades[bn++] = x; blades[bn++] = y + h;  blades[bn++] = tip;
            }

            if ((i + side) % 3 == 0) {
                GLfloat fy = y + h + 0.035f;
                GLfloat fz = z + 0.015f;
                GLfloat r = 0.055f;

                /* Four simple petal triangles around a bright centre. */
                flowers[fn++] = x; flowers[fn++] = fy; flowers[fn++] = fz;
                flowers[fn++] = x; flowers[fn++] = fy + r; flowers[fn++] = fz;
                flowers[fn++] = x; flowers[fn++] = fy; flowers[fn++] = fz + r;

                flowers[fn++] = x; flowers[fn++] = fy; flowers[fn++] = fz;
                flowers[fn++] = x; flowers[fn++] = fy; flowers[fn++] = fz + r;
                flowers[fn++] = x; flowers[fn++] = fy - r; flowers[fn++] = fz;

                flowers[fn++] = x; flowers[fn++] = fy; flowers[fn++] = fz;
                flowers[fn++] = x; flowers[fn++] = fy - r; flowers[fn++] = fz;
                flowers[fn++] = x; flowers[fn++] = fy; flowers[fn++] = fz - r;

                flowers[fn++] = x; flowers[fn++] = fy; flowers[fn++] = fz;
                flowers[fn++] = x; flowers[fn++] = fy; flowers[fn++] = fz - r;
                flowers[fn++] = x; flowers[fn++] = fy + r; flowers[fn++] = fz;
            }
        }
    }

    glDisable(GL_TEXTURE_2D);
    glVertexPointer(3, GL_FLOAT, 0, blades);
    glColor4f(0.12f, 0.48f, 0.12f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, bn / 3);

    glVertexPointer(3, GL_FLOAT, 0, flowers);
    glColor4f(1.0f, 0.82f, 0.20f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, fn / 3);

    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}



/* Textured cloud cards, positioned behind the Act 1 play lane. */
static void draw_act1_textured_clouds(GLfloat center_z)
{
    struct TerrainVertex v[18];
    const GLfloat x = 3.0f;
    const GLfloat z_offsets[3] = {-10.0f, -1.0f, 8.0f};
    const GLfloat y_values[3] = {6.5f, 7.0f, 6.3f};
    const GLfloat widths[3] = {2.3f, 2.0f, 2.5f};
    const GLfloat heights[3] = {0.85f, 0.75f, 0.90f};
    int i, n = 0;

    for (i = 0; i < 3; ++i) {
        GLfloat z = center_z + z_offsets[i];
        GLfloat y = y_values[i];
        GLfloat w = widths[i];
        GLfloat h = heights[i];

        v[n++] = (struct TerrainVertex){x,y-h,z-w,0.0f,1.0f};
        v[n++] = (struct TerrainVertex){x,y+h,z-w,0.0f,0.0f};
        v[n++] = (struct TerrainVertex){x,y+h,z+w,1.0f,0.0f};

        v[n++] = (struct TerrainVertex){x,y-h,z-w,0.0f,1.0f};
        v[n++] = (struct TerrainVertex){x,y+h,z+w,1.0f,0.0f};
        v[n++] = (struct TerrainVertex){x,y-h,z+w,1.0f,1.0f};
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, id_act1_cloud);
    glColor4f(1.0f,1.0f,1.0f,0.96f);

    glVertexPointer(3, GL_FLOAT, sizeof(struct TerrainVertex), &v[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct TerrainVertex), &v[0].u);
    glDrawArrays(GL_TRIANGLES, 0, n);

    glDisable(GL_BLEND);
    glColor4f(1.0f,1.0f,1.0f,1.0f);
}

/* Three small floating islands with textured grass tops and rock undersides. */
static void draw_act1_floating_islands(GLfloat center_z)
{
    const GLfloat x0 = 1.05f;
    const GLfloat x1 = 2.55f;
    const GLfloat z_offsets[3] = {-8.0f, 0.5f, 8.0f};
    const GLfloat heights[3] = {2.65f, 3.15f, 2.45f};
    const GLfloat half_widths[3] = {1.15f, 0.95f, 1.25f};

    int i;

    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f,1.0f,1.0f,1.0f);

    for (i = 0; i < 3; ++i) {
        struct TerrainVertex top[6];
        struct TerrainVertex rock[12];
        GLfloat z0 = center_z + z_offsets[i] - half_widths[i];
        GLfloat z1 = center_z + z_offsets[i] + half_widths[i];
        GLfloat y = heights[i];
        GLfloat bottom[3] = {
            (x0 + x1) * 0.5f,
            y - 0.85f,
            (z0 + z1) * 0.5f
        };
        GLfloat corners[4][3] = {
            {x0,y,z0}, {x1,y,z0}, {x1,y,z1}, {x0,y,z1}
        };
        int j, rn = 0;

        top[0] = (struct TerrainVertex){x0,y,z0,0.0f,0.0f};
        top[1] = (struct TerrainVertex){x1,y,z0,2.0f,0.0f};
        top[2] = (struct TerrainVertex){x1,y,z1,2.0f,1.0f};
        top[3] = (struct TerrainVertex){x0,y,z0,0.0f,0.0f};
        top[4] = (struct TerrainVertex){x1,y,z1,2.0f,1.0f};
        top[5] = (struct TerrainVertex){x0,y,z1,0.0f,1.0f};

        for (j = 0; j < 4; ++j) {
            int next = (j + 1) & 3;
            rock[rn++] = (struct TerrainVertex){
                corners[j][0],corners[j][1],corners[j][2],0.0f,0.0f
            };
            rock[rn++] = (struct TerrainVertex){
                corners[next][0],corners[next][1],corners[next][2],1.0f,0.0f
            };
            rock[rn++] = (struct TerrainVertex){
                bottom[0],bottom[1],bottom[2],0.5f,1.0f
            };
        }

        glBindTexture(GL_TEXTURE_2D, id_checker);
        glVertexPointer(3, GL_FLOAT, sizeof(struct TerrainVertex), &rock[0].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(struct TerrainVertex), &rock[0].u);
        glDrawArrays(GL_TRIANGLES, 0, rn);

        glBindTexture(GL_TEXTURE_2D, id_grass);
        glVertexPointer(3, GL_FLOAT, sizeof(struct TerrainVertex), &top[0].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(struct TerrainVertex), &top[0].u);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

/* Sonic Anniversary Act 1: dedicated side-scrolling ground profile. */
static GLfloat act1_ground_height(GLfloat z)
{
    GLfloat d = z + 12.0f;

    return -1.05f
         + 0.72f * sinf(d * 0.30f)
         + 0.20f * sinf(d * 0.60f)
         + 0.07f * sinf(d * 1.20f);
}

/* Continuous side-scrolling Act 1 ground: grass top and checkerboard earth. */
static void draw_act1_terrain(GLfloat center_z)
{
    const int segments = 48;
    const GLfloat z_start = center_z - 18.0f;
    const GLfloat step = 0.75f;
    const GLfloat front_x = -2.45f;
    const GLfloat back_x = 2.45f;
    const GLfloat bottom_y = -5.0f;

    struct TerrainVertex earth[48 * 6];
    struct TerrainVertex grass[48 * 6];
    int i, n = 0;

    for (i = 0; i < segments; ++i) {
        GLfloat za = z_start + (GLfloat)i * step;
        GLfloat zb = za + step;
        GLfloat ya = act1_ground_height(za);
        GLfloat yb = act1_ground_height(zb);
        GLfloat ua = (GLfloat)i * 0.42f;
        GLfloat ub = (GLfloat)(i + 1) * 0.42f;

        /* Camera-facing vertical checkerboard earth face. */
        earth[n++] = (struct TerrainVertex){front_x, bottom_y, za, ua, 2.8f};
        earth[n++] = (struct TerrainVertex){front_x, ya, za, ua, 0.0f};
        earth[n++] = (struct TerrainVertex){front_x, yb, zb, ub, 0.0f};
        earth[n++] = (struct TerrainVertex){front_x, bottom_y, za, ua, 2.8f};
        earth[n++] = (struct TerrainVertex){front_x, yb, zb, ub, 0.0f};
        earth[n++] = (struct TerrainVertex){front_x, bottom_y, zb, ub, 2.8f};

        /* Grass surface across the playable depth lane. */
        grass[i * 6 + 0] = (struct TerrainVertex){front_x, ya, za, 0.0f, ua};
        grass[i * 6 + 1] = (struct TerrainVertex){back_x,  ya, za, 4.0f, ua};
        grass[i * 6 + 2] = (struct TerrainVertex){back_x,  yb, zb, 4.0f, ub};
        grass[i * 6 + 3] = (struct TerrainVertex){front_x, ya, za, 0.0f, ua};
        grass[i * 6 + 4] = (struct TerrainVertex){back_x,  yb, zb, 4.0f, ub};
        grass[i * 6 + 5] = (struct TerrainVertex){front_x, yb, zb, 0.0f, ub};
    }

    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBindTexture(GL_TEXTURE_2D, id_checker);
    glVertexPointer(3, GL_FLOAT, sizeof(struct TerrainVertex), &earth[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct TerrainVertex), &earth[0].u);
    glDrawArrays(GL_TRIANGLES, 0, n);

    glBindTexture(GL_TEXTURE_2D, id_grass);
    glVertexPointer(3, GL_FLOAT, sizeof(struct TerrainVertex), &grass[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct TerrainVertex), &grass[0].u);
    glDrawArrays(GL_TRIANGLES, 0, segments * 6);

    /* Scenery is placed behind the route and scrolls with Act 1. */
    draw_act1_green_hill_background(center_z);
    draw_act1_floating_islands(center_z);
    draw_act1_textured_clouds(center_z);
    draw_act1_grass_and_flowers(center_z);
}

/* Distant layered mountain silhouettes for the GHZ Act 2 horizon. */
static void draw_ghz_mountain_backdrop(void)
{
    GLfloat far_ridge[] = {
        -25.0f,-1.45f,-29.0f,  -19.0f,4.8f,-29.0f,  -13.0f,-1.45f,-29.0f,
        -17.0f,-1.45f,-29.0f,   -9.0f,5.8f,-29.0f,   -2.0f,-1.45f,-29.0f,
         -5.0f,-1.45f,-29.0f,    2.0f,4.6f,-29.0f,    9.0f,-1.45f,-29.0f,
          5.0f,-1.45f,-29.0f,   14.0f,6.0f,-29.0f,   21.0f,-1.45f,-29.0f,
         17.0f,-1.45f,-29.0f,   23.0f,4.5f,-29.0f,   28.0f,-1.45f,-29.0f
    };

    GLfloat near_ridge[] = {
        -24.0f,-1.40f,-25.0f,  -17.0f,2.8f,-25.0f,  -10.0f,-1.40f,-25.0f,
        -13.0f,-1.40f,-25.0f,   -5.0f,3.7f,-25.0f,    3.0f,-1.40f,-25.0f,
         -1.0f,-1.40f,-25.0f,    7.0f,3.0f,-25.0f,   14.0f,-1.40f,-25.0f,
         10.0f,-1.40f,-25.0f,   18.0f,3.5f,-25.0f,   25.0f,-1.40f,-25.0f
    };

    glDisable(GL_TEXTURE_2D);

    glColor4f(0.10f,0.34f,0.24f,1.0f);
    glVertexPointer(3,GL_FLOAT,0,far_ridge);
    glDrawArrays(GL_TRIANGLES,0,15);

    glColor4f(0.13f,0.46f,0.25f,1.0f);
    glVertexPointer(3,GL_FLOAT,0,near_ridge);
    glDrawArrays(GL_TRIANGLES,0,12);
}

static void draw_terrain(void)
{
    /* Segmented grass route follows the Green Hill rolling profile. */
    struct TerrainVertex route[44 * 6];
    const GLfloat x0 = -3.12f;
    const GLfloat x1 =  3.12f;
    const GLfloat z0 = -14.0f;
    const GLfloat step = 0.5f;
    int i, n = 0;

    for (i = 0; i < 44; ++i) {
        GLfloat za = z0 + (GLfloat)i * step;
        GLfloat zb = za + step;
        GLfloat ya = ghz_ground_height_v2(za);
        GLfloat yb = ghz_ground_height_v2(zb);

        route[n++] = (struct TerrainVertex){x0,ya,za,0.0f,(GLfloat)i*0.23f};
        route[n++] = (struct TerrainVertex){x1,ya,za,4.0f,(GLfloat)i*0.23f};
        route[n++] = (struct TerrainVertex){x1,yb,zb,4.0f,(GLfloat)(i+1)*0.23f};
        route[n++] = (struct TerrainVertex){x0,ya,za,0.0f,(GLfloat)i*0.23f};
        route[n++] = (struct TerrainVertex){x1,yb,zb,4.0f,(GLfloat)(i+1)*0.23f};
        route[n++] = (struct TerrainVertex){x0,yb,zb,0.0f,(GLfloat)(i+1)*0.23f};
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,id_grass);
    glColor4f(1,1,1,1);
    glVertexPointer(3,GL_FLOAT,sizeof(struct TerrainVertex),&route[0].x);
    glTexCoordPointer(2,GL_FLOAT,sizeof(struct TerrainVertex),&route[0].u);
    glDrawArrays(GL_TRIANGLES,0,n);

    glDisable(GL_TEXTURE_2D);
    draw_valley_side(-1.0f);
    draw_valley_side( 1.0f);
    draw_ghz_mountain_backdrop();
    draw_wall(-3.15f,-3.40f);
    draw_wall( 3.15f, 3.40f);
    draw_ghz_cliffs_v2();
    draw_ghz_hills_v2();
    draw_ghz_palm_trees();
    draw_ghz_pillars();
    draw_clouds();

    glColor4f(1,1,1,1);
    glEnable(GL_TEXTURE_2D);
}


/* Compact 5x7 bitmap font for title-screen text. */
static const unsigned char title_font[27][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
 {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
 {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,14},{17,17,17,31,17,17,17},
 {14,4,4,4,4,4,14},{7,2,2,2,18,18,12},
 {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
 {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
 {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
 {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
 {17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {0,0,0,0,0,0,0}
};

static void draw_title_rect(float x,float y,float w,float h,
                            float r,float g,float b)
{
    GLfloat v[18]={x,y,0,x+w,y,0,x+w,y+h,0,
                   x,y,0,x+w,y+h,0,x,y+h,0};
    glColor4f(r,g,b,1);
    glVertexPointer(3,GL_FLOAT,0,v);
    glDrawArrays(GL_TRIANGLES,0,6);
}

static void draw_title_text(const char *str,float x,float y,
                            float scale,float r,float g,float b)
{
    static GLfloat verts[12000*3];
    unsigned int count=0;
    int i,row,col,index;

    for(i=0;str[i] && count+210<=12000;i++) {
        char c=str[i];
        if(c>='a' && c<='z') c=(char)(c-'a'+'A');
        index=(c>='A' && c<='Z') ? c-'A' : 26;

        for(row=0;row<7;row++) {
            for(col=0;col<5;col++) {
                float px,py;
                if(!(title_font[index][row]&(1<<(4-col)))) continue;
                px=x+(float)i*6.0f*scale+(float)col*scale;
                py=y+(6.0f-(float)row)*scale;

                verts[count*3]=px;
                verts[count*3+1]=py;
                verts[count*3+2]=0; count++;
                verts[count*3]=px+scale;
                verts[count*3+1]=py;
                verts[count*3+2]=0; count++;
                verts[count*3]=px+scale;
                verts[count*3+1]=py+scale;
                verts[count*3+2]=0; count++;
                verts[count*3]=px;
                verts[count*3+1]=py;
                verts[count*3+2]=0; count++;
                verts[count*3]=px+scale;
                verts[count*3+1]=py+scale;
                verts[count*3+2]=0; count++;
                verts[count*3]=px;
                verts[count*3+1]=py+scale;
                verts[count*3+2]=0; count++;
            }
        }
    }
    glColor4f(r,g,b,1);
    glVertexPointer(3,GL_FLOAT,0,verts);
    glDrawArrays(GL_TRIANGLES,0,(GLsizei)count);
}

static void draw_title_image(GLuint texture,
                             float x, float y, float w, float h,
                             float uv_width, float uv_height,
                             int use_alpha)
{
    GLfloat vertices[] = {
        x,     y,     0.0f,
        x + w, y,     0.0f,
        x,     y + h, 0.0f,
        x + w, y + h, 0.0f
    };

    GLfloat uvs[] = {
        0.0f,      uv_height,
        uv_width,  uv_height,
        0.0f,      0.0f,
        uv_width,  0.0f
    };

    if (use_alpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, uvs);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (use_alpha)
        glDisable(GL_BLEND);
}

static void draw_title_overlay(int card)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, 480, 0, 272, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (!card) {
        /* Main Sonic Anniversary artwork and supplied prompt images. */
        draw_title_rect(0, 0, 480, 272, 1.0f, 1.0f, 1.0f);

        draw_title_image(id_title_screen,
                         20.0f, 44.0f, 440.0f, 215.6f,
                         1.0f, 1.0f, 0);

        draw_title_image(id_press_start,
                         125.0f, 10.0f, 190.0f, 66.2f,
                         PRESS_START_UV_WIDTH,
                         PRESS_START_UV_HEIGHT, 1);

        draw_title_image(id_start_button,
                         365.0f, 15.0f, 100.0f, 40.7f,
                         START_BUTTON_UV_WIDTH,
                         START_BUTTON_UV_HEIGHT, 1);
    } else {
        /* Actual supplied Green Hill Zone Act 1 title card. */
        draw_title_rect(0, 0, 480, 272, 1.0f, 1.0f, 1.0f);

        draw_title_image(id_gh_title_card,
                         10.0f, 7.0f, 460.0f, 258.75f,
                         1.0f, GH_TITLE_IMAGE_UV_HEIGHT, 0);
    }

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glColor4f(1, 1, 1, 1);
}
static void draw_sega_splash(void)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0,480,0,272,-1,1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    draw_title_rect(0,0,480,272,0.0f,0.0f,0.0f);
    draw_title_text("SEGA",132,101,9.0f,1.0f,1.0f,1.0f);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glColor4f(1,1,1,1);
}


int main(void)
{
    EGLDisplay dpy;
    EGLConfig config;
    EGLint num_configs;
    EGLContext ctx;
    EGLSurface surface;
    SceCtrlData pad;
    float angle = 0.0f;
    int facing_direction = 1;
    float player_x = 0.0f;
    float player_z = 0.0f;
    float jump_y = 0.0f;
    float jump_velocity = 0.0f;
    float run_phase = 0.0f;
    float velocity_x = 0.0f;
    float velocity_z = 0.0f;
    float input_x;
    float input_z;
    float target_x;
    float target_z;
    float current_speed;
    float speed_ratio;
    float bob = 0.0f;
    float lean = 0.0f;
    float shoe_swing = 0.0f;
    float arm_swing = 0.0f;
    float arm_back = 0.0f;
    float jump_spin = 0.0f;
    float max_speed;
    float acceleration;
    int moving;
    int boosting;
    int drifting;
    int spindash_input;
    int drift_direction;
    int running;
    int airborne;
    unsigned int previous_buttons = 0;
    int game_state = -1; /* SEGA splash, title, card, gameplay */
    unsigned int title_card_start = 0;
    unsigned int sega_splash_start = 0;
        int thok_used = 0;
        int spindash_charge = 0;
        int spindash_charging = 0;
        int act_mode = 1; /* 1 = side-scrolling Act 1, 2 = 3D Act 2 */

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    EGLCHK(dpy = eglGetDisplay(0));
    EGLCHK(eglInitialize(dpy, NULL, NULL));
    EGLCHK(eglChooseConfig(dpy, attrib_list, &config, 1, &num_configs));

    if (num_configs == 0)
        return 1;

    EGLCHK(ctx = eglCreateContext(dpy, config, NULL, NULL));
    EGLCHK(surface = eglCreateWindowSurface(dpy, config, 0, NULL));
    EGLCHK(eglMakeCurrent(dpy, surface, surface, ctx));

    GLCHK(glDisable(GL_STENCIL_TEST));
    GLCHK(glDisable(GL_ALPHA_TEST));
    GLCHK(glDisable(GL_SCISSOR_TEST));
    GLCHK(glDisable(GL_CULL_FACE));
    GLCHK(glDisable(GL_LIGHTING));

    GLCHK(glEnable(GL_DEPTH_TEST));
    GLCHK(glDepthFunc(GL_LEQUAL));
    GLCHK(glEnable(GL_TEXTURE_2D));
    GLCHK(glShadeModel(GL_SMOOTH));
    GLCHK(glViewport(0, 0, 480, 272));

    GLCHK(glEnableClientState(GL_VERTEX_ARRAY));
    GLCHK(glEnableClientState(GL_TEXTURE_COORD_ARRAY));

    GLCHK(glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE));
    GLCHK(glColor4f(1.0f, 1.0f, 1.0f, 1.0f));


    if (!classic_rig_init()) {
        fprintf(stderr, "Classic Sonic rig initialization failed.\\n");
        return 1;
    }
    build_grass_texture();
    upload_grass_hd_texture(&id_grass);
    build_ghz_checker_texture();
    upload_ghz_checker_texture(&id_checker);
    upload_title_screen_texture();
    upload_rgba_title_texture(&id_start_button,
                              START_BUTTON_TEX_WIDTH,
                              START_BUTTON_TEX_HEIGHT,
                              start_button_pixels);
    upload_rgba_title_texture(&id_press_start,
                              PRESS_START_TEX_WIDTH,
                              PRESS_START_TEX_HEIGHT,
                              press_start_pixels);
    upload_gh_title_card_texture();
    build_act1_cloud_texture();
    upload_act1_cloud_texture();
    sonic_audio_init();
    sega_splash_start = sceKernelGetSystemTimeLow();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_SELECT) {
            sonic_audio_shutdown();
            break;
        }


        /* Black SEGA startup screen. */
        if (game_state == -1) {
            GLCHK(glClearColor(0.0f,0.0f,0.0f,1.0f));
            GLCHK(glClearDepthf(1.0f));
            GLCHK(glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT));
            draw_sega_splash();
            previous_buttons=pad.Buttons;
            EGLCHK(eglSwapBuffers(dpy,surface));

            if ((unsigned int)(sceKernelGetSystemTimeLow() - sega_splash_start) >= 2000000u) {
                game_state = 0;
                sonic_audio_play_music(SONIC_AUDIO_TITLE);
            }
            continue;
        }

        /* White Sonic Anniversary title screen. */
        if (game_state == 0) {
            GLCHK(glClearColor(1.0f,1.0f,1.0f,1.0f));
            GLCHK(glClearDepthf(1.0f));
            GLCHK(glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT));
            draw_title_overlay(0);

            if ((pad.Buttons & PSP_CTRL_START) &&
                !(previous_buttons & PSP_CTRL_START)) {
                game_state = 1;
                sonic_audio_play_sfx(SONIC_SFX_START);
                title_card_start=sceKernelGetSystemTimeLow();
            }

            previous_buttons=pad.Buttons;
            EGLCHK(eglSwapBuffers(dpy,surface));
            continue;
        }

        /* Five-second GREEN HILL ZONE card. */
        if (game_state == 1) {
            GLCHK(glClearColor(0.015f,0.07f,0.24f,1.0f));
            GLCHK(glClearDepthf(1.0f));
            GLCHK(glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT));
            draw_title_overlay(1);
            previous_buttons=pad.Buttons;
            EGLCHK(eglSwapBuffers(dpy,surface));

            if ((unsigned int)(sceKernelGetSystemTimeLow() - title_card_start) >= 5000000u) {
                game_state = 2;
                sonic_audio_play_music(SONIC_AUDIO_GREEN_HILL);
            }
            continue;
        }

        /* Analog movement: Act 1 is side-scrolling; Act 2 keeps 3D axes. */
        {
            float stick_x = (128.0f - (float)pad.Lx) / 128.0f;
            float stick_z = (128.0f - (float)pad.Ly) / 128.0f;

            input_x = 0.0f;
            input_z = 0.0f;

            if (act_mode == 1) {
                /* Act 1 is a true side-scrolling lane: horizontal only. */
                if (fabsf(stick_x) > 0.15f)
                    input_z = -stick_x;
                else {
                    if (pad.Buttons & PSP_CTRL_LEFT)  input_z -= 1.0f;
                    if (pad.Buttons & PSP_CTRL_RIGHT) input_z += 1.0f;
                }
                input_x = 0.0f;
                if (jump_y <= 0.001f &&
                    (((pad.Buttons & (PSP_CTRL_DOWN | PSP_CTRL_CROSS)) ==
                      (PSP_CTRL_DOWN | PSP_CTRL_CROSS)) ||
                     (pad.Buttons & PSP_CTRL_RTRIGGER)))
                    input_z = 0.0f;
            } else if (fabsf(stick_x) > 0.15f || fabsf(stick_z) > 0.15f) {
                input_x = stick_x;
                input_z = stick_z;
            } else {
                if (pad.Buttons & PSP_CTRL_LEFT)  input_x += 1.0f;
                if (pad.Buttons & PSP_CTRL_RIGHT) input_x -= 1.0f;
                if (pad.Buttons & PSP_CTRL_UP)    input_z += 1.0f;
                if (pad.Buttons & PSP_CTRL_DOWN)  input_z -= 1.0f;
            }

            float length = sqrtf(input_x * input_x + input_z * input_z);
            if (length > 1.0f) {
                input_x /= length;
                input_z /= length;
            }
        }
        if (act_mode == 1 && fabsf(input_z) > 0.01f)
            facing_direction = input_z < 0.0f ? -1 : 1;
        boosting = (act_mode != 1 &&
                    (pad.Buttons & PSP_CTRL_SQUARE) != 0);
        if (act_mode == 1) {
            input_x = 0.0f;
            velocity_x = 0.0f;
            player_x = 0.0f;
        }
        drifting = (act_mode != 1) &&
                   (((pad.Buttons & PSP_CTRL_LTRIGGER) != 0) ||
                    ((pad.Buttons & PSP_CTRL_RTRIGGER) != 0));
        drift_direction = 0;
        if (pad.Buttons & PSP_CTRL_LTRIGGER)
            drift_direction = 1;
        else if (pad.Buttons & PSP_CTRL_RTRIGGER)
            drift_direction = -1;
        max_speed = boosting ? 0.55f :
                    (act_mode == 1 ? 0.11f : 0.23f);
        acceleration = boosting ? 0.018f :
                       (act_mode == 1 ? 0.0014f : 0.0052f);

        target_x = input_x * max_speed;
        target_z = input_z * max_speed;

        /* Boost always launches forward, even from a standstill. */
        if (boosting && input_x == 0.0f && input_z == 0.0f) {
            float facing_rad = angle * 0.0174532925f;
            target_x = sinf(facing_rad) * max_speed;
            target_z = cosf(facing_rad) * max_speed;
        }

        if (input_x == 0.0f && input_z == 0.0f && !boosting) {
            float braking = (act_mode == 1) ? 0.0055f : 0.0011f;
            velocity_x = approach_value(velocity_x, 0.0f, braking);
            velocity_z = approach_value(velocity_z, 0.0f, braking);
        } else {
            float speed_now = sqrtf(
                velocity_x * velocity_x + velocity_z * velocity_z
            );
            float ratio = speed_now / max_speed;
            if (ratio > 1.0f) ratio = 1.0f;

            /* Keep steering responsive instead of heavily resisting turns. */
            float steering = 1.0f - ratio * 0.22f;

            velocity_x = approach_value(
                velocity_x, target_x, acceleration * steering
            );
            velocity_z = approach_value(
                velocity_z, target_z, acceleration * steering
            );

            /* Preserve momentum while steering through a turn. */
            if (act_mode != 1 && speed_now > 0.01f &&
                velocity_x * target_x + velocity_z * target_z > 0.0f) {
                float new_speed = sqrtf(
                    velocity_x * velocity_x + velocity_z * velocity_z
                );
                if (new_speed > 0.001f && new_speed < speed_now) {
                    float scale = speed_now / new_speed;
                    velocity_x *= scale;
                    velocity_z *= scale;
                }
            }

            /* Drift adds sideways momentum without cancelling forward speed. */
            if (drifting && jump_y <= 0.001f) {
                velocity_x += drift_direction *
                    (boosting ? 0.006f : 0.003f);
            }
        }

                /* Ground slope follows the active act's terrain profile. */
        if (jump_y <= 0.001f) {
            float slope;
            if (act_mode == 1) {
                slope = (
                    act1_ground_height(player_z + 0.2f) -
                    act1_ground_height(player_z - 0.2f)
                ) / 0.4f;
            } else {
                slope = (
                    ghz_ground_height_v2(player_z + 0.2f) -
                    ghz_ground_height_v2(player_z - 0.2f)
                ) / 0.4f;
            }
            float slope_accel = -slope * 0.0035f;
            if (slope_accel >  0.0010f) slope_accel =  0.0010f;
            if (slope_accel < -0.0010f) slope_accel = -0.0010f;
            velocity_z += slope_accel;
        }

        player_x += velocity_x;
        player_z += velocity_z;

        current_speed = sqrtf(
            velocity_x * velocity_x + velocity_z * velocity_z
        );
        moving = current_speed > 0.001f;

        /* Face the direction of travel. */
        if (act_mode != 1 && current_speed > 0.004f) {
            angle = atan2f(velocity_x, velocity_z) * 57.2957795f;
        }

        /* Act 1 has a narrow depth lane; Act 2 retains its original bounds. */
        if (act_mode == 1) {
            if (player_x < -0.55f) {
                player_x = -0.55f;
                if (velocity_x < 0.0f) velocity_x = 0.0f;
            }
            if (player_x > 0.55f) {
                player_x = 0.55f;
                if (velocity_x > 0.0f) velocity_x = 0.0f;
            }
        } else {
            if (player_x < -2.55f) {
                player_x = -2.55f;
                if (velocity_x < 0.0f) velocity_x = 0.0f;
            }
            if (player_x > 2.55f) {
                player_x = 2.55f;
                if (velocity_x > 0.0f) velocity_x = 0.0f;
            }
            if (player_z < -12.5f) {
                player_z = -12.5f;
                if (velocity_z < 0.0f) velocity_z = 0.0f;
            }
            if (player_z > 6.5f) {
                player_z = 6.5f;
                if (velocity_z > 0.0f) velocity_z = 0.0f;
            }
        }

        /* Recalculate movement after boundary collision. */
        current_speed = sqrtf(
            velocity_x * velocity_x + velocity_z * velocity_z
        );
        moving = current_speed > 0.001f;

        /* Act 1 Spin Dash: DOWN + CROSS or R, while grounded. */
        spindash_input =
            ((pad.Buttons & (PSP_CTRL_DOWN | PSP_CTRL_CROSS)) ==
             (PSP_CTRL_DOWN | PSP_CTRL_CROSS)) ||
            ((pad.Buttons & PSP_CTRL_RTRIGGER) != 0);
        if (act_mode == 1 && jump_y <= 0.001f && spindash_input) {
            if (spindash_charge < 18)
                spindash_charge++;
            spindash_charging = 1;
            velocity_x = 0.0f;
            velocity_z = 0.0f;
        } else if (act_mode == 1 && spindash_charging) {
            if (!spindash_input) {
                velocity_z = (float)facing_direction *
                             (0.12f + (float)spindash_charge * 0.0035f);
                if (velocity_z > 0.19f) velocity_z = 0.19f;
                if (velocity_z < -0.19f) velocity_z = -0.19f;
            }
            spindash_charge = 0;
            spindash_charging = 0;
        }

        /* Jump, then allow one mid-air Thok per jump. */
        if (!(act_mode == 1 &&
              ((pad.Buttons & PSP_CTRL_DOWN) ||
               spindash_charging)) &&
            (pad.Buttons & PSP_CTRL_CROSS) &&
            !(previous_buttons & PSP_CTRL_CROSS)) {
            if (jump_y <= 0.001f) {
                sonic_audio_play_sfx(SONIC_SFX_JUMP);
                jump_velocity = 0.16f;
                thok_used = 0;
            } else if (!thok_used && act_mode != 1) {
                float dir_x = input_x;
                float dir_z = input_z;
                float dir_len;

                if (dir_x == 0.0f && dir_z == 0.0f) {
                    dir_x = velocity_x;
                    dir_z = velocity_z;
                }

                dir_len = sqrtf(dir_x * dir_x + dir_z * dir_z);

                if (dir_len > 0.001f) {
                    dir_x /= dir_len;
                    dir_z /= dir_len;
                } else {
                    dir_x = 0.0f;
                    dir_z = 1.0f;
                }

                velocity_x = dir_x * 0.29f;
                velocity_z = dir_z * 0.29f;
                thok_used = 1;
            }
        }

        jump_y += jump_velocity;
        jump_velocity -= 0.0065f;
        if (jump_y <= 0.0f) {
            jump_y = 0.0f;
            jump_velocity = 0.0f;
            thok_used = 0;
        }
        /* Select idle, walk, run, boost, and curled jump animations. */
        speed_ratio = current_speed / max_speed;
        if (speed_ratio > 1.0f)
            speed_ratio = 1.0f;

        running = moving && !boosting && speed_ratio > 0.62f;
        airborne = jump_y > 0.001f;

        if (moving) {
            float phase_rate;

            if (boosting)
                phase_rate = 0.48f;
            else if (running)
                phase_rate = 0.34f;
            else
                phase_rate = 0.20f;

            run_phase += phase_rate * speed_ratio;
            bob = fabsf(sinf(run_phase)) * 0.035f * speed_ratio;
            lean = drifting ? -12.0f * drift_direction : running ? 3.0f : 0.0f;

            if (boosting) {
                /* Streamlined arms and a compact stride. */
                arm_back = -25.0f;
                arm_swing = sinf(run_phase) * 5.0f;
                shoe_swing = sinf(run_phase) * 12.0f;
            } else if (running) {
                arm_back = 8.0f;
                arm_swing = sinf(run_phase) * 22.0f;
                shoe_swing = sinf(run_phase) * 34.0f;
            } else {
                arm_back = 90.0f;
                arm_swing = sinf(run_phase) * 12.0f;
                shoe_swing = sinf(run_phase) * 18.0f;
            }
        } else {
            bob = 0.0f;
            lean = 0.0f;
            arm_back = 90.0f;
            arm_swing = 0.0f;
            shoe_swing = 0.0f;
        }

        /*
         * Curl the limbs during the jump.
         * The character spins around its local forward axis rather than
         * tumbling end-over-end.
         */
        if (airborne) {
            jump_spin += 24.0f;
            if (jump_spin >= 360.0f)
                jump_spin -= 360.0f;

            bob = 0.0f;
            lean = 0.0f;
            arm_back = -42.0f;
            arm_swing = 0.0f;
            shoe_swing = 0.0f;
        } else {
            jump_spin = 0.0f;
        }

        previous_buttons = pad.Buttons;

        GLCHK(glClearColor(0.25f, 0.65f, 0.95f, 1.0f));
        GLCHK(glClearDepthf(1.0f));
        GLCHK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        GLCHK(glMatrixMode(GL_PROJECTION));
        GLCHK(glLoadIdentity());
        GLCHK(gluPerspectivef(55.0f, 16.0f/9.0f, 0.1f, 100.0f));

        GLCHK(glMatrixMode(GL_MODELVIEW));
        GLCHK(glLoadIdentity());
        {
            if (act_mode == 1) {
                /* Fixed side-on camera for Classic Sonic Act 1. */
                static float camera_y = 0.0f;
                static float camera_z = 0.0f;
                static int camera_initialized = 0;

                float ground = act1_ground_height(player_z);
                float wanted_y = ground + 1.65f;

                if (!camera_initialized) {
                    camera_y = wanted_y;
                    camera_z = player_z;
                    camera_initialized = 1;
                }

                camera_y += (wanted_y - camera_y) * 0.10f;
                camera_z = player_z;

                GLCHK(gluLookAtf(-4.5f, camera_y, camera_z,
                                 0.0f, ground + 0.65f, player_z,
                                 0.0f, 1.0f, 0.0f));

                draw_act1_terrain(player_z);
            } else {
                /* Original third-person chase camera for Act 2. */
                static float camera_x = 0.0f;
                static float camera_y = 1.35f;
                static float camera_z = -10.5f;
                static int camera_initialized = 0;

                float ground = ghz_ground_height_v2(player_z);
                float wanted_x = player_x;
                float wanted_y = ground + 4.0f;
                float wanted_z = player_z - 10.5f;
                float look_x = player_x;
                float look_y = ground + 0.95f + jump_y * 0.35f;
                float look_z = player_z + 3.0f;

                if (!camera_initialized) {
                    camera_x = wanted_x;
                    camera_y = wanted_y;
                    camera_z = wanted_z;
                    camera_initialized = 1;
                }

                camera_x += (wanted_x - camera_x) * 0.12f;
                camera_y += (wanted_y - camera_y) * 0.10f;
                camera_z += (wanted_z - camera_z) * 0.12f;

                GLCHK(gluLookAtf(camera_x, camera_y, camera_z,
                                 look_x, look_y, look_z,
                                 0.0f, 1.0f, 0.0f));

                draw_terrain();
            }
        }

        if (act_mode == 1) {
            int rig_animation;
            float rig_time;

            if (spindash_charging) {
                rig_animation = CLASSIC_RIG_JUMP;
            } else if (airborne) {
                rig_animation = (jump_velocity > 0.0f)
                    ? CLASSIC_RIG_JUMP : CLASSIC_RIG_FALL;
            } else if (moving) {
                /* CLASSIC_RIG_RUN is the walk clip;
                   CLASSIC_RIG_BOOST is the actual run clip. */
                rig_animation = (current_speed >= 0.055f)
                    ? CLASSIC_RIG_BOOST : CLASSIC_RIG_RUN;
            } else {
                rig_animation = CLASSIC_RIG_IDLE;
            }

            rig_time = (float)sceKernelGetSystemTimeLow() * 0.000001f;
            if (rig_animation == CLASSIC_RIG_RUN)
                rig_time *= 0.35f;
            else if (rig_animation == CLASSIC_RIG_BOOST)
                rig_time *= 0.75f;

            GLCHK(glTranslatef(player_x,
                               act1_ground_height(player_z) + jump_y,
                               player_z));
            GLCHK(glRotatef(facing_direction < 0 ? 180.0f : 0.0f,
                            0.0f, 1.0f, 0.0f));
            GLCHK(glScalef(0.55f, 0.55f, 0.55f));
            classic_rig_draw(rig_animation, rig_time);
        } else {
            /* Original Adventure Sonic Act 2 rendering remains unchanged. */
            GLCHK(glTranslatef(player_x,
                               ghz_ground_height_v2(player_z)
                               + 1.02f + jump_y + bob,
                               player_z));
            GLCHK(glRotatef(lean, 0.0f, 0.0f, 1.0f));
            GLCHK(glRotatef(angle, 0.0f, 1.0f, 0.0f));
            if (airborne)
                GLCHK(glRotatef(jump_spin, 0.0f, 0.0f, 1.0f));
            GLCHK(glScalef(1.8f, 1.8f, 1.8f));
            GLCHK(glTranslatef(0.0f, -0.56f, -0.105f));
            /* Modern Sonic renderer will be connected here. */
        }
        EGLCHK(eglSwapBuffers(dpy, surface));
    }

    EGLCHK(eglTerminate(dpy));
    sceKernelExitGame();
    return 0;
}
