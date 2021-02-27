#include <cstdio>
#include <miosix.h>
#include "mxgui/display.h"
#include "mxgui/entry.h"
#include "mxgui/misc_inst.h"
#include "mxgui/level2/input.h"
#include <level2/simple_plot.h>
#include <unistd.h>
#include <cmath>

using namespace miosix;
using namespace mxgui;
using namespace std;


// Enable MXGUI_ORIENTATION_HORIZONTAL in mxgui/config/mxgui_settings.h line 83
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



