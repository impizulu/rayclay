/*
    platformer_levels.h - the three courses, as ASCII art.

    A course is a list of bars. A bar is PL_BAR_CELLS cells across (four beats of
    four cells) and PL_ROWS rows tall, written top row first, so a bar reads the
    way it plays. One jump lasts one beat at PL_SPEED_NORMAL: obstacles placed on
    the beat make the player press on the beat.

        .  empty        #  block, stand on it, die on its side
        ^  spike        v  spike hanging from above
        o  jump orb     p  jump pad
        $  coin         on the arc of a jump, so a press on the beat takes them all

    Included once, by the implementation half of platformer_backend.h.
*/
#ifndef PLATFORMER_LEVELS_H
#define PLATFORMER_LEVELS_H

static const PlBar BAR_RUN = {{
    "................",
    "................",
    "................",
    "................",
    "................",
    "................" }};

/* The first obstacle stands past the scripted bench run's 128 ticks. */
static const PlBar BAR_LEAD = {{
    "................",
    "................",
    "................",
    "............$...",
    "...........$.$..",
    "......$.$...^..." }};

static const PlBar BAR_SPIKE = {{
    "................",
    "................",
    "................",
    "............$...",
    "...........$.$..",
    "....^.......^..." }};

static const PlBar BAR_PULSE = {{
    "................",
    "................",
    "................",
    "....$...$...$...",
    "................",
    "....^...^...^..." }};

static const PlBar BAR_DOUBLE = {{
    "................",
    "................",
    "................",
    "............$$..",
    "................",
    "....^^......^^.." }};

static const PlBar BAR_TRIPLE = {{
    "................",
    "................",
    "................",
    ".....$..........",
    "....$.$.........",
    "....^^^........." }};

static const PlBar BAR_SLAB = {{
    "................",
    "................",
    "................",
    "................",
    ".......$$$......",
    "......#####....." }};

static const PlBar BAR_HOP = {{
    "................",
    "................",
    "........$.......",
    ".......$.$......",
    "................",
    "...####^^^####.." }};

static const PlBar BAR_STEPS_UP = {{
    "................",
    "................",
    "................",
    "............$.##",
    "........$.######",
    "......##########" }};

static const PlBar BAR_STEPS_TOP = {{
    "................",
    "................",
    "..$$............",
    "######.......$$.",
    "######..........",
    "######.......^^." }};

static const PlBar BAR_TOWERS = {{
    "................",
    "................",
    "..........$.....",
    ".......$......$.",
    "......##......##",
    "......##......##" }};

static const PlBar BAR_TOWERS_OUT = {{
    "................",
    "................",
    "................",
    ".........$......",
    "##..............",
    "##.......^......" }};

/* Hanging spikes over a low run: a held jump dies here, so let go. */
static const PlBar BAR_TUNNEL = {{
    "................",
    "................",
    "................",
    "...##########...",
    "...vvvvvvvvvv...",
    ".....$..$..$...." }};

static const PlBar BAR_PAD = {{
    "................",
    ".....$$$........",
    "................",
    "................",
    "................",
    "...p^^^........." }};

static const PlBar BAR_PAD_WALL = {{
    "................",
    "................",
    "..........$$$...",
    ".......#########",
    ".......#########",
    "...p...#########" }};

static const PlBar BAR_PAD_DROP = {{
    "................",
    "................",
    "..$$............",
    "#####......$$...",
    "#####...........",
    "#####......^^..." }};

static const PlBar BAR_ORB = {{
    "................",
    ".........$......",
    "................",
    ".......o........",
    "................",
    "......^^^^......" }};

static const PlBar BAR_ORB_PAIR = {{
    "................",
    "......$...$.....",
    "................",
    "....o...o.......",
    "................",
    "...^^^^^^^^^...." }};

/* The last bar stands empty so nobody dies on the doorstep of the gate. */
static const PlBar *const COURSE_MIDNIGHT[] = {
    &BAR_RUN, &BAR_LEAD, &BAR_SPIKE, &BAR_PULSE, &BAR_SPIKE, &BAR_DOUBLE, &BAR_SLAB, &BAR_SPIKE,
    &BAR_HOP, &BAR_DOUBLE, &BAR_PAD, &BAR_PULSE, &BAR_SLAB, &BAR_DOUBLE, &BAR_STEPS_UP,
    &BAR_STEPS_TOP, &BAR_SPIKE, &BAR_TUNNEL, &BAR_PULSE, &BAR_TUNNEL, &BAR_DOUBLE, &BAR_HOP,
    &BAR_PAD, &BAR_TRIPLE, &BAR_TOWERS, &BAR_TOWERS_OUT, &BAR_PULSE, &BAR_RUN, &BAR_RUN,
};

static const PlBar *const COURSE_SUNSET[] = {
    &BAR_RUN, &BAR_LEAD, &BAR_PULSE, &BAR_DOUBLE, &BAR_ORB, &BAR_SPIKE, &BAR_PAD, &BAR_TRIPLE,
    &BAR_HOP, &BAR_ORB_PAIR, &BAR_DOUBLE, &BAR_ORB, &BAR_PULSE, &BAR_TOWERS, &BAR_TOWERS_OUT,
    &BAR_TRIPLE, &BAR_PAD_WALL, &BAR_PAD_DROP, &BAR_ORB, &BAR_TUNNEL, &BAR_HOP, &BAR_PULSE,
    &BAR_STEPS_UP, &BAR_STEPS_TOP, &BAR_ORB_PAIR, &BAR_DOUBLE, &BAR_PAD, &BAR_TRIPLE,
    &BAR_TUNNEL, &BAR_PULSE, &BAR_RUN, &BAR_RUN,
};

static const PlBar *const COURSE_VOLTAGE[] = {
    &BAR_RUN, &BAR_LEAD, &BAR_DOUBLE, &BAR_TRIPLE, &BAR_ORB, &BAR_PULSE, &BAR_HOP, &BAR_ORB_PAIR,
    &BAR_TRIPLE, &BAR_TOWERS, &BAR_TOWERS_OUT, &BAR_ORB, &BAR_PAD_WALL, &BAR_PAD_DROP,
    &BAR_DOUBLE, &BAR_TUNNEL, &BAR_TRIPLE, &BAR_ORB_PAIR, &BAR_HOP, &BAR_PAD, &BAR_TRIPLE,
    &BAR_STEPS_UP, &BAR_STEPS_TOP, &BAR_ORB, &BAR_ORB_PAIR, &BAR_DOUBLE, &BAR_TUNNEL,
    &BAR_PULSE, &BAR_TOWERS, &BAR_TOWERS_OUT, &BAR_TRIPLE, &BAR_ORB_PAIR, &BAR_HOP, &BAR_TRIPLE,
    &BAR_PULSE, &BAR_RUN, &BAR_RUN,
};

#define PL_COURSE(list) list, (int)(sizeof list / sizeof *list)

static const PlLevelInfo PL_LEVELS[PL_LEVEL_COUNT] = {
    { "Midnight Drive", "Steady shards and friendly pads. Press on the beat.", "EASY", 1,
      PL_SPEED_NORMAL, PL_COURSE(COURSE_MIDNIGHT),
      { { 0x120a2e, 0x5a1d6e, 0x9a4ab0, 0x3b1858, 0x26103e, 0x0b0518, 0x8ff7ea, 0xffb347 },
        { 0x08142a, 0x1c5170, 0x3a8fa8, 0x153b55, 0x0f2638, 0x040b14, 0xffd9a8, 0xff6fae } } },
    { "Sunset Circuit", "Orbs over the shards. Press again in midair.", "NORMAL", 2,
      PL_SPEED_NORMAL, PL_COURSE(COURSE_SUNSET),
      { { 0x2a0640, 0xc21e6e, 0xe84a7e, 0xa61a6a, 0x5c0f58, 0x1f0530, 0xffe2a8, 0xffb52e },
        { 0x3a0a18, 0xe0521c, 0xff7a3c, 0xc23c20, 0x74200f, 0x2a0810, 0xffe9b8, 0xffd23e } } },
    { "Voltage", "A faster tempo and no time to think.", "HARD", 3,
      PL_SPEED_FAST, PL_COURSE(COURSE_VOLTAGE),
      { { 0x04140f, 0x0b6e4c, 0x12a06a, 0x0c7d58, 0x07402f, 0x03110c, 0xdcffb0, 0xb4ff2e },
        { 0x0a0a2a, 0x12507c, 0x1c80a8, 0x136a90, 0x0a3a52, 0x06091f, 0xb8fff0, 0x3cffd2 } } },
};

#endif /* PLATFORMER_LEVELS_H */
