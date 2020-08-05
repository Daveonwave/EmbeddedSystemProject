#include "display_st7735.h"
#include "miosix.h"
#include <kernel/scheduler/scheduler.h>
#include <cstdarg>

using namespace std;
using namespace miosix;

#ifdef _BOARD_STM32F4DISCOVERY

/**
 * DMA TX end of transfer
 */
void __attribute__((naked)) DMA2_Stream3_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z20SPI1txDmaHandlerImplv");  //TODO: controllare parametro stringa
    restoreContext();
}

static Thread *waiting = 0;     //Eventual thread waiting for the DMA to complete
static bool dmaTransferInProgress = false; //DMA transfer is in progress, requires waiting
static bool dmaTransferActivated = false;  //DMA transfer has been activated, and needs cleanup

/**
 * DMA TX end of transfer actual implementation
 */
void __attribute__((used)) SPI1txDmaHandlerImpl()
{
    DMA2->LIFCR = DMA_LIFCR_CTCIF3
                | DMA_LIFCR_CTEIF3
                | DMA_LIFCR_CDMEIF3;
    dmaTransferInProgress = false;
    if(waiting == 0) return;
    waiting->IRQwakeup();
    if(waiting->IRQgetPriority() > Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();
    waiting = 0;
}

namespace mxgui {

/**
 * Function to attach the display we are using.
 */
void registerDisplayHook(DisplayManager& dm) {
    dm.registerDisplay(&DisplayImpl::instance());
}

/**
 * Class DisplayImpl
 */
DisplayImpl& DisplayImpl::instance() {
    static DisplayImpl instance;
    return instance;
}

void DisplayImpl::doTurnOn() {
    //TODO: RCC configuration
    {
        FastInterruptDisableLock dLock;

        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
        SPI1->CR2=0;
        SPI1->CR1=SPI_CR1_SSM  //Software cs
                | SPI_CR1_SSI  //Hardware cs internally tied high
                | SPI_CR1_MSTR //Master mode
                | SPI_CR1_SPE; //SPI enabled
    }

    //TODO: GPIOs are really already enabled? (RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN)
    {
        FastInterruptDisableLock dLock;

        scl::mode(Mode::ALTERNATE);     scl::alternateFunction(5);
        sda::mode(Mode::ALTERNATE);     sda::alternateFunction(5);
        csx::mode(Mode::OUTPUT);
        dcx::mode(Mode::OUTPUT);
        resx::mode(Mode::OUTPUT);
    }

    // TODO: fatto a caso, copiato da sony-newman
    resx::low();
    Thread::sleep(10);
    resx::high();
    Thread::sleep(10);

    const unsigned char initCmds[] = {
        0x01, 0x00,                         // ST7735_SWRESET
        0x11, 0x00,                         // ST7735_SLPOUT
        0x3A, 0X01, 0x05,                   // ST7735_COLMOD, color mode: 16-bit/pixel
        0xB1, 0x03, 0x01, 0x2C, 0x2D,       // ST7735_FRMCTR1, normal mode frame rate
        0x36, 0x01, 0x08,                   // ST7735_MADCTL, row/col addr, bottom-top refresh
        0xB6, 0x02, 0x15, 0x02,             // ST7735_DISSET5, display settings
        0xB4, 0x01, 0x00,                   // ST7735_INVCTR, line inversion active
        0xC0, 0x02, 0x02, 0x70,             // ST7735_PWCTR1, default (4.7V, 1.0 uA)
        0xC1, 0x01, 0x05,                   // ST7735_PWCTR2, default (VGH=14.7V, VGL=-7.35V)
        0xC2, 0x02, 0x01, 0x02,             // ST7735_PWCTR3, opamp current small, boost frequency
        0xC5, 0x02, 0x3C, 0x38,             // ST7735_VMCTR1, VCOMH=4V VCOML=-1.1
        0xFC, 0x02, 0x11, 0x15,             // ST7735_PWCTR6, power control (partial mode+idle) TODO: get rid of it
        0xE0, 0x10,                         // ST7735_GMCTRP1, Gamma adjustments (pos. polarity)
            0x09, 0x16, 0x09, 0x20,         // (Not entirely necessary, but provides
            0x21, 0x1B, 0x13, 0x19,         // accurate colors)
            0x17, 0x15, 0x1E, 0x2B,
            0x04, 0x05, 0x02, 0x0E,
        0xE1, 0x10,                         // ST7735_GMCTRN1, Gamma adjustments (neg. polarity)
            0x0B, 0x14, 0x08, 0x1E,         // (Not entirely necessary, but provides
            0x22, 0x1D, 0x18, 0x1E,         // accurate colors)
            0x1B, 0x1A, 0x24, 0x2B,
            0x06, 0x06, 0x02, 0x0F,
        0x2A, 0x04,                         // ST7735_CASET, column address
            0x00, 0x00,                     // x_start = 0
            0x00, 0x7F,                     // x_end = 127
        0x2B, 0x04,                         // ST7735_RASET, row address
            0x00, 0x00,                     // x_start = 0
            0x00, 0x9F,                     // x_end = 159
        0x13, 0x00,                         // ST7735_NORON, normal display mode on
        0x29, 0x00,                         // ST7735_DISPON, display on
        0x00                                //END while
    };

    //Send initializiation commands
    const unsigned char *cmds=initCmds;
    while(*cmds)
    {
        unsigned char cmd=*cmds++;
        unsigned char numArgs=*cmds++;
        writeReg(cmd,cmds,numArgs);
        cmds+=numArgs;
    }

    clear(black);
    waitDmaCompletion();
    
    //Turn on display
    //writeReg(0x29,0x00);

}

void DisplayImpl::doTurnOff() {
    sendCmd(0x28, 0);   //ST7735_DISPOFF TODO: should be followed by SLPIN
    delayMs(150);
}

//No function to set brightness
void DisplayImpl::doSetBrightness(int brt) {}

pair<short int, short int> DisplayImpl::doGetSize() const {
    return make_pair(height, width);
}

void DisplayImpl::write(Point p, const char *text) {
    font.draw(*this, textColor, p, text);
}

void DisplayImpl::clippedWrite(Point p, Point a,  Point b, const char *text) {
    font.clippedDraw(*this, textColor, p, a, b, text);
}

void DisplayImpl::clear(Color color) {
    clear(Point(0,0), Point(width-1,  height-1), color);
}

//TODO: TO IMPLEMENT ---------------------------------------------------------------------
//----------------------------------------------------------------------------------------
void DisplayImpl::clear(Point p1, Point p2, Color color) {
    imageWindow(p1,p2);
    int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);
    pixel = color;
}

void DisplayImpl::beginPixel() {}

void DisplayImpl::setPixel(Point p, Color color) {}

void DisplayImpl::line(Point a, Point b, Color color) {}

void DisplayImpl::scanLine(Point p, const Color *colors, unsigned short length) {}

Color *DisplayImpl::getScanLineBuffer() {
    Color *color = NULL;
    return color;
}

void DisplayImpl::scanLineBuffer(Point p, unsigned short length) {}

void DisplayImpl::drawImage(Point p, const ImageBase& img) {}

void DisplayImpl::clippedDrawImage(Point p, Point a, Point b, const ImageBase& img) {}

void DisplayImpl::drawRectangle(Point a, Point b, Color c) {}

DisplayImpl::pixel_iterator DisplayImpl::begin(Point p1,
    Point p2, IteratorDirection d) {
        return pixel_iterator();
    }

//void DisplayImpl::setCursor(Point p) {}

void DisplayImpl::textWindow(Point p1, Point p2) {}

void DisplayImpl::imageWindow(Point p1, Point p2) {}

void DisplayImpl::update() {}

DisplayImpl::~DisplayImpl() {}
//---------------------------------------------------------------------------------------
//TODO: ---------------------------------------------------------------------------------

void DisplayImpl::writeReg(unsigned char reg, unsigned char data)
{
    /*SPITransaction t;
    {
        CommandTransaction c;
        writeRam(reg);
    }
    writeRam(data);*/
}

void DisplayImpl::writeReg(unsigned char reg, const unsigned char *data, int len)
{
    /*SPITransaction t;
    {
        CommandTransaction c;
        writeRam(reg);
    }
    if(data) for(int i=0;i<len;i++) writeRam(*data++);*/
}

DisplayImpl::DisplayImpl(): buffer(0) {

    doTurnOn();
    setFont(droid11);
    setTextColor(make_pair(Color(0xffff),Color(0x0000)));
}

void DisplayImpl::window(Point p1, Point p2)
{
    //Taken from underverk's SmartWatch_Toolchain/src/driver_display.c
    unsigned char buffer[8];
    unsigned char y0 = p1.y() + 0x1c;
    unsigned char y1 = p2.y() + 0x1c;
    buffer[0] = p1.x()>>4; buffer[1] = p1.x() & 15;
    buffer[2] = p2.x()>>4; buffer[3] = p2.x() & 15;
    buffer[4] = y0>>4;     buffer[5] = y0 & 15;
    buffer[6] = y1>>4;     buffer[7] = y1 & 15;
    // writeReg(0x0a, buffer, sizeof(buffer));
}

static inline void textWindow(Point p1, Point p2){
    sendCmd(0x36, 1, 0x08);     // ST7735_MADCTL, row/col addr, bottom-top refresh
    //window(p1, p2);
}

void DisplayImpl::startDmaTransfer(const unsigned short *data, int length,
        bool increm)
{
    csx::low();
    {
        CommandTransaction c;
        writeRam(0xc); //TODO: cambiare come in .h
    }
    //Wait until the SPI is busy, required otherwise the last byte is not fully sent
    while((SPI1->SR & SPI_SR_TXE) == 0) ;
    while(SPI1->SR & SPI_SR_BSY) ;

    SPI1->CR1 = 0;
    SPI1->CR2 = SPI_CR2_TXDMAEN;
    SPI1->CR1 = SPI_CR1_SSM
              | SPI_CR1_SSI
              | SPI_CR1_DFF
              | SPI_CR1_MSTR
              | SPI_CR1_SPE;

    dmaTransferActivated = true;
    dmaTransferInProgress = true;

    NVIC_ClearPendingIRQ(DMA2_Stream3_IRQn);//DMA2 stream 3 channel 3 = SPI1_TX
    NVIC_SetPriority(DMA2_Stream3_IRQn, 10);//Low priority for DMA
    NVIC_EnableIRQ(DMA2_Stream3_IRQn);

    DMA2_Stream3->CR = 0;
    DMA2_Stream3->PAR = reinterpret_cast<unsigned int>(&SPI1->DR);
    DMA2_Stream3->M0AR = reinterpret_cast<unsigned int>(data);
    DMA2_Stream3->NDTR = length;
    DMA2_Stream3->CR = DMA_SxCR_CHSEL_0 //Channel 3
                | DMA_SxCR_CHSEL_1
                | DMA_SxCR_PL_1    //High priority because fifo disabled
                | DMA_SxCR_MSIZE_0 //Read 16 bit from memory
                | DMA_SxCR_PSIZE_0 //Write 16 bit to peripheral
                | (increm ? DMA_SxCR_MINC : 0) //Increment memory pointer
                | DMA_SxCR_DIR_0   //Memory to peripheral
                | DMA_SxCR_TCIE    //Interrupt on transfer complete
                | DMA_SxCR_TEIE    //Interrupt on transfer error
                | DMA_SxCR_DMEIE   //Interrupt on direct mode error
                | DMA_SxCR_EN;     //Start DMA
}

void DisplayImpl::waitDmaCompletion()
{
    if(dmaTransferActivated==false) return; //Nothing to do

    {
        FastInterruptDisableLock dLock;
        if(dmaTransferInProgress)
        {
            waiting = Thread::IRQgetCurrentThread();
            do {
                waiting->IRQwait();
                {
                    FastInterruptEnableLock eLock(dLock);
                    Thread::yield();
                }
            } while(waiting != 0);
        }
    }

    dmaTransferActivated=false;
    
    NVIC_DisableIRQ(DMA2_Stream3_IRQn);

    //Wait for last byte to be sent
    while((SPI1->SR & SPI_SR_TXE)==0) ;
    while(SPI1->SR & SPI_SR_BSY) ;

    csx::high();
    
    SPI1->CR1=0;
    SPI1->CR2=0;
    SPI1->CR1=SPI_CR1_SSM
            | SPI_CR1_SSI
            | SPI_CR1_MSTR
            | SPI_CR1_SPE;

    //Quirk: reset RXNE by reading DR, or a byte remains in the input buffer
    volatile short temp=SPI1->DR;
    (void)temp;
}

} //namespace mxgui

#endif //_BOARD_STM32F4DISCOVERY
