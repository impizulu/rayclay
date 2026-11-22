/*
    mascot.h - the logo that watches you sign in

    The RayClay mascot at the top of the form reads the address while you type it,
    shuts its eyes for your password, and peeks with one eye if you press Show.
    Nothing here is an image swap: the logo is drawn as usual, and rcIconEmit draws
    a pair of live eyes on top of it, in the logo's own coordinates, from the
    numbers in a Mascot. The app says where it should look, and mascot_ease moves
    there over a few frames, asking for those frames only while it is moving.
*/
#ifndef APP_MASCOT_H
#define APP_MASCOT_H

#include "rayclay.h"
#include "icons/rc_icons_rayclay_logo.h"

/* The logo's square box, and the height it needs: the art is wider than tall. */
enum { MASCOT_BOX = 112, MASCOT_ART_H = 68 };

typedef struct {
    float gazeX, gazeY;       /* the pupils, -1 to 1 from the middle of the eye */
    float lidLeft, lidRight;  /* 0 is open, 1 is shut                           */
} Mascot;

/* As the logo itself is drawn: the sideways glance. */
#define MASCOT_REST (RC_LIT(Mascot){ 0.7f, 0.42f, 0.0f, 0.0f })

/* The logo's numbers, from rc_icons_rayclay_logo.h's viewBox. */
#define MASCOT_VIEWBOX 731.429f
#define MASCOT_EYE_LEFT 293.714f
#define MASCOT_EYE_RIGHT 443.714f
#define MASCOT_EYE_Y 434.714f
#define MASCOT_EYE_RX 38.0f
#define MASCOT_EYE_RY 44.0f
#define MASCOT_PUPIL_R 17.0f
#define MASCOT_GLINT_R 6.0f

static inline void mascot_eye(RC_BoundingBox b, float cx, float gazeX, float gazeY, float lid) {
    const float y = MASCOT_EYE_Y, vb = MASCOT_VIEWBOX;
    float       open = 1.0f - lid;
    RC_Color    head = rcHex(0x5F6BB0), ink = rcHex(0x23264A);

    /* The face first, over the eye the logo drew, so a shut eye is shut. */
    rcIconDrawFilledEllipse(b, cx, y, MASCOT_EYE_RX + 2.0f, MASCOT_EYE_RY + 2.0f, vb, head);
    if (open > 0.05f)
        rcIconDrawFilledEllipse(b, cx, y, MASCOT_EYE_RX, MASCOT_EYE_RY * open, vb, RC_WHITE);
    if (open > 0.5f) {
        float px = cx + gazeX * (MASCOT_EYE_RX - MASCOT_PUPIL_R);
        float py = y + gazeY * (MASCOT_EYE_RY * open - MASCOT_PUPIL_R);

        rcIconDrawFilledCircle(b, px, py, MASCOT_PUPIL_R, vb, ink);
        rcIconDrawFilledCircle(b, px - 6.0f, py - 8.0f, MASCOT_GLINT_R, vb, RC_WHITE);
    }
    if (lid > 0.8f) {
        const RC_IconPoint arc[5] = {
            { cx - 34.0f, y }, { cx - 18.0f, y + 13.0f }, { cx, y + 18.0f },
            { cx + 18.0f, y + 13.0f }, { cx + 34.0f, y }
        };

        rcIconDrawPolyline(b, arc, 5, vb, 11.0f, false, ink);
    }
}

static inline void mascot_eyes_glyph(RC_BoundingBox b, RC_Color c, const void *u) {
    const Mascot *m = (const Mascot *)u;

    (void)c;
    mascot_eye(b, MASCOT_EYE_LEFT, m->gazeX, m->gazeY, m->lidLeft);
    mascot_eye(b, MASCOT_EYE_RIGHT, m->gazeX, m->gazeY, m->lidRight);
}

/* The logo, then the eyes in a float exactly over it. The float passes the pointer
   through, so the mascot never gets in the way of anything. */
static inline void mascot_draw(const Mascot *m) {
    rcBox(.id = "mascot", .align = "cc", .wType = RC_PX(MASCOT_BOX), .hType = RC_PX(MASCOT_ART_H)) {
        rcIconRayClayLogo((float)MASCOT_BOX);
        rcBox(.floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER,
                            .element = RC_ANCHOR_CENTER, .capture = RC_CAPTURE_PASSTHROUGH }) {
            rcIconEmit((float)MASCOT_BOX, RC_WHITE, mascot_eyes_glyph, m);
        }
    }
}

static inline bool mascot_approach(float *value, float target, float step) {
    float d = target - *value;

    if (d > -0.005f && d < 0.005f) {
        *value = target;
        return false;
    }
    *value += d * step;
    return true;
}

/* A step of the way to the pose, by the frame's time so a slow frame moves as far.
   True while it is still on the way: the caller then asks for another frame. */
static inline bool mascot_ease(Mascot *m, Mascot pose, float dt) {
    float step = dt * 14.0f;
    bool  moving = false;

    if (step > 1.0f)
        step = 1.0f;
    moving |= mascot_approach(&m->gazeX, pose.gazeX, step);
    moving |= mascot_approach(&m->gazeY, pose.gazeY, step);
    moving |= mascot_approach(&m->lidLeft, pose.lidLeft, step);
    moving |= mascot_approach(&m->lidRight, pose.lidRight, step);
    return moving;
}

/* From a distance on the screen to a gaze: it turns quickly near the logo and
   flattens out far away, as eyes do, and stays inside -1 to 1. */
static inline float mascot_gaze(float distance) {
    return distance / ((distance < 0.0f ? -distance : distance) + 120.0f);
}

#endif /* APP_MASCOT_H */
