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

// Enable MXGUI_ORIENTATION_VERTICAL in mxgui/config/mxgui_settings.h line 82
int main(){

    // VERTICAL orientation test
    Point start = Point(0, 0);
    Point end   = Point(127,159);
    Point mid   = Point(64, 80);
    Display& display = DisplayManager::instance().getDisplay();

    {
        DrawingContext dc(display);

        dc.clear(black);
        dc.drawRectangle(start, end, grey);
        dc.write(Point(8,8), "HELLO, WORLD !");
        dc.clear(mid, end, green);
        dc.clear(Point(96, 120), end, red);
        dc.clear(Point(112, 140), end, blue);

        dc.setTextColor(black, white);
        dc.write(Point(4, 120), "cool :)");
        dc.drawRectangle(start, Point(1,1), red);

        delayMs(3000);
    }

    for (;;){
        DrawingContext dc(display);
        dc.drawImage(start, img1_v);
        delayMs(3000);
        dc.drawImage(start, img2_v);
        delayMs(3000);
    }

}
