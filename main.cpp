#include <cstdio>
#include <miosix.h>
#include "mxgui/display.h"
#include "mxgui/entry.h"
#include "mxgui/misc_inst.h"
#include "mxgui/level2/input.h"

using namespace miosix;
using namespace mxgui;

int main() {
    Display &display = DisplayManager::instance().getDisplay();
    Point start = Point(0, 0);
    Point end = Point(127, 159);

    {
        DrawingContext dc(display);
        dc.clear(black);
        dc.setPixel(start, green);
        dc.setPixel(end, red);

        //dc.write(Point(3, 23), "X");
        //dc.write(Point(118, 43), "T");
        dc.clippedWrite(start, start, Point(15, 15), "F");

    }

    for (;;);
}
