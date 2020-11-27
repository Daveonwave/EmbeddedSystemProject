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
    //using led = Gpio<GPIOB_BASE, 3>;	
    //led::mode(Mode::OUTPUT);
	

    Display& display = DisplayManager::instance().getDisplay();
    
    {
        DrawingContext dc(display);
        dc.clear(0xFF00);
        dc.write(Point(36,5), "Ciao ciao");
    }
    for(;;) ;
    

	//dc.write(Point(0,0), "Ciao");
	
    /*
	for(;;){
        ledOn();
        Thread::sleep(500);
        ledOff();
        Thread::sleep(500);
    }
    */
    
    
}
