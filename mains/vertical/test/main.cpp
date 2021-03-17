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

    static const Color rainbow[]={
            63488,63520,63584,63616,63744,63776,63840,
            63872,63936,64000,64032,64128,64192,64256,
            64288,64352,64384,64448,64544,64608,64640,
            64704,64768,64800,64864,64960,65024,65056,
            65120,65152,65216,65280,65376,65408,65472,
            65504,63456,63456,61408,57312,55264,55264,
            53216,51168,49120,49120,45024,42976,40928,
            38880,38880,36832,34784,30688,30688,28640,
            26592,24544,24544,22496,18400,16352,16352,
            14304,12256,10208,8160,6112,4064,2016,
            2016,2017,2017,2018,2020,2021,2021,
            2022,2023,2024,2025,2026,2027,2028,
            2029,2029,2030,2031,2033,2033,2034,
            2035,2036,2037,2037,2039,2040,2041,
            2041,2042,2043,2044,2045,2046,2047,
            2047,1983,1919,1887,1791,1727,1663,
            1631,1567,1535,1471,1375,1311,1279,
            1215,1151,1119,1055,959,895,863,
            799,767,703,639,607,479,447,
            383,351,287,255,191,95,31,
            31,2079,4127,6175,6175,10271,12319,
            14367,14367,16415,18463,20511,22559,24607,
            26655,28703,30751,30751,34847,36895,38943,
            38943,40991,43039,45087,47135,49183,51231,
            53279,55327,55327,57375,61471,63519,63519,
            63519,63518,63517,63516,63515,63514,63513,
            63512,63512,63511,63510,63508,63508,63507,
            63506,63505,63504,63504,63502,63501,63500,
            63500,63499,63498,63497,63496,63495,63494,
            63493,63492,63492,63491,63489,63488,63488
    };

    // VERTICAL orientation test
    Point start = Point(0, 0);
    Point end   = Point(127,159);
    Point mid   = Point(64, 80);
    Display& display = DisplayManager::instance().getDisplay();

    for (;;){
        DrawingContext dc(display);

        dc.clear(black);

        dc.write(Point(8,8), "HELLO, WORLD !");
        dc.clear(mid, end, green);
        dc.clear(Point(96, 120), end, red);
        dc.clear(Point(112, 140), end, blue);

        dc.setTextColor(black, white);
        dc.write(Point(4, 120), "cool :)");
        dc.drawRectangle(start, Point(1,1), red);

        for(int k=mid.y()-40;k<mid.y()-10;k++)
            dc.scanLine(Point(0,k),rainbow,dc.getWidth());
        dc.drawRectangle(start, end, grey);

        delayMs(5000);

        dc.drawImage(start, img1_v);
        delayMs(5000);
        dc.drawImage(start, img2_v);
        delayMs(5000);
    }

}
