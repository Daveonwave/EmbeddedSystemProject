#include <cstdio>
#include <miosix.h>
#include "mxgui/display.h"
#include "mxgui/entry.h"
#include "mxgui/misc_inst.h"
#include "mxgui/level2/input.h"
#include "mxgui/resource_image.h"
#include "images/img1_v.h"
#include "images/img2_v.h"
#include "images/img1_h.h"
#include "images/img2_h.h"

using namespace miosix;
using namespace mxgui;
using namespace std;

// Enable MXGUI_ORIENTATION_HORIZONTAL in mxgui/config/mxgui_settings.h line 83
int main(){

    // HORIZONTAL orientation test
    Point start = Point(0, 0);
    Point end   = Point(159,127);
    Point mid   = Point(80, 64);
    Display& display = DisplayManager::instance().getDisplay();

    {
        DrawingContext dc(display);

        dc.clear(black);
        dc.drawRectangle(start, end, grey);
        dc.write(Point(8,8), "HELLO, WORLD !");
        dc.clear(mid, end, green);
        dc.clear(Point(120, 96), end, red);
        dc.clear(Point(140, 112), end, blue);

        dc.setTextColor(black, white);
        dc.write(Point(4, 100), "cool :)");
        dc.drawRectangle(start, Point(1,1), red);

        delayMs(3000);
    }

    for (;;){
        DrawingContext dc(display);
        dc.drawImage(start, img1_h);
        delayMs(3000);
        dc.drawImage(start, img2_h);
        delayMs(3000);
    }

}
