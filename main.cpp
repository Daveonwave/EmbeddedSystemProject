#include <cstdio>
#include <miosix.h>
#include "mxgui/display.h"
#include "mxgui/entry.h"
#include "mxgui/misc_inst.h"
#include "mxgui/level2/input.h"

using namespace miosix;
using namespace mxgui;

/*
 * //TODO: this example run with HORIZONTAL screen
#include <entry.h>
#include <display.h>
#include <level2/simple_plot.h>
#include <unistd.h>
#include <cmath>
using namespace std;
ENTRY(){
    int i=0;
    vector<float> data1;
    vector<float> data2;

    vector<Dataset> dataset;
    dataset.push_back(Dataset(data1,red));
    dataset.push_back(Dataset(data2,green));

    Display& display=DisplayManager::instance().getDisplay();
    SimplePlot plot1(Point(0,0), Point(159,127));

    for(;;i+=2)
    {
        {
            DrawingContext dc(display);
            plot1.draw(dc,dataset);
        }
        data1.push_back(20*(1+0.05*i)*sin(0.1*i));
        data2.push_back(20*(1+0.05*i));
        usleep(100000);
    }
}
*/



int main(){
    Display& display = DisplayManager::instance().getDisplay();
    Point start = Point(0, 0);
    Point end   = Point(127,159);
    Point mid   = Point(64, 80);

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
    }

    for (;;);

}


