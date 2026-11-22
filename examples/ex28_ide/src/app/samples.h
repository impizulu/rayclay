/*
    samples.h - the small project that opens where there is no folder to read

    A phone bundle or a web page has no source tree beside it, and a program started from
    somewhere else finds no examples/ folder. The workspace then holds these four files in
    memory, so the editor opens on real work everywhere. Saving one hands its bytes to the
    user with rcFileDialogSave.
*/
#ifndef APP_SAMPLES_H
#define APP_SAMPLES_H

static const char SAMPLE_README[] =
    "# Harbor Tide\n"
    "\n"
    "A small tide table for the **Portland** harbor. It reads `stations.json` and prints\n"
    "the next high and low water for each station.\n"
    "\n"
    "## Build\n"
    "\n"
    "```\n"
    "cc -std=c11 -O2 tide.c -o tide\n"
    "./tide stations.json\n"
    "```\n"
    "\n"
    "## Notes\n"
    "\n"
    "- Heights are in meters above chart datum.\n"
    "- The mark in `logo.svg` is the app icon; open it to see its live preview.\n"
    "- See the [harmonic constants](https://tidesandcurrents.noaa.gov) for the model.\n";

static const char SAMPLE_C[] =
    "/* tide.c - the next high and low water from a station's harmonic constants. */\n"
    "#include <math.h>\n"
    "#include <stdio.h>\n"
    "\n"
    "#define HOURS_PER_DAY 24\n"
    "#define PI 3.14159265358979\n"
    "\n"
    "typedef struct {\n"
    "    const char *name;\n"
    "    double      amplitude;   /* meters  */\n"
    "    double      speed;       /* deg/h   */\n"
    "    double      phase;       /* degrees */\n"
    "} Constituent;\n"
    "\n"
    "static const Constituent PORTLAND[] = {\n"
    "    { \"M2\", 1.350, 28.984, 104.0 },\n"
    "    { \"S2\", 0.210, 30.000, 142.0 },\n"
    "    { \"N2\", 0.280, 28.440,  78.0 },\n"
    "    { \"K1\", 0.140, 15.041, 201.0 },\n"
    "};\n"
    "\n"
    "/* The height at `hours` past the epoch: a sum of cosines. */\n"
    "static double height_at(double hours)\n"
    "{\n"
    "    double h = 0.0;\n"
    "\n"
    "    for (int i = 0; i < 4; i++) {\n"
    "        const Constituent *c = &PORTLAND[i];\n"
    "        h += c->amplitude * cos((c->speed * hours - c->phase) * PI / 180.0);\n"
    "    }\n"
    "    return h;\n"
    "}\n"
    "\n"
    "int main(void)\n"
    "{\n"
    "    for (int hour = 0; hour < HOURS_PER_DAY; hour += 3)\n"
    "        printf(\"%02d:00  %+.2f m\\n\", hour, height_at(hour));\n"
    "    return 0;\n"
    "}\n";

static const char SAMPLE_JSON[] =
    "{\n"
    "  \"harbor\": \"Portland\",\n"
    "  \"datum\": \"MLLW\",\n"
    "  \"stations\": [\n"
    "    { \"id\": 8418150, \"name\": \"Portland\", \"lat\": 43.657, \"lon\": -70.247 },\n"
    "    { \"id\": 8413320, \"name\": \"Bar Harbor\", \"lat\": 44.392, \"lon\": -68.205 }\n"
    "  ],\n"
    "  \"units\": \"meters\",\n"
    "  \"predict\": true\n"
    "}\n";

static const char SAMPLE_SVG[] =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\"\n"
    "     stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\""
    " stroke-linejoin=\"round\">\n"
    "  <!-- a wave over a tide line: edit a number and watch the preview -->\n"
    "  <path d=\"M2 12c2-2 4-2 6 0s4 2 6 0 4-2 6 0\"/>\n"
    "  <path d=\"M2 18c2-2 4-2 6 0s4 2 6 0 4-2 6 0\"/>\n"
    "  <circle cx=\"17\" cy=\"5\" r=\"2\"/>\n"
    "</svg>\n";

static const struct {
    const char *name;
    const char *bytes;
    int         size;
} SAMPLES[] = {
    { "README.md",     SAMPLE_README, (int)sizeof SAMPLE_README - 1 },
    { "logo.svg",      SAMPLE_SVG,    (int)sizeof SAMPLE_SVG - 1 },
    { "stations.json", SAMPLE_JSON,   (int)sizeof SAMPLE_JSON - 1 },
    { "tide.c",        SAMPLE_C,      (int)sizeof SAMPLE_C - 1 },
};

#endif /* APP_SAMPLES_H */
