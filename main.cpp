#include <cstdio>
#include <miosix.h>
#include "mxgui/display.h"
#include "mxgui/entry.h"
#include "mxgui/misc_inst.h"
#include "mxgui/level2/input.h"

using namespace std;
using namespace miosix;
using namespace mxgui;

int main()
{
    DrawingContext dc(DisplayManager::instance().getDisplay());
    dc.clear(red);
    for(;;)
    {
        ledOn();
        Thread::sleep(1500);
        ledOff();
        Thread::sleep(1500);
    }
}
