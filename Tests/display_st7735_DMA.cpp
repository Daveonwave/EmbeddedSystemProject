#include "display_st7735.h"
#include <kernel/scheduler/scheduler.h>

using namespace std;
using namespace miosix;

#ifdef _BOARD_STM32F4DISCOVERY

/**
 * DMA TX end of transfer
 */
void __attribute__((naked)) DMA2_Stream3_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z20SPI2txDmaHandlerImplv");  //TODO: controllare branch to label assembly
    restoreContext();
}

static Thread *waiting = 0;     //Eventual thread waiting for the DMA to complete
static bool dmaTransferInProgress = false; //DMA transfer is in progress, requires waiting
static bool dmaTransferActivated = false;  //DMA transfer has been activated, and needs cleanup

/**
 * DMA TX end of transfer actual implementation
 */
void __attribute__((used)) SPI2txDmaHandlerImpl()
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
    //waitDmaCompletion();
    writeReg(0x29);
    delayMs(150);
}

void DisplayImpl::doTurnOff() {
    waitDmaCompletion();
    writeReg(0x28);     //ST7735_DISPOFF TODO: should be followed by SLPIN
    delayMs(150);
}

//No function to set brightness
void DisplayImpl::doSetBrightness(int brt) {}

pair<short int, short int> DisplayImpl::doGetSize() const {
    return make_pair(height, width);
}

void DisplayImpl::write(Point p, const char *text) {
    waitDmaCompletion();
    font.draw(*this, textColor, p, text);
}

void DisplayImpl::clippedWrite(Point p, Point a,  Point b, const char *text) {
    waitDmaCompletion();
    font.clippedDraw(*this, textColor, p, a, b, text);
}

void DisplayImpl::clear(Color color) {
    clear(Point(0,0), Point(width-1, height-1), color);
}

//TODO: TO IMPLEMENT
void DisplayImpl::clear(Point p1, Point p2, Color color) {
    waitDmaCompletion();
    imageWindow(p1, p2);
    int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);
    pixel = color;
    startDmaTransfer(&pixel, numPixels, false);
}

void DisplayImpl::beginPixel() {
    waitDmaCompletion();
    //TODO: find out implementation
    imageWindow(Point(0,0), Point(width-1, height-1));
}

void DisplayImpl::setPixel(Point p, Color color) {
    setCursor(p);
    SPITransaction t;
    writeRamBegin();
    writeRam(color);
    writeRamEnd();
}

void DisplayImpl::line(Point a, Point b, Color color) {
    waitDmaCompletion();
    //Horizontal line speed optimization
    if(a.y() == b.y())
    {
        imageWindow(Point(min(a.x(), b.x()), a.y()),
                    Point(max(a.x(), b.x()), a.y()));
        int numPixels = abs(a.x() - b.x());
        pixel = color;
        startDmaTransfer(&pixel, numPixels + 1, false);
        return;
    }
    //Vertical line speed optimization
    if(a.x() == b.x())
    {
        textWindow(Point(a.x(), min(a.y(), b.y())),
                    Point(a.x(), max(a.y(), b.y())));
        int numPixels = abs(a.y() - b.y());
        pixel = color;
        startDmaTransfer(&pixel, numPixels + 1, false);
        return;
    }
    Line::draw(*this, a, b, color);
}

void DisplayImpl::scanLine(Point p, const Color *colors, unsigned short length) {
    waitDmaCompletion();
    imageWindow(p, Point(width - 1, p.y()));
    startDmaTransfer(colors, length, true);
    waitDmaCompletion();
}

Color *DisplayImpl::getScanLineBuffer() {
    return buffers[which];
}

void DisplayImpl::scanLineBuffer(Point p, unsigned short length) {
    waitDmaCompletion();
    imageWindow(p, Point(width - 1, p.y()));
    startDmaTransfer(buffers[which], length, true);
    which = (which == 0 ? 1 : 0);
}

void DisplayImpl::drawImage(Point p, const ImageBase& img) {
    short int xEnd = p.x() + img.getWidth() - 1;
    short int yEnd = p.y() + img.getHeight() - 1;
    if(xEnd >= width || yEnd >= height) return;
    
    waitDmaCompletion();

    const unsigned short *imgData = img.getData();
    if(imgData!=0)
    {
        //Optimized version for memory-loaded images
        imageWindow(p, Point(xEnd,yEnd));
        int numPixels = img.getHeight() * img.getWidth();
        startDmaTransfer(imgData, numPixels, true);
        //TODO: check this REINTERPRED CAST
        //If the image is in RAM don't overlap I/O, as the caller could
        //deallocate it. If it is in FLASH it's guaranteed to be const
        if(reinterpret_cast<unsigned int>(imgData)>=0x20000000)
            waitDmaCompletion();
    } else img.draw(*this, p);
}

void DisplayImpl::clippedDrawImage(Point p, Point a, Point b, const ImageBase& img) {
    waitDmaCompletion();
    img.clippedDraw(*this, p, a, b);
}

void DisplayImpl::drawRectangle(Point a, Point b, Color c) {
    line(a,Point(b.x(), a.y()), c);
    line(Point(b.x(), a.y()), b, c);
    line(b,Point(a.x(), b.y()), c);
    line(Point(a.x(), b.y()), a, c);
}

void DisplayImpl::update() { waitDmaCompletion(); }

DisplayImpl::pixel_iterator DisplayImpl::begin(Point p1,
    Point p2, IteratorDirection d) 
{
    if(p1.x()<0 || p1.y()<0 || p2.x()<0 || p2.y()<0) return pixel_iterator();
    if(p1.x() >= width || p1.y() >= height || p2.x() >= width || p2.y() >= height)
        return pixel_iterator();
    if(p2.x() < p1.x() || p2.y() < p1.y()) return pixel_iterator();
    
    waitDmaCompletion();

    if(d == DR) textWindow(p1, p2);
    else imageWindow(p1, p2);
    
    SPITransaction t;
    writeRamBegin();
    
    unsigned int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);
    return pixel_iterator(numPixels);
}

DisplayImpl::~DisplayImpl() {}

DisplayImpl::DisplayImpl(): which(0) {
    {
        FastInterruptDisableLock dLock;

        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
        SPI2->CR2=0;
        SPI2->CR1=SPI_CR1_SSM  //Software cs
                | SPI_CR1_SSI  //Hardware cs internally tied high
                | SPI_CR1_MSTR //Master mode
                | SPI_CR1_SPE; //SPI enabled

        scl::mode(Mode::ALTERNATE);     scl::alternateFunction(5);
        sda::mode(Mode::ALTERNATE);     sda::alternateFunction(5);
        csx::mode(Mode::OUTPUT);
        dcx::mode(Mode::OUTPUT);
        resx::mode(Mode::OUTPUT);
    }

    csx::high();
    dcx::high();

    resx::high();
    delayMs(150);
    resx::low();
    delayMs(150);
    resx::high();
    delayMs(150);
    
    writeReg(0x01);    // ST7735_SWRESET
    delayMs(150);
    writeReg(0x11);    // ST7735_SLPOUT
    delayMs(150);

    //Send initializiation commands
    const unsigned char *cmds=initCmds;
    while(*cmds)
    {
        unsigned char cmd=*cmds++;
        unsigned char numArgs=*cmds++;
        writeReg(cmd,cmds,numArgs);
        cmds+=numArgs;
    }

    waitDmaCompletion();
    
    doTurnOn();
    setFont(droid11);
    setTextColor(make_pair(white, black));
}

//TODO: implement with CASET and RASET
void DisplayImpl::window(Point p1, Point p2)
{
    //Setting column bounds, ST7735_CASET
    unsigned char buff_caset[4];
    buff_caset[0] = p1.x()>>8;      buff_caset[1] = p1.x() & 255;
    buff_caset[2] = p2.x()>>8;      buff_caset[3] = p2.x() & 255;
    writeReg(0x2A, buff_caset, sizeof(buff_caset));
    
    //Setting row bounds, ST7735_RASET
    unsigned char buff_raset[4];
    buff_raset[0] = p1.y()>>8;      buff_raset[1] = p1.y() & 255;
    buff_raset[2] = p2.y()>>8;      buff_raset[3] = p2.y() & 255;
    writeReg(0x2B, buff_raset, sizeof(buff_raset));
}

/**
 * Write only commands with one parameter
 */
void DisplayImpl::writeReg(unsigned char reg, unsigned char data)
{
    SPITransaction t;
    {
        CommandTransaction c;
        writeRam(reg);
    }
    writeRam(data);
}

/**
 * Write commands with more parameters
 */
void DisplayImpl::writeReg(unsigned char reg, const unsigned char *data, int len)
{
    SPITransaction t;
    {
        CommandTransaction c;
        writeRam(reg);
    }
    if(data) for(int i = 0; i < len; i++) writeRam(*data++);
}

/**
 * Start data transfer with DMA. We take this from sony-newman implementation.
 */
void DisplayImpl::startDmaTransfer(const unsigned short *data, int length,
        bool increm)
{
    csx::low();
    {
        CommandTransaction c;
        writeRam(0x2C);     // ST7735_RAMWR, no restriction on length of parameters
    }
    //Wait until the SPI is busy, required otherwise the last byte is not fully sent
    while((SPI2->SR & SPI_SR_TXE) == 0) ;
    while(SPI2->SR & SPI_SR_BSY) ;

    SPI2->CR1 = 0;
    SPI2->CR2 = SPI_CR2_TXDMAEN;
    SPI2->CR1 = SPI_CR1_SSM
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
    DMA2_Stream3->PAR = reinterpret_cast<unsigned int>(&SPI2->DR);
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

/**
 * Wait the completion of data transfer with DMA. We take this from Sony-Newman implementation.
 */
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
    while((SPI2->SR & SPI_SR_TXE) == 0) ;
    while(SPI2->SR & SPI_SR_BSY) ;

    csx::high();
    
    SPI2->CR1=0;
    SPI2->CR2=0;
    SPI2->CR1=SPI_CR1_SSM
            | SPI_CR1_SSI
            | SPI_CR1_MSTR
            | SPI_CR1_SPE;

    //Quirk: reset RXNE by reading DR, or a byte remains in the input buffer
    volatile short temp=SPI2->DR;
    (void)temp;
}

const unsigned char initCmds[] = {
        0x3A, 0X01, 0x05,                   // ST7735_COLMOD, color mode: 16-bit/pixel
        0xB1, 0x03, 0x00, 0x06, 0x03,       // ST7735_FRMCTR1, normal mode frame rate
        0xB6, 0x02, 0x15, 0x02,             // ST7735_DISSET5, display settings
        0xB4, 0x01, 0x00,                   // ST7735_INVCTR, line inversion active
        0xC0, 0x02, 0x02, 0x70,             // ST7735_PWCTR1, default (4.7V, 1.0 uA)
        0xC1, 0x01, 0x05,                   // ST7735_PWCTR2, default (VGH=14.7V, VGL=-7.35V)
        0xC3, 0x02, 0x02, 0x07,             // ST7735_PWCTR4, bclk/2, opamp current small and medium low
        0xC5, 0x02, 0x3C, 0x38,             // ST7735_VMCTR1, VCOMH=4V VCOML=-1.1
        0xFC, 0x02, 0x11, 0x15,             // ST7735_PWCTR6, power control (partial mode+idle)
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
        0x13,                               // ST7735_NORON, normal display mode on
        0x00                                // ST7735_NOP
};

} //namespace mxgui

#endif //_BOARD_STM32F4DISCOVERY
