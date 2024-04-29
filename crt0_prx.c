#define _PSP_FW_VERSION 150
#include "headers/crt0_prx.h"
#include "headers/float.h"
#include "headers/module.h"
#include <pspchnnlsv.h>
#include <pspctrl.h>
#include <pspctrl_kernel.h>
#include <pspdisplay.h>
#include <pspdisplay_kernel.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspkerneltypes.h>
#include <pspmoduleinfo.h>
#include <pspmodulemgr.h>
#include <psppower.h>
#include <pspsdk.h>
#include <pspthreadman.h>
#include <pspthreadman_kernel.h>
#include <pspumd.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <psputility.h>
#include <psputility_savedata.h>
#include <pspwlan.h>
#include <stdlib.h>
#include <string.h>

extern SceUID sceKernelSearchModuleByName(unsigned char *);

//Defines
PSP_MODULE_INFO("cheatER", 0x3007, 1, 2);//0x3007
PSP_MAIN_THREAD_ATTR(0);                 //0 for kernel mode too

//Globals
unsigned char *gameDir = "ms0:/seplugins/cheatER/__________.txt";
unsigned char gameId[10];
unsigned char running = 0;
SceUID thid;
#define MAX_THREAD 64
static int thread_count_start, thread_count_now, pauseuid = -1;
static SceUID thread_buf_start[MAX_THREAD], thread_buf_now[MAX_THREAD];

//Structures
typedef struct Hook {
    ModuleFunc modfunc;
    char modname[32];
    char libname[32];
    u32 nid;
    void *func;
} Hook;

typedef struct Block {
    unsigned char flags;
    unsigned int address;
    unsigned int stdVal;
    unsigned int hakVal;
} Block;

typedef struct Cheat {
    unsigned short block;
    unsigned short len;
    unsigned char flags;
    unsigned char name[32];
} Cheat;

//Defines
//Block flags
#define FLAG_DMA (1 << 4)
#define FLAG_FREEZE (1 << 5)
#define FLAG_DWORD (3 << 6)
#define FLAG_UWORD (2 << 6)
#define FLAG_WORD (1 << 6)
#define FLAG_BYTE (0 << 6)

//Cheat flags
#define FLAG_SELECTED (1 << 0)//If selected, will be disabled/enabled by music button
#define FLAG_CONSTANT (1 << 1)//If 1, cheat is constantly on regardless of music button
#define FLAG_FRESH (1 << 2)   //Cheat was just recently enabled/disabled

#define BLOCK_MAX 2048

#define START_ADDRESS 0x48804000
#define END_ADDRESS 0x4A000000
#define SKIP_BYTES 0x4000

//Globals
SceCtrlData pad;
unsigned int blockTotal = 0;
unsigned int cheatTotal = 0;
Block block[BLOCK_MAX];
Cheat cheat[BLOCK_MAX];
unsigned char buffer[64];
unsigned char cheatStatus = 0;
unsigned char cheatSaved = 0;
unsigned int cheatSelected = 0;
unsigned int tabSelected = 0;
unsigned char menuDrawn = 0;
void *vram;
unsigned int menuKey = PSP_CTRL_VOLUP | PSP_CTRL_VOLDOWN;
unsigned int triggerKey = PSP_CTRL_NOTE;
unsigned int screenKey = PSP_CTRL_LTRIGGER | PSP_CTRL_SQUARE;
unsigned int cheatHz = 0;
unsigned char cheatFlash = 0;
unsigned char cheatPause = 0;
unsigned char cheatSearch = 0;
unsigned char extMenu = 0;
unsigned int extSelected[4] = {0, 0, 0, 0};
unsigned char extOpt = 0;
unsigned int extOptArg = 0;
unsigned int dumpNo = 0;
unsigned int cheatNo = 0;
unsigned int searchNo = 0;
unsigned int searchMax = 0;
unsigned int searchHistoryCounter = 0;
Block searchHistory[16];
unsigned int searchResultCounter = 0;
unsigned int searchAddress[100];
unsigned int browseAddress = START_ADDRESS;
unsigned int browseY = 0;
unsigned int browseC = 0;
unsigned int browseX = 0;
unsigned int browseLines = 16;
unsigned int decodeFormat = 0x48800000;
unsigned int trackFormat = 0x48800000;
unsigned int browseFormat = 0x48800000;
unsigned int decodeAddress = START_ADDRESS;
unsigned int decodeY = 0;
unsigned int decodeC = 0;
unsigned int decodeX = 0;
unsigned int trackAddress = START_ADDRESS;
unsigned int trackY = 0;
unsigned int trackX = 0;
unsigned int trackC = 0;
unsigned int trackBackup = 0;
unsigned char trackMode = 0;
unsigned char trackStatus = 0;
unsigned char trackPause = 0;
unsigned int cheatDMA = 0;
unsigned char cheatButtonAgeX = 0;
unsigned char cheatButtonAgeY = 0;
unsigned char searchMode = 0;
unsigned char copyMenu = 0;//0=Menu Off, 1=Menu On, Copy selected, 2=Menu On, Paste selected
unsigned int copyData = 0x08804000;
unsigned char screenTime = 0;

unsigned char fileBuffer[1536];
unsigned int fileBufferSize = 0;
unsigned int fileBufferBackup = 0;
unsigned int fileBufferFileOffset = 0;
unsigned int fileBufferOffset = 1024;
unsigned int screenNo = 0;

unsigned char screenPath[64] = {0};

char* getGamePathFromID(char* gameID);


#define fileBufferPeek(a_out, a_ahead)                  \
    if ((fileBufferOffset + a_ahead) >= 1024) {         \
        fileBufferBackup = sceIoLseek(fd, 0, SEEK_CUR); \
        sceIoLseek(fd, a_ahead, SEEK_CUR);              \
        sceIoRead(fd, &a_out, 1);                       \
        sceIoLseek(fd, fileBufferBackup, SEEK_SET);     \
    } else {                                            \
        a_out = fileBuffer[fileBufferOffset + a_ahead]; \
    }
#define fileBufferRead(a_out, a_size)                     \
    if (fileBufferOffset == 1024) {                       \
        fileBufferSize = sceIoRead(fd, fileBuffer, 1024); \
        fileBufferOffset = 0;                             \
    }                                                     \
    memcpy(a_out, &fileBuffer[fileBufferOffset], a_size); \
    fileBufferOffset += a_size;                           \
    fileBufferFileOffset += a_size;
#define lineClear(a_line)                                                                      \
    pspDebugScreenSetXY(0, a_line);                                                            \
    pspDebugScreenPuts("                                                                   "); \
    pspDebugScreenSetXY(0, a_line);

//Arrays
unsigned int decDelta[10] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};
unsigned char *searchModeName[] = {
        "  0=Same                                                        ",
        "  1=Different                                                   ",
        "  2=Greater                                                     ",
        "  3=Less                                                        ",
        "  4=Inc by    ",
        "  5=Dec by    ",
        "  6=Equal to  "};

//Functions
int module_start(SceSize args, void *argp) __attribute__((alias("_start")));

int module_stop(SceSize args, void *argp) __attribute__((alias("_stop")));

static void gamePause(SceUID thid);

static void gameResume(SceUID thid);

//Mac Address hooking module
unsigned char cfg[] = {'C', 'F', 'G', 0x88, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x80, 0x00, 0x00,
                       0x00, 0x30, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00};

unsigned int hookMac(unsigned char *a_mac) {
    memcpy(a_mac, cfg + 5, 6);
    return 0;
}

Hook hookA[1] =
        {
                {{0, NULL}, "sceWlan_Driver", "sceWlanDrv", 0x0c622081, hookMac},
};

//Cheat handler
unsigned int char2hex(unsigned char *a_data, unsigned int *a_type) {
    unsigned int dword = 0;
    unsigned int power = 0;

    while (power < 8) {
        switch (a_data[power]) {
            case '_':
                break;//Freeze cheat
            case '0':
                dword |= 0 * (1 << (4 * (7 - power)));
                break;
            case '1':
                dword |= 1 * (1 << (4 * (7 - power)));
                break;
            case '2':
                dword |= 2 * (1 << (4 * (7 - power)));
                break;
            case '3':
                dword |= 3 * (1 << (4 * (7 - power)));
                break;
            case '4':
                dword |= 4 * (1 << (4 * (7 - power)));
                break;
            case '5':
                dword |= 5 * (1 << (4 * (7 - power)));
                break;
            case '6':
                dword |= 6 * (1 << (4 * (7 - power)));
                break;
            case '7':
                dword |= 7 * (1 << (4 * (7 - power)));
                break;
            case '8':
                dword |= 8 * (1 << (4 * (7 - power)));
                break;
            case '9':
                dword |= 9 * (1 << (4 * (7 - power)));
                break;
            case 'a':
            case 'A':
                dword |= 0xA * (1 << (4 * (7 - power)));
                break;
            case 'b':
            case 'B':
                dword |= 0xB * (1 << (4 * (7 - power)));
                break;
            case 'c':
            case 'C':
                dword |= 0xC * (1 << (4 * (7 - power)));
                break;
            case 'd':
            case 'D':
                dword |= 0xD * (1 << (4 * (7 - power)));
                break;
            case 'e':
            case 'E':
                dword |= 0xE * (1 << (4 * (7 - power)));
                break;
            case 'f':
            case 'F':
                dword |= 0xF * (1 << (4 * (7 - power)));
                break;
            default:
                dword >>= 4 * (8 - power);
                *a_type = power;
                return dword;
        }

        power++;
    }

    *a_type = 8;
    return dword;
}

unsigned int cheatNew(unsigned char a_size, unsigned int a_address, unsigned int a_value) {
    if ((cheatTotal + 1 < BLOCK_MAX) && (blockTotal + 1 < BLOCK_MAX)) {
        cheat[cheatTotal].block = blockTotal;
        cheat[cheatTotal].flags = 0;
        cheat[cheatTotal].len = 1;

        block[blockTotal].address = a_address;
        block[blockTotal].address &= 0x0FFFFFFF;
        block[blockTotal].address += 0x08800000;

        block[blockTotal].flags = 0;
        block[blockTotal].hakVal = a_value;

        switch (a_size) {
            case 1:
                block[blockTotal].flags |= FLAG_BYTE;
                if (cheatSaved) block[blockTotal].stdVal = *((unsigned char *) (block[blockTotal].address));
                break;
            case 2:
                block[blockTotal].flags |= FLAG_WORD;
                if (cheatSaved) block[blockTotal].stdVal = *((unsigned short *) (block[blockTotal].address));
                break;
            case 4:
                block[blockTotal].flags |= FLAG_DWORD;
                if (cheatSaved) block[blockTotal].stdVal = *((unsigned int *) (block[blockTotal].address));
                break;
            default:
                block[blockTotal].flags |= FLAG_UWORD;
        }

        sprintf(&cheat[cheatTotal].name, "NEW CHEAT %d", cheatNo);
        cheatNo++;
        cheatTotal++;
        blockTotal++;
    }
}

unsigned int blockAdd(int fd, unsigned char *a_data) {
    unsigned int type;
    unsigned int offset;
    unsigned char hex[8];

    if (blockTotal != BLOCK_MAX) {
        block[blockTotal].flags = 0;

        sceIoLseek(fd, 1, SEEK_CUR);
        sceIoRead(fd, hex, 8);

        block[blockTotal].address = char2hex(hex, &type);

        if (block[blockTotal].address == 0xFFFFFFFF) {
            block[blockTotal].flags |= FLAG_DMA;
            block[blockTotal].stdVal = 0xFFFFFFFF;
        } else {
            block[blockTotal].address &= 0x0FFFFFFF;
            block[blockTotal].address += 0x08800000;
        }

        offset = sceIoLseek(fd, 3, SEEK_CUR);
        sceIoRead(fd, hex, 8);

        block[blockTotal].hakVal = char2hex(hex, &type);

        if (hex[0] == '_') {
            block[blockTotal].flags |= FLAG_FREEZE;
        }

        switch (type) {
            case 2:
                block[blockTotal].flags |= FLAG_BYTE;
                break;
            case 4:
                block[blockTotal].flags |= FLAG_WORD;
                break;
            case 8:
                block[blockTotal].flags |= FLAG_DWORD;
                break;
            default:
                block[blockTotal].flags |= FLAG_UWORD;
        }

        sceIoLseek(fd, offset + type, SEEK_SET);//Reposition the cursor depending on size of Hex value

        blockTotal++;

        return 1;
    }
    return 0;
}

void cheatToggle(unsigned int a_cheat, unsigned char enable) {
    unsigned int counter;
    unsigned char resetDMA = 0;
    cheatDMA = 0;

    counter = cheat[a_cheat].block;
    while (counter < (cheat[a_cheat].block + cheat[a_cheat].len)) {
        if (block[counter].flags & FLAG_DMA) {
            if (block[counter].hakVal != 0xFFFFFFFF) {
                cheatDMA = *((unsigned int *) (0x08800000 + (block[counter].hakVal & 0x1FFFFFF))) - 0x08800000;

                if (block[counter].stdVal != cheatDMA) {
                    resetDMA = 1;
                    block[counter].stdVal = cheatDMA;
                } else {
                    resetDMA = 0;
                }
            } else {
                cheatDMA = 0;
                resetDMA = 0;
            }
        } else {
            if (enable) {
                if (((cheatDMA) && (resetDMA)) ||
                    ((cheat[a_cheat].flags & FLAG_FRESH) && (block[counter].flags & FLAG_FREEZE))) {
                    switch (block[counter].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            block[counter].stdVal = *((unsigned int *) (cheatDMA + block[counter].address));
                            break;
                        case FLAG_WORD:
                            block[counter].stdVal = *((unsigned short *) (cheatDMA + block[counter].address));
                            break;
                        case FLAG_BYTE:
                            block[counter].stdVal = *((unsigned char *) (cheatDMA + block[counter].address));
                            break;
                    }
                    if (block[counter].flags & FLAG_FREEZE) {
                        block[counter].hakVal = block[counter].stdVal;
                    }
                }

                //Apply cheat!
                switch (block[counter].flags & FLAG_DWORD) {
                    case FLAG_DWORD:
                        *((unsigned int *) (cheatDMA + block[counter].address)) = block[counter].hakVal;
                        sceKernelDcacheWritebackInvalidateRange(cheatDMA + block[counter].address, 4);
                        sceKernelIcacheInvalidateRange(cheatDMA + block[counter].address, 4);
                        break;
                    case FLAG_WORD:
                        *((unsigned short *) (cheatDMA + block[counter].address)) = (unsigned short) block[counter].hakVal;
                        sceKernelDcacheWritebackInvalidateRange(cheatDMA + block[counter].address, 2);
                        sceKernelIcacheInvalidateRange(cheatDMA + block[counter].address, 2);
                        break;
                    case FLAG_BYTE:
                        *((unsigned char *) (cheatDMA + block[counter].address)) = (unsigned char) block[counter].hakVal;
                        sceKernelDcacheWritebackInvalidateRange(cheatDMA + block[counter].address, 1);
                        sceKernelIcacheInvalidateRange(cheatDMA + block[counter].address, 1);
                        break;
                }
            } else {
                if (!resetDMA) {
                    switch (block[counter].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            *((unsigned int *) (cheatDMA + block[counter].address)) = block[counter].stdVal;
                            sceKernelDcacheWritebackInvalidateRange(cheatDMA + block[counter].address, 4);
                            sceKernelIcacheInvalidateRange(cheatDMA + block[counter].address, 4);
                            break;
                        case FLAG_WORD:
                            *((unsigned short *) (cheatDMA + block[counter].address)) = (unsigned short) block[counter].stdVal;
                            sceKernelDcacheWritebackInvalidateRange(cheatDMA + block[counter].address, 2);
                            sceKernelIcacheInvalidateRange(cheatDMA + block[counter].address, 2);
                            break;
                        case FLAG_BYTE:
                            *((unsigned char *) (cheatDMA + block[counter].address)) = (unsigned char) block[counter].stdVal;
                            sceKernelDcacheWritebackInvalidateRange(cheatDMA + block[counter].address, 1);
                            sceKernelIcacheInvalidateRange(cheatDMA + block[counter].address, 1);
                            break;
                    }
                }
            }
            counter++;
        }
    }
}


void cheatEnable(unsigned int a_cheat) {
    cheatToggle(a_cheat, 1);
}

void cheatDisable(unsigned int a_cheat) {
    cheatToggle(a_cheat, 0);
}


void cheatApply(unsigned char a_type)//0=Enable/Disable FLAG_FRESH, -1=Enable only all (Freeze on all)
{
    unsigned int counter;

    if (!cheatSaved) return;
    if (!a_type) { cheatFlash = 2; }//Write the cheat twice even if CWCheat mode is off

    if (cheatStatus) {
        //Enable cheats
        counter = 0;
        while (counter < cheatTotal) {
            if ((cheat[counter].flags & FLAG_SELECTED) || (cheat[counter].flags & FLAG_CONSTANT)) {
                if (((!a_type) && (cheat[counter].flags & FLAG_FRESH)) || (a_type)) {
                    cheatEnable(counter);
                    cheat[counter].flags &= ~FLAG_FRESH;
                }
            } else if ((!a_type) && (cheat[counter].flags & FLAG_FRESH)) {
                cheatDisable(counter);
                cheat[counter].flags &= ~FLAG_FRESH;
            }
            counter++;
        }
    } else {
        //Disable cheats
        counter = 0;
        while (counter < cheatTotal) {
            if (cheat[counter].flags & FLAG_CONSTANT) {
                if (((!a_type) && (cheat[counter].flags & FLAG_FRESH)) || (a_type)) {
                    cheatEnable(counter);
                    cheat[counter].flags &= ~FLAG_FRESH;
                }
            } else if ((!a_type) && (cheat[counter].flags & FLAG_FRESH)) {
                cheatDisable(counter);
                cheat[counter].flags &= ~FLAG_FRESH;
            }
            counter++;
        }
    }
}

void cheatSave() {
    unsigned char fileChar = 0;
    unsigned char fileMisc[3];
    unsigned int fileSize = 0;
    unsigned int counter = -1;
    unsigned int scounter = 0;
    unsigned char fileMode = 0;//0=Unknown/Initial, 1=Comment, 2=Waiting for \n (ignoring)
    int fd;
    int tempFd;

    //1) Open the original cheat file
    fd = sceIoOpen(gameDir, PSP_O_RDONLY, 0777);
    if (fd > 0) {
        //Find the file size
        fileSize = sceIoLseek(fd, 0, SEEK_END);
        sceIoLseek(fd, 0, SEEK_SET);

        //Initiate the read buffer
        fileBufferOffset = 1024;
        fileBufferFileOffset = 0;

        //2) Open up the temporary and get ready to regenerate it
        tempFd = sceIoOpen("ms0:/seplugins/cheatER/temp.txt", PSP_O_WRONLY | PSP_O_CREAT, 0777);
        if (tempFd <= 0) {
            sceIoClose(fd);
            return;
        }

        //Add the codes that are already there
        while (fileBufferFileOffset < fileSize) {
            //Read a byte
            fileBufferRead(&fileChar, 1);
            if (fileBufferSize == 0) break;

            //Interpret the byte based on the mode
            if (fileMode == 0) {
                //Pick a mode
                switch (fileChar) {
                    case ';':
                        fileMode = 1;
                        sceIoWrite(tempFd, ";", 1);
                        break;

                    case '#':
                        fileMode = 2;
                        counter++;
                        //Add a double line skip?
                        if (counter != 0) {
                            sceIoWrite(tempFd, "\r\n", 2);
                        }

                        //Is there an error...?
                        if (counter >= cheatTotal) {
                            sceIoClose(tempFd);
                            sceIoClose(fd);
                            return;
                        }

                        //Set up the subCounter
                        scounter = cheat[counter].block;

                        //Is it on by default...?
                        if (cheat[counter].flags & FLAG_CONSTANT) {
                            sceIoWrite(tempFd, "#!!", 3);
                        } else if (cheat[counter].flags & FLAG_SELECTED) {
                            sceIoWrite(tempFd, "#!", 2);
                        } else {
                            sceIoWrite(tempFd, "#", 1);
                        }
                        //Write out the name of the cheat
                        sceIoWrite(tempFd, &cheat[counter].name, strlen(cheat[counter].name));
                        sceIoWrite(tempFd, "\r\n", 2);
                        break;

                    case '0':
                        if ((fileBufferFileOffset) < fileSize) {
                            fileBufferPeek(fileMisc[0], 0);
                            if (fileMisc[0] == 'x') {
                                //Is there an error...?
                                if (counter == (unsigned int) -1) {
                                    sceIoClose(tempFd);
                                    sceIoClose(fd);
                                    return;
                                }
                                if (scounter >= (cheat[counter].block + cheat[counter].len)) {
                                    sceIoClose(tempFd);
                                    sceIoClose(fd);
                                    return;
                                }

                                //Write out the address
                                if (block[scounter].flags & FLAG_DMA) {
                                    sprintf(buffer, "0x%08lX ", (block[scounter].address));
                                    sceIoWrite(tempFd, buffer, strlen(buffer));
                                } else {
                                    sprintf(buffer, "0x%08lX ", (block[scounter].address - 0x08800000));
                                    sceIoWrite(tempFd, buffer, strlen(buffer));
                                }

                                //Write out the value
                                if (block[scounter].flags & FLAG_FREEZE) {
                                    switch (block[scounter].flags & FLAG_DWORD) {
                                        case FLAG_DWORD:
                                            sprintf(buffer, "0x________\r\n");
                                            break;

                                        case FLAG_WORD:
                                            sprintf(buffer, "0x____\r\n");
                                            break;

                                        case FLAG_BYTE:
                                            sprintf(buffer, "0x__\r\n");
                                            break;
                                    }
                                } else {
                                    switch (block[scounter].flags & FLAG_DWORD) {
                                        case FLAG_DWORD:
                                            sprintf(buffer, "0x%08lX\r\n", block[scounter].hakVal);
                                            break;

                                        case FLAG_WORD:
                                            sprintf(buffer, "0x%04hX\r\n", (unsigned short) block[scounter].hakVal);
                                            break;

                                        case FLAG_BYTE:
                                            sprintf(buffer, "0x%02hX\r\n", (unsigned char) block[scounter].hakVal);
                                            break;
                                    }
                                }
                                sceIoWrite(tempFd, buffer, strlen(buffer));

                                //Skip the rest
                                fileMode = 2;
                                scounter++;
                            }
                        }
                        break;
                }
            } else if (fileMode == 1) {
                //Just copy it out straight to the file
                if ((fileChar == '\r') || (fileChar == '\n')) {
                    sceIoWrite(tempFd, "\r\n", 2);
                    fileMode = 0;
                } else {
                    sceIoWrite(tempFd, &fileChar, 1);
                }
            } else if (fileMode == 2) {
                //Just wait for an '\r' or '\n'
                if ((fileChar == '\r') || (fileChar == '\n')) {
                    fileMode = 0;
                }
            }
        }

        //Close the files
        sceIoClose(tempFd);
        sceIoClose(fd);

        //Delete the old file, rename the temporary
        sceIoRemove(gameDir);
        sceIoRename("ms0:/seplugins/cheatER/temp.txt", gameDir);
    }

    //Open the file for appending
    fd = sceIoOpen(gameDir, PSP_O_CREAT | PSP_O_WRONLY | PSP_O_APPEND, 0777);
    if (fd > 0) {
        //Add any new codes
        counter++;
        if (counter != 0) sceIoWrite(fd, "\r\n", 2);
        while (counter < cheatTotal) {
            //Write the cheat name
            if (cheat[counter].flags & FLAG_CONSTANT) {
                sceIoWrite(fd, "#!!", 3);
            } else if (cheat[counter].flags & FLAG_SELECTED) {
                sceIoWrite(fd, "#!", 2);
            } else {
                sceIoWrite(fd, "#", 1);
            }

            //Write out the name of the cheat
            sceIoWrite(fd, &cheat[counter].name, strlen(cheat[counter].name));
            sceIoWrite(fd, "\r\n", 2);

            //Loop through the addresses
            scounter = cheat[counter].block;
            while (scounter < (cheat[counter].block + cheat[counter].len)) {
                //Write out the address
                sprintf(buffer, "0x%08lX ", (block[scounter].address - 0x08800000));
                sceIoWrite(fd, buffer, strlen(buffer));

                //Write out the value
                switch (block[scounter].flags & FLAG_DWORD) {
                    case FLAG_DWORD:
                        sprintf(buffer, "0x%08lX\r\n", block[scounter].hakVal);
                        break;

                    case FLAG_WORD:
                        sprintf(buffer, "0x%04hX\r\n", (unsigned short) block[scounter].hakVal);
                        break;

                    case FLAG_BYTE:
                        sprintf(buffer, "0x%02hX\r\n", (unsigned char) block[scounter].hakVal);
                        break;
                }
                sceIoWrite(fd, buffer, strlen(buffer));

                //Next address
                scounter++;
            }

            //Next cheat
            counter++;
            sceIoWrite(fd, "\r\n", 2);
        }

        //Close the file
        sceIoClose(fd);
    }
}

void buttonCallback(int curr, int last, void *arg) {
    unsigned int counter;
    unsigned int scounter;
    unsigned int address;

    if (vram == NULL) return;

    if (((curr & menuKey) == menuKey) && (!menuDrawn)) {
        menuDrawn = 1;
        if (cheatSelected >= cheatTotal) cheatSelected = 0;
        tabSelected = 0;
    } else if (((curr & screenKey) == screenKey) && (!menuDrawn)) {
        screenTime = 1;
    } else if (((curr & triggerKey) == triggerKey) && (!menuDrawn)) {
        //Backup all the cheat "blocks"
        if (!cheatSaved) {
            counter = 0;
            scounter = 0;
            while (counter < blockTotal) {
                if (cheat[scounter].block == counter) {
                    cheatDMA = 0;//Reset DNA on every new cheat
                    scounter++;
                }

                if (block[counter].flags & FLAG_DMA) {
                    if (block[counter].hakVal != 0xFFFFFFFF) {
                        cheatDMA = *((unsigned int *) (0x08800000 + (block[counter].hakVal & 0x1FFFFFF))) - 0x08800000;

                        if (((cheatDMA >= 0x00004000) && (cheatDMA < 0x01800000)) ||
                            ((cheatDMA >= 0x40004000) && (cheatDMA < 0x41800000))) {
                            block[counter].stdVal = cheatDMA;
                        }
                    } else {
                        cheatDMA = 0;
                    }
                } else {
                    address = cheatDMA + block[counter].address;

                    if (((address >= 0x08804000) && (address < 0x0A000000)) ||
                        ((address >= START_ADDRESS) && (address < END_ADDRESS))) {
                        switch (block[counter].flags & FLAG_DWORD) {
                            case FLAG_DWORD:
                                if (address % 4 == 0) {
                                    block[counter].stdVal = *((unsigned int *) (address));
                                }
                                break;
                            case FLAG_WORD:
                                if (address % 2 == 0) {
                                    block[counter].stdVal = *((unsigned short *) (address));
                                }
                                break;
                            case FLAG_BYTE:
                                block[counter].stdVal = *((unsigned char *) (address));
                                break;
                        }
                    }
                }
                counter++;
            }
        }

        //Turn on/off cheats
        cheatStatus = !cheatStatus;

        //Apply the cheats accordingly (make em fresh)
        counter = 0;
        while (counter < cheatTotal) {
            if ((cheat[counter].flags & FLAG_CONSTANT) && (!cheatSaved)) {
                cheat[counter].flags |= FLAG_FRESH;
                cheatEnable(counter);
                cheat[counter].flags &= ~FLAG_FRESH;
            }
            if (cheat[counter].flags & FLAG_SELECTED) {
                cheat[counter].flags |= FLAG_FRESH;
                if (cheatStatus) {
                    cheatEnable(counter);
                } else {
                    cheatDisable(counter);
                }
                cheat[counter].flags &= ~FLAG_FRESH;
            }
            counter++;
        }
        cheatSaved = 1;

        //Wait 0.5 seconds
        sceKernelDelayThread(500000);
    }
}

static void gamePause(SceUID thid) {
    if (pauseuid >= 0)
        return;
    pauseuid = thid;
    sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, thread_buf_now, MAX_THREAD, &thread_count_now);
    int x, y, match;
    for (x = 0; x < thread_count_now; x++) {
        match = 0;
        SceUID tmp_thid = thread_buf_now[x];
        for (y = 0; y < thread_count_start; y++) {
            if ((tmp_thid == thread_buf_start[y]) || (tmp_thid == thid)) {
                match = 1;
                break;
            }
        }
        if (match == 0)
            sceKernelSuspendThread(tmp_thid);
    }
}

static void gameResume(SceUID thid) {
    if (pauseuid != thid)
        return;
    pauseuid = -1;
    int x, y, match;
    for (x = 0; x < thread_count_now; x++) {
        match = 0;
        SceUID tmp_thid = thread_buf_now[x];
        for (y = 0; y < thread_count_start; y++) {
            if ((tmp_thid == thread_buf_start[y]) || (tmp_thid == thid)) {
                match = 1;
                break;
            }
        }
        if (match == 0)
            sceKernelResumeThread(tmp_thid);
    }
}

#include "headers/mips.h"

signed char lolDirection = 1;
unsigned char lolValue = 0;
unsigned char lolInit = 1;

void menuDraw() {
    unsigned int counter;
    unsigned int scounter;
    unsigned int convBase;
    unsigned int convTotal;

    //Draw the menu

    pspDebugScreenSetXY(0, 0);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPuts("Eps cheatER\nThe NitePR Revamp\n\n");
    pspDebugScreenSetTextColor(0xFF00FFFF);
    pspDebugScreenSetTextColor(cheatStatus ? 0xFF00FF00 : 0xFF0000FF);
    pspDebugScreenPuts(
            cheatStatus ? "[CHEATS ARE ON]\n\n" : (!cheatSaved ? "[ALL CHEATS ARE OFF]\n\n" : "[CHEATS ARE OFF]\n\n"));

    //User friendly note
    if (lolInit) {
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPuts("[Welcome!]");
        pspDebugScreenSetTextColor(0xFF808080);
        pspDebugScreenPuts("\n--------------------------------------------------------------------");
        pspDebugScreenSetTextColor(0xFF0000FF);
        pspDebugScreenPuts(
                "Please tell CheatER when to generate OFF codes by pressing\nthe MUSICAL NOTE button while inside the game at least once\nand NOT when the cheat menu is showing!!!\n\n");
        pspDebugScreenSetTextColor(0xFF00FF00);
        pspDebugScreenPuts(
                "HINTs:\n - To take a screenshot, press DOWN + SQUARE\n\n - Some cheats need the PRX->Cheat Hz option to be set to 15/1000\n   This is for cheats that overwrite a changing value\n\n");
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        return;
    }

    //Extended/sub menus?
    if (extMenu) {
        switch (extMenu) {
            case 1://DRAW EXT CHEAT
                //Draw the tabs
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("[Editing Cheat: '");
                pspDebugScreenPuts(cheat[cheatSelected].name);
                pspDebugScreenPuts("'] ");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("\n--------------------------------------------------------------------");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("  Address     Value.Hex   Value.Dec   Value.ASCII  Value.Float\n");

                //Print out the cheat lines
                convBase = cheat[cheatSelected].block;
                convTotal = cheat[cheatSelected].len;
                counter = cheat[cheatSelected].block;
                while (counter < (cheat[cheatSelected].block + cheat[cheatSelected].len)) {
                    //Scroll feature right here, in two lines =3
                    if ((signed int) (counter - convBase) < (signed int) (((extSelected[0] - convBase) - 11) -
                                                                          ((((signed int) (extSelected[0] - convBase) +
                                                                             11) -
                                                                            ((signed int) convTotal)) > 0
                                                                                   ? abs(
                                                                                             ((signed int) (extSelected[0] -
                                                                                                            convBase) +
                                                                                              11) -
                                                                                             ((signed int) convTotal))
                                                                                   : 0))) {
                        counter++;
                        continue;
                    }
                    if ((signed int) (counter - convBase) > (signed int) (((extSelected[0] - convBase) + 11) +
                                                                          (((signed int) (extSelected[0] - convBase) -
                                                                            11) < 0
                                                                                   ? abs((signed int) ((extSelected[0] - convBase) - 11))
                                                                                   : 0))) {
                        counter++;
                        continue;
                    }

                    //Apply the row color
                    if (counter == extSelected[0]) {
                        pspDebugScreenSetTextColor(0xFF8080BB);
                    } else {
                        pspDebugScreenSetTextColor(0xFFBBBBBB);
                    }

                    //Print out the address
                    if (block[counter].flags & FLAG_DMA) {
                        pspDebugScreenPuts("  0xFFFFFFFF  ");
                    } else {
                        sprintf(buffer, "  0x%08lX  ", block[counter].address - 0x08800000);
                        pspDebugScreenPuts(buffer);
                    }

                    //Print out the hex
                    switch (block[counter].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            sprintf(buffer, "0x%08lX  ", block[counter].hakVal);
                            break;
                        case FLAG_WORD:
                            sprintf(buffer, "0x____%04hX  ", (unsigned short) block[counter].hakVal);
                            break;
                        case FLAG_BYTE:
                            sprintf(buffer, "0x______%02hX  ", (unsigned char) block[counter].hakVal);
                            break;
                    }
                    pspDebugScreenPuts(buffer);

                    //Print out the decimal
                    switch (block[counter].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            sprintf(buffer, "%010lu  ", block[counter].hakVal);
                            break;
                        case FLAG_WORD:
                            sprintf(buffer, "%010lu  ", (unsigned short) block[counter].hakVal);
                            break;
                        case FLAG_BYTE:
                            sprintf(buffer, "%010lu  ", (unsigned char) block[counter].hakVal);
                            break;
                    }
                    pspDebugScreenPuts(buffer);

                    //Print out the ASCII
                    buffer[0] = *((unsigned char *) (((unsigned int) &block[counter].hakVal) + 0));
                    if ((buffer[0] <= 0x20) || (buffer[0] == 0xFF)) buffer[0] = '.';
                    buffer[1] = *((unsigned char *) (((unsigned int) &block[counter].hakVal) + 1));
                    if ((buffer[1] <= 0x20) || (buffer[1] == 0xFF)) buffer[1] = '.';
                    buffer[2] = *((unsigned char *) (((unsigned int) &block[counter].hakVal) + 2));
                    if ((buffer[2] <= 0x20) || (buffer[2] == 0xFF)) buffer[2] = '.';
                    buffer[3] = *((unsigned char *) (((unsigned int) &block[counter].hakVal) + 3));
                    if ((buffer[3] <= 0x20) || (buffer[3] == 0xFF)) buffer[3] = '.';
                    buffer[4] = 0;
                    pspDebugScreenPuts(buffer);

                    //Print out the float
                    pspDebugScreenPuts("         ");
                    f_cvt(&block[counter].hakVal, buffer, sizeof(buffer), 6, MODE_GENERIC);
                    pspDebugScreenPuts(buffer);

                    //Skip a line, draw the pointer =)
                    if (counter == extSelected[0]) {
                        //Skip the initial line
                        pspDebugScreenPuts("\n");

                        //Skip the desired amount?
                        pspDebugScreenPuts("    ");
                        if (extSelected[1] != 0)//Skip address
                        {
                            pspDebugScreenPuts("            ");
                            if (extSelected[1] != 1)//Skip Hex
                            {
                                pspDebugScreenPuts("          ");
                                if (extSelected[1] != 2)//Skip Dec
                                {
                                    pspDebugScreenPuts("            ");
                                    //Skip ASCII
                                }
                            }
                        }

                        //Skip the minimalist amount
                        unsigned char tempCounter = extSelected[2];
                        while (tempCounter) {
                            pspDebugScreenPuts(" ");
                            tempCounter--;
                        }

                        //Draw the symbol (Finally!!)
                        if (extSelected[3]) {
                            pspDebugScreenSetTextColor(0xFF0000FF);
                        } else {
                            pspDebugScreenSetTextColor(0xFFFF0000);
                        }
                        pspDebugScreenPuts("^");
                    }

                    //Goto the next cheat down under
                    pspDebugScreenPuts("\n");
                    counter++;
                }

                //Helper
                pspDebugScreenSetXY(0, 33);
                pspDebugScreenSetTextColor(0xFFFF8000);
                pspDebugScreenPuts(">< = Edit On/Off; \\|/ /|\\ <- -> = Cycle; [] = Alt-Type; () = Close");
                break;

            case 2://DRAW EXT SEARCH
                //Draw the tabs
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("[Editing Search]");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("\n--------------------------------------------------------------------");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("  Value.Hex   Value.Dec   Value.ASCII  Value.Float\n");

                //Apply the row color
                if (extSelected[0] == 0) {
                    pspDebugScreenSetTextColor(0xFF8080BB);
                } else {
                    pspDebugScreenSetTextColor(0xFFBBBBBB);
                }

                //Print out the hex
                switch (searchHistory[0].flags & FLAG_DWORD) {
                    case FLAG_DWORD:
                        sprintf(buffer, "  0x%08lX  ", searchHistory[0].hakVal);
                        break;
                    case FLAG_WORD:
                        sprintf(buffer, "  0x____%04hX  ", (unsigned short) searchHistory[0].hakVal);
                        break;
                    case FLAG_BYTE:
                        sprintf(buffer, "  0x______%02hX  ", (unsigned char) searchHistory[0].hakVal);
                        break;
                }
                pspDebugScreenPuts(buffer);

                //Print out the decimal
                switch (searchHistory[0].flags & FLAG_DWORD) {
                    case FLAG_DWORD:
                        sprintf(buffer, "%010u  ", searchHistory[0].hakVal);
                        break;
                    case FLAG_WORD:
                        sprintf(buffer, "%010u  ", (unsigned short) searchHistory[0].hakVal);
                        break;
                    case FLAG_BYTE:
                        sprintf(buffer, "%010u  ", (unsigned char) searchHistory[0].hakVal);
                        break;
                }
                pspDebugScreenPuts(buffer);

                //Print out the ASCII
                buffer[0] = *((unsigned char *) (((unsigned int) &searchHistory[0].hakVal) + 0));
                if ((buffer[0] <= 0x20) || (buffer[0] == 0xFF)) buffer[0] = '.';
                buffer[1] = *((unsigned char *) (((unsigned int) &searchHistory[0].hakVal) + 1));
                if ((buffer[1] <= 0x20) || (buffer[1] == 0xFF)) buffer[1] = '.';
                buffer[2] = *((unsigned char *) (((unsigned int) &searchHistory[0].hakVal) + 2));
                if ((buffer[2] <= 0x20) || (buffer[2] == 0xFF)) buffer[2] = '.';
                buffer[3] = *((unsigned char *) (((unsigned int) &searchHistory[0].hakVal) + 3));
                if ((buffer[3] <= 0x20) || (buffer[3] == 0xFF)) buffer[3] = '.';
                buffer[4] = 0;
                pspDebugScreenPuts(buffer);

                //Print out the float
                pspDebugScreenPuts("         ");
                f_cvt(&searchHistory[0].hakVal, buffer, sizeof(buffer), 6, MODE_GENERIC);
                pspDebugScreenPuts(buffer);

                //Skip a line, draw the pointer =)
                pspDebugScreenPuts("\n");
                if (extSelected[0] == 0) {
                    //Skip the desired amount?
                    pspDebugScreenPuts("    ");
                    if (extSelected[1] != 0)//Skip Hex
                    {
                        pspDebugScreenPuts("          ");
                        if (extSelected[1] != 1)//Skip Dec
                        {
                            pspDebugScreenPuts("            ");
                            //Skip ASCII
                        }
                    }

                    //Skip the minimalist amount
                    unsigned char tempCounter = extSelected[2];
                    while (tempCounter) {
                        pspDebugScreenPuts(" ");
                        tempCounter--;
                    }

                    //Draw the symbol (Finally!!)
                    if (extSelected[3]) {
                        pspDebugScreenSetTextColor(0xFF0000FF);
                    } else {
                        pspDebugScreenSetTextColor(0xFFFF0000);
                    }
                    pspDebugScreenPuts("^");
                }
                pspDebugScreenPuts("\n");

                //Draw the misc menus
                pspDebugScreenSetTextColor(extSelected[0] == 1 ? 0xFF0000FF : 0xFF808080);
                pspDebugScreenPuts("  Search\n");
                if (searchNo) {
                    pspDebugScreenSetTextColor(extSelected[0] == 2 ? 0xFF0000FF : 0xFF808080);
                    pspDebugScreenPuts("  Undo Search\n");
                }

                //Print out results
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("\n[Search Results: ");
                sprintf(buffer, "%d Found - Only showing first 100]", searchResultCounter);
                pspDebugScreenPuts(buffer);
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("\n--------------------------------------------------------------------");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("  Address     Value.Hex   Value.Dec   Value.ASCII  Value.Float\n");

                //Print out the results variables
                convTotal = ((searchResultCounter > 100) ? 100 : searchResultCounter);
                counter = 0;
                while (counter < convTotal) {
                    //Scroll feature right here, in two lines =3
                    if ((signed int) (counter) < (signed int) (((extSelected[0] - 3) - 7) -
                                                               ((((signed int) (extSelected[0] - 3) + 7) -
                                                                 ((signed int) convTotal)) > 0
                                                                        ? abs(
                                                                                  ((signed int) (extSelected[0] - 3) + 7) -
                                                                                  ((signed int) convTotal))
                                                                        : 0))) {
                        counter++;
                        continue;
                    }
                    if ((signed int) (counter) > (signed int) (((extSelected[0] - 3) + 7) +
                                                               (((signed int) (extSelected[0] - 3) - 7) < 0 ? abs(
                                                                                                                      (signed int) ((extSelected[0] - 3) - 7))
                                                                                                            : 0))) {
                        counter++;
                        continue;
                    }

                    //Apply the row color
                    if (counter == (extSelected[0] - 3)) {
                        pspDebugScreenSetTextColor(0xFF8080BB);
                    } else {
                        pspDebugScreenSetTextColor(0xFFBBBBBB);
                    }

                    //Print out the address
                    sprintf(buffer, "  0x%08lX  ", (searchAddress[counter] - 0x48800000));
                    pspDebugScreenPuts(buffer);

                    //Print out the hex
                    switch (searchHistory[0].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            sprintf(buffer, "0x%08lX  ", *((unsigned int *) (searchAddress[counter])));
                            break;
                        case FLAG_WORD:
                            sprintf(buffer, "0x____%04hX  ", *((unsigned short *) (searchAddress[counter])));
                            break;
                        case FLAG_BYTE:
                            sprintf(buffer, "0x______%02hX  ", *((unsigned char *) (searchAddress[counter])));
                            break;
                    }
                    pspDebugScreenPuts(buffer);

                    //Print out the decimal
                    switch (searchHistory[0].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            sprintf(buffer, "%010lu  ", *((unsigned int *) (searchAddress[counter])));
                            break;
                        case FLAG_WORD:
                            sprintf(buffer, "%010lu  ", *((unsigned short *) (searchAddress[counter])));
                            break;
                        case FLAG_BYTE:
                            sprintf(buffer, "%010lu  ", *((unsigned char *) (searchAddress[counter])));
                            break;
                    }
                    pspDebugScreenPuts(buffer);

                    //Print out the ASCII
                    buffer[0] = *((unsigned char *) (searchAddress[counter] + 0));
                    if ((buffer[0] <= 0x20) || (buffer[0] == 0xFF)) buffer[0] = '.';
                    if ((searchHistory[0].flags & FLAG_DWORD) != FLAG_BYTE) {
                        buffer[1] = *((unsigned char *) (searchAddress[counter] + 1));
                        if ((buffer[1] <= 0x20) || (buffer[1] == 0xFF)) buffer[1] = '.';
                    } else {
                        buffer[1] = '.';
                    }
                    if ((searchHistory[0].flags & FLAG_DWORD) == FLAG_DWORD) {
                        buffer[2] = *((unsigned char *) (searchAddress[counter] + 2));
                        if ((buffer[2] <= 0x20) || (buffer[2] == 0xFF)) buffer[2] = '.';
                        buffer[3] = *((unsigned char *) (searchAddress[counter] + 3));
                        if ((buffer[3] <= 0x20) || (buffer[3] == 0xFF)) buffer[3] = '.';
                    } else {
                        buffer[2] = buffer[3] = '.';
                    }
                    buffer[4] = 0;
                    pspDebugScreenPuts(buffer);

                    //Print out the float
                    pspDebugScreenPuts("         ");
                    f_cvt(searchAddress[counter], buffer, sizeof(buffer), 6, MODE_GENERIC);
                    pspDebugScreenPuts(buffer);

                    //Goto the next cheat down under
                    pspDebugScreenPuts("\n");
                    counter++;
                }

                //Helper
                pspDebugScreenSetXY(0, 33);
                pspDebugScreenSetTextColor(0xFFFF8000);
                if (extSelected[0] == 0) {
                    if (searchNo == 0) {
                        pspDebugScreenPuts(">< = Edit On/Off; \\|/ /|\\ <- -> = Cycle; [] = Alt-Type; () = Cancel");
                    } else {
                        pspDebugScreenPuts(">< = Edit On/Off; \\|/ /|\\ <- -> = Cycle; () = Cancel               ");
                    }
                } else if ((extSelected[0] == 1) || (extSelected[0] == 2)) {
                    pspDebugScreenPuts(">< = Select; () = Cancel                                           ");
                } else {
                    pspDebugScreenPuts(">< = Add Selected Cheat; () = Cancel                               ");
                }
                break;

            case 3://DRAW EXT DIFF SEARCH
                //Draw the tabs
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("[Editing Search]");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("\n--------------------------------------------------------------------");
                pspDebugScreenSetTextColor(0xFF808080);
                if (searchMode > 3) {
                    pspDebugScreenPuts("  Mode          Value.Hex   Value.Dec   Value.ASCII  Value.Float\n");
                } else {
                    pspDebugScreenPuts("  Mode                                                          \n");
                }

                //Apply the row color
                if (extSelected[0] == 0) {
                    pspDebugScreenSetTextColor(0xFF8080BB);
                } else {
                    pspDebugScreenSetTextColor(0xFFBBBBBB);
                }

                //Print out the mode name
                pspDebugScreenPuts(searchModeName[searchMode]);

                if (searchMode > 3) {
                    //Print out the hex
                    switch (searchHistory[0].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            sprintf(buffer, "  0x%08lX  ", searchHistory[0].hakVal);
                            break;
                        case FLAG_WORD:
                            sprintf(buffer, "  0x____%04hX  ", (unsigned short) searchHistory[0].hakVal);
                            break;
                        case FLAG_BYTE:
                            sprintf(buffer, "  0x______%02hX  ", (unsigned char) searchHistory[0].hakVal);
                            break;
                    }
                    pspDebugScreenPuts(buffer);

                    //Print out the decimal
                    switch (searchHistory[0].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            sprintf(buffer, "%010u  ", searchHistory[0].hakVal);
                            break;
                        case FLAG_WORD:
                            sprintf(buffer, "%010u  ", (unsigned short) searchHistory[0].hakVal);
                            break;
                        case FLAG_BYTE:
                            sprintf(buffer, "%010u  ", (unsigned char) searchHistory[0].hakVal);
                            break;
                    }
                    pspDebugScreenPuts(buffer);

                    //Print out the ASCII
                    buffer[0] = *((unsigned char *) (((unsigned int) &searchHistory[0].hakVal) + 0));
                    if ((buffer[0] <= 0x20) || (buffer[0] == 0xFF)) buffer[0] = '.';
                    buffer[1] = *((unsigned char *) (((unsigned int) &searchHistory[0].hakVal) + 1));
                    if ((buffer[1] <= 0x20) || (buffer[1] == 0xFF)) buffer[1] = '.';
                    buffer[2] = *((unsigned char *) (((unsigned int) &searchHistory[0].hakVal) + 2));
                    if ((buffer[2] <= 0x20) || (buffer[2] == 0xFF)) buffer[2] = '.';
                    buffer[3] = *((unsigned char *) (((unsigned int) &searchHistory[0].hakVal) + 3));
                    if ((buffer[3] <= 0x20) || (buffer[3] == 0xFF)) buffer[3] = '.';
                    buffer[4] = 0;
                    pspDebugScreenPuts(buffer);

                    //Print out the float
                    pspDebugScreenPuts("         ");
                    f_cvt(&searchHistory[0].hakVal, buffer, sizeof(buffer), 6, MODE_GENERIC);
                    pspDebugScreenPuts(buffer);
                }

                //Skip a line, draw the pointer =)
                pspDebugScreenPuts("\n");
                if (extSelected[0] == 0) {
                    //Skip the desired amount?
                    pspDebugScreenPuts("    ");
                    if (extSelected[1] != 0)//Skip Mode
                    {
                        pspDebugScreenPuts("              ");
                        if (extSelected[1] != 1)//Skip Hex
                        {
                            pspDebugScreenPuts("          ");
                            if (extSelected[1] != 2)//Skip Dec
                            {
                                pspDebugScreenPuts("            ");
                                //Skip ASCII
                            }
                        }
                    }

                    //Skip the minimalist amount
                    unsigned char tempCounter = extSelected[2];
                    while (tempCounter) {
                        pspDebugScreenPuts(" ");
                        tempCounter--;
                    }

                    //Draw the symbol (Finally!!)
                    if (extSelected[3]) {
                        pspDebugScreenSetTextColor(0xFF0000FF);
                    } else {
                        pspDebugScreenSetTextColor(0xFFFF0000);
                    }
                    pspDebugScreenPuts("^");
                }
                pspDebugScreenPuts("\n");

                //Draw the misc menus
                pspDebugScreenSetTextColor(extSelected[0] == 1 ? 0xFF0000FF : 0xFF808080);
                pspDebugScreenPuts("  Search\n");
                if (searchNo) {
                    pspDebugScreenSetTextColor(extSelected[0] == 2 ? 0xFF0000FF : 0xFF808080);
                    pspDebugScreenPuts("  Undo Search\n");
                }

                //Print out results
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("\n[Search Results: ");
                sprintf(buffer, "%d Found - Only showing first 100]", searchResultCounter);
                pspDebugScreenPuts(buffer);
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("\n--------------------------------------------------------------------");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("  Address     Value.Hex   Value.Dec   Value.ASCII  Value.Float\n");

                //Print out the results variables
                convTotal = ((searchResultCounter > 100) ? 100 : searchResultCounter);
                counter = 0;
                while (counter < convTotal) {
                    //Scroll feature right here, in two lines =3
                    if ((signed int) (counter) < (signed int) (((extSelected[0] - 3) - 7) -
                                                               ((((signed int) (extSelected[0] - 3) + 7) -
                                                                 ((signed int) convTotal)) > 0
                                                                        ? abs(
                                                                                  ((signed int) (extSelected[0] - 3) + 7) -
                                                                                  ((signed int) convTotal))
                                                                        : 0))) {
                        counter++;
                        continue;
                    }
                    if ((signed int) (counter) > (signed int) (((extSelected[0] - 3) + 7) +
                                                               (((signed int) (extSelected[0] - 3) - 7) < 0 ? abs(
                                                                                                                      (signed int) ((extSelected[0] - 3) - 7))
                                                                                                            : 0))) {
                        counter++;
                        continue;
                    }

                    //Apply the row color
                    if (counter == (extSelected[0] - 3)) {
                        pspDebugScreenSetTextColor(0xFF8080BB);
                    } else {
                        pspDebugScreenSetTextColor(0xFFBBBBBB);
                    }

                    //Print out the address
                    sprintf(buffer, "  0x%08lX  ", (searchAddress[counter] - 0x48800000));
                    pspDebugScreenPuts(buffer);

                    //Print out the hex
                    switch (searchHistory[0].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            sprintf(buffer, "0x%08lX  ", *((unsigned int *) (searchAddress[counter])));
                            break;
                        case FLAG_WORD:
                            sprintf(buffer, "0x____%04hX  ", *((unsigned short *) (searchAddress[counter])));
                            break;
                        case FLAG_BYTE:
                            sprintf(buffer, "0x______%02hX  ", *((unsigned char *) (searchAddress[counter])));
                            break;
                    }
                    pspDebugScreenPuts(buffer);

                    //Print out the decimal
                    switch (searchHistory[0].flags & FLAG_DWORD) {
                        case FLAG_DWORD:
                            sprintf(buffer, "%010lu  ", *((unsigned int *) (searchAddress[counter])));
                            break;
                        case FLAG_WORD:
                            sprintf(buffer, "%010lu  ", *((unsigned short *) (searchAddress[counter])));
                            break;
                        case FLAG_BYTE:
                            sprintf(buffer, "%010lu  ", *((unsigned char *) (searchAddress[counter])));
                            break;
                    }
                    pspDebugScreenPuts(buffer);

                    //Print out the ASCII
                    buffer[0] = *((unsigned char *) (searchAddress[counter] + 0));
                    if ((buffer[0] <= 0x20) || (buffer[0] == 0xFF)) buffer[0] = '.';
                    if ((searchHistory[0].flags & FLAG_DWORD) != FLAG_BYTE) {
                        buffer[1] = *((unsigned char *) (searchAddress[counter] + 1));
                        if ((buffer[1] <= 0x20) || (buffer[1] == 0xFF)) buffer[1] = '.';
                    } else {
                        buffer[1] = '.';
                    }
                    if ((searchHistory[0].flags & FLAG_DWORD) == FLAG_DWORD) {
                        buffer[2] = *((unsigned char *) (searchAddress[counter] + 2));
                        if ((buffer[2] <= 0x20) || (buffer[2] == 0xFF)) buffer[2] = '.';
                        buffer[3] = *((unsigned char *) (searchAddress[counter] + 3));
                        if ((buffer[3] <= 0x20) || (buffer[3] == 0xFF)) buffer[3] = '.';
                    } else {
                        buffer[2] = buffer[3] = '.';
                    }
                    buffer[4] = 0;
                    pspDebugScreenPuts(buffer);

                    //Print out the float
                    pspDebugScreenPuts("         ");
                    f_cvt(searchAddress[counter], buffer, sizeof(buffer), 6, MODE_GENERIC);
                    pspDebugScreenPuts(buffer);

                    //Goto the next cheat down under
                    pspDebugScreenPuts("\n");
                    counter++;
                }

                //Helper
                pspDebugScreenSetXY(0, 33);
                pspDebugScreenSetTextColor(0xFFFF8000);
                if (extSelected[0] == 0) {
                    pspDebugScreenPuts(">< = Edit On/Off; \\|/ /|\\ <- -> = Cycle; () = Cancel               ");
                } else if ((extSelected[0] == 1) || (extSelected[0] == 2)) {
                    pspDebugScreenPuts(">< = Select; () = Cancel                                           ");
                } else {
                    pspDebugScreenPuts(">< = Add Selected Cheat; () = Cancel                               ");
                }
                break;

            case 4://DRAW EXT TEXT search
                //Draw the tabs
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("[Editing Search]");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("\n--------------------------------------------------------------------");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("  Text                                                          \n");

                //Apply the row color
                if (extSelected[0] == 0) {
                    pspDebugScreenSetTextColor(0xFF8080BB);
                } else {
                    pspDebugScreenSetTextColor(0xFFBBBBBB);
                }

                //Print out the ASCII
                pspDebugScreenPuts("  '");
                fileBuffer[50] = 0;
                pspDebugScreenPuts(fileBuffer);

                //Skip a line, draw the pointer =)
                pspDebugScreenPuts("'\n");
                if (extSelected[0] == 0) {
                    //Skip the desired amount?
                    pspDebugScreenPuts("   ");

                    //Skip the minimalist amount
                    unsigned char tempCounter = extSelected[2];
                    while (tempCounter) {
                        pspDebugScreenPuts(" ");
                        tempCounter--;
                    }

                    //Draw the symbol (Finally!!)
                    if (extSelected[3]) {
                        pspDebugScreenSetTextColor(0xFF0000FF);
                    } else {
                        pspDebugScreenSetTextColor(0xFFFF0000);
                    }
                    pspDebugScreenPuts("^");
                }
                pspDebugScreenPuts("\n");

                //Draw the misc menus
                pspDebugScreenSetTextColor(extSelected[0] == 1 ? 0xFF0000FF : 0xFF808080);
                pspDebugScreenPuts("  Search\n");

                //Print out results
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("\n[Search Results: Only showing first 100]");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("\n--------------------------------------------------------------------");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("  Address   Text                                              \n");

                //Print out the results variables
                convTotal = ((searchResultCounter > 100) ? 100 : searchResultCounter);
                counter = 0;
                while (counter < convTotal) {
                    //Scroll feature right here, in two lines =3
                    if ((signed int) (counter) < (signed int) (((extSelected[0] - 2) - 7) -
                                                               ((((signed int) (extSelected[0] - 2) + 7) -
                                                                 ((signed int) convTotal)) > 0
                                                                        ? abs(
                                                                                  ((signed int) (extSelected[0] - 2) + 7) -
                                                                                  ((signed int) convTotal))
                                                                        : 0))) {
                        counter++;
                        continue;
                    }
                    if ((signed int) (counter) > (signed int) (((extSelected[0] - 2) + 7) +
                                                               (((signed int) (extSelected[0] - 2) - 7) < 0 ? abs(
                                                                                                                      (signed int) ((extSelected[0] - 2) - 7))
                                                                                                            : 0))) {
                        counter++;
                        continue;
                    }

                    //Apply the row color
                    if (counter == (extSelected[0] - 2)) {
                        pspDebugScreenSetTextColor(0xFF8080BB);
                    } else {
                        pspDebugScreenSetTextColor(0xFFBBBBBB);
                    }

                    //Print out the address
                    sprintf(buffer, "  0x%08lX  '", (searchAddress[counter] - 0x48800000));
                    pspDebugScreenPuts(buffer);

                    //Print out the ASCII
                    memset(buffer, 0, 17);
                    scounter = 0;
                    while (scounter < 16) {
                        if ((searchAddress[counter] + scounter) < END_ADDRESS) {
                            buffer[scounter] = *((unsigned char *) (searchAddress[counter] + scounter));
                            if ((buffer[scounter] <= 0x20) || (buffer[scounter] == 0xFF)) buffer[scounter] = '.';
                        } else {
                            break;
                        }
                        scounter++;
                    }
                    pspDebugScreenPuts(buffer);

                    //Goto the next cheat down under
                    pspDebugScreenPuts("'\n");
                    counter++;
                }

                //Helper
                pspDebugScreenSetXY(0, 33);
                pspDebugScreenSetTextColor(0xFFFF8000);
                if (extSelected[0] == 0) {
                    pspDebugScreenPuts(">< = Edit On/Off; \\|/ /|\\ <- -> = Cycle; [] = Trim; () = Cancel    ");
                } else if (extSelected[0] == 1) {
                    pspDebugScreenPuts(">< = Select; () = Cancel                                           ");
                } else {
                    pspDebugScreenPuts("<- -> = Scroll Address; () = Cancel                                ");
                }
                break;
        }
    } else {
        //Draw the tabs
        //Used for each tabs menu. make sure if moving tabs around here
        // the tabSelected is updated to reflect the new tab order
        pspDebugScreenSetTextColor(tabSelected == 0 ? 0xFFFFFFFF : 0xFF808080);
        pspDebugScreenPuts("[Cheats List] ");
        pspDebugScreenSetTextColor(tabSelected == 1 ? 0xFFFFFFFF : 0xFF808080);
        pspDebugScreenPuts("[Searcher] ");
        pspDebugScreenSetTextColor(tabSelected == 2 ? 0xFFFFFFFF : 0xFF808080);//was 3
        pspDebugScreenPuts("[Mem View] ");
        pspDebugScreenSetTextColor(tabSelected == 3 ? 0xFFFFFFFF : 0xFF808080);//was 4
        pspDebugScreenPuts("[Decoder] ");
        pspDebugScreenSetTextColor(tabSelected == 4 ? 0xFFFFFFFF : 0xFF808080);//was 5
        pspDebugScreenPuts("[Tools] ");
        pspDebugScreenSetTextColor(tabSelected == 5 ? 0xFFFFFFFF : 0xFF808080);//was 2
        pspDebugScreenPuts("[Settings] ");
        pspDebugScreenSetTextColor(0xFF808080);
        pspDebugScreenPuts("\n--------------------------------------------------------------------");

        //Draw the options for the respective tab
        switch (tabSelected) {
            case 0://CHEATS LIST
                counter = 0;
                while (counter < cheatTotal) {
                    //Scroll feature right here, in two lines =3
                    if ((int) counter < (int) ((cheatSelected - 11) -
                                               ((((int) cheatSelected + 11) -
                                                 ((int) cheatTotal)) > 0
                                                        ? abs(
                                                                  ((int) cheatSelected + 11) -
                                                                  ((int) cheatTotal))
                                                        : 0))) {
                        counter++;
                        continue;
                    }
                    if ((int) counter > (int) ((cheatSelected + 11) +
                                               (((int) cheatSelected - 11) < 0 ? abs(
                                                                                         (signed int) (cheatSelected - 11))
                                                                               : 0))) {
                        counter++;
                        continue;
                    }

                    pspDebugScreenPuts("  ");
                    if (cheatSelected == counter) {
                        //Highlight the selection
                        if (cheat[cheatSelected].flags & FLAG_SELECTED) {
                            pspDebugScreenSetTextColor(0xFF0000FF);
                        } else if (cheat[cheatSelected].flags & FLAG_CONSTANT) {
                            pspDebugScreenSetTextColor(0xFFFF40FF);
                        } else {
                            pspDebugScreenSetTextColor(0xFF000080);
                        }
                    } else {
                        //Don't highlight the selection
                        if (cheat[counter].flags & FLAG_SELECTED) {
                            pspDebugScreenSetTextColor(0xFFFFFFFF);
                        } else if (cheat[counter].flags & FLAG_CONSTANT) {
                            pspDebugScreenSetTextColor(0xFFFF0000);
                        } else {
                            pspDebugScreenSetTextColor(0xFF808080);
                        }
                    }
                    pspDebugScreenPuts(cheat[counter].name);
                    pspDebugScreenPuts("\n");
                    counter++;
                }

                //Helper
                pspDebugScreenSetTextColor(0xFFFF8000);
                pspDebugScreenSetXY(0, 31);
                pspDebugScreenPuts(">< = Select Cheat for On/Off mode;");
                pspDebugScreenSetXY(0, 32);
                pspDebugScreenPuts("[] = Select Cheat for Always On mode; /\\ = Edit Cheat;");
                pspDebugScreenSetXY(0, 33);
                pspDebugScreenPuts("() = Cancel/Return to Game");
                break;

            case 1://DRAW SEARCHER
                counter = 0;
                while (counter < (3 + ((!cheatSearch) * 2))) {
                    //Scroll feature right here, in two lines =3
                    if ((int) counter < (int) ((cheatSelected - 12) - ((((int) cheatSelected + 12) - ((int) cheatTotal)) > 0 ? abs(((int) cheatSelected + 12) - ((int) cheatTotal)) : 0))) {
                        counter++;
                        continue;
                    }
                    if ((int) counter > (int) ((cheatSelected + 12) + (((int) cheatSelected - 12) < 0 ? abs((int) (cheatSelected - 12)) : 0))) {
                        counter++;
                        continue;
                    }

                    if (cheatSelected == counter) {
                        //Highlight the selection
                        pspDebugScreenSetTextColor(0xFF0000FF);
                    } else {
                        //Don't highlight the selection
                        pspDebugScreenSetTextColor(0xFF808080);
                    }
                    if (!cheatSearch) {
                        switch (counter) {
                            case 0:
                                pspDebugScreenPuts("  Find Exact Value\n");
                                break;
                            case 1:
                                pspDebugScreenPuts("  Find Unknown Value - 8bit\n");
                                break;
                            case 2:
                                pspDebugScreenPuts("  Find Unknown Value - 16bit\n");
                                break;
                            case 3:
                                pspDebugScreenPuts("  Find Unknown Value - 32bit\n");
                                break;
                            case 4:
                                pspDebugScreenPuts("  Find Text\n");
                                break;
                        }
                    } else {
                        switch (counter) {
                            case 0:
                                pspDebugScreenPuts("  Continue to find Exact Value\n");
                                break;
                            case 1:
                                pspDebugScreenPuts("  Continue to find Unknown Value\n");
                                break;
                            case 2:
                                pspDebugScreenPuts("  Reset search\n");
                                break;
                        }
                    }
                    counter++;
                }

                //Print out search history
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("\n[Search History]");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("\n--------------------------------------------------------------------");
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("  Mode          Value.Hex   Value.Dec   Value.ASCII  Value.Float\n");
                scounter = 0;
                while (scounter < searchHistoryCounter) {
                    //Apply the row color
                    pspDebugScreenSetTextColor(0xFF8080BB - (scounter * 0x00080808));

                    //Print out the mode
                    pspDebugScreenPuts(searchModeName[searchHistory[scounter + 1].stdVal]);
                    if (searchHistory[scounter + 1].stdVal > 3) {
                        //Print out the hex
                        switch (searchHistory[scounter + 1].flags & FLAG_DWORD) {
                            case FLAG_DWORD:
                                sprintf(buffer, "  0x%08lX  ", searchHistory[scounter + 1].hakVal);
                                break;
                            case FLAG_WORD:
                                sprintf(buffer, "  0x____%04hX  ", (unsigned short) searchHistory[scounter + 1].hakVal);
                                break;
                            case FLAG_BYTE:
                                sprintf(buffer, "  0x______%02hX  ",
                                        (unsigned char) searchHistory[scounter + 1].hakVal);
                                break;
                        }
                        pspDebugScreenPuts(buffer);

                        //Print out the decimal
                        switch (searchHistory[scounter + 1].flags & FLAG_DWORD) {
                            case FLAG_DWORD:
                                sprintf(buffer, "%010lu  ", searchHistory[scounter + 1].hakVal);
                                break;
                            case FLAG_WORD:
                                sprintf(buffer, "%010lu  ", (unsigned short) searchHistory[scounter + 1].hakVal);
                                break;
                            case FLAG_BYTE:
                                sprintf(buffer, "%010lu  ", (unsigned char) searchHistory[scounter + 1].hakVal);
                                break;
                        }
                        pspDebugScreenPuts(buffer);

                        //Print out the ASCII
                        buffer[0] = *((unsigned char *) (((unsigned int) &searchHistory[scounter + 1].hakVal) + 0));
                        if ((buffer[0] <= 0x20) || (buffer[0] == 0xFF)) buffer[0] = '.';
                        buffer[1] = *((unsigned char *) (((unsigned int) &searchHistory[scounter + 1].hakVal) + 1));
                        if ((buffer[1] <= 0x20) || (buffer[1] == 0xFF)) buffer[1] = '.';
                        buffer[2] = *((unsigned char *) (((unsigned int) &searchHistory[scounter + 1].hakVal) + 2));
                        if ((buffer[2] <= 0x20) || (buffer[2] == 0xFF)) buffer[2] = '.';
                        buffer[3] = *((unsigned char *) (((unsigned int) &searchHistory[scounter + 1].hakVal) + 3));
                        if ((buffer[3] <= 0x20) || (buffer[3] == 0xFF)) buffer[3] = '.';
                        buffer[4] = 0;
                        pspDebugScreenPuts(buffer);

                        //Print out the float
                        pspDebugScreenPuts("         ");
                        f_cvt(&searchHistory[scounter + 1].hakVal, buffer, sizeof(buffer), 6, MODE_GENERIC);
                        pspDebugScreenPuts(buffer);
                    }

                    //Goto the next line
                    pspDebugScreenPuts("\n");

                    //Increment scounter
                    scounter++;
                }

                //Helper
                pspDebugScreenSetTextColor(0xFFFF8000);
                pspDebugScreenSetXY(0, 33);
                pspDebugScreenPuts(">< = Select; () = Cancel/Return to Game");
                break;


            case 2://DRAW BROWSER
                pspDebugScreenSetTextColor(0xFF808080);
                if (browseLines == 8) {
                    pspDebugScreenPuts("  Address     00  01  02  03  04  05  06  07    ASCII\n");
                } else {
                    pspDebugScreenPuts("  Address     000102030405060708090A0B0C0D0E0F  ASCII\n");
                }

                //Write out the RAM
                counter = 0;
                while (counter < 22) {
                    //Apply the row color
                    if (counter == browseY) {
                        pspDebugScreenSetTextColor(0xFF8080BB);
                    } else {
                        pspDebugScreenSetTextColor(0xFFBBBBBB);
                    }

                    //Print out the address
                    sprintf(buffer, "  0x%08lX  ", (browseAddress + (counter * browseLines)) - browseFormat);
                    pspDebugScreenPuts(buffer);

                    //Print out the bytes per line
                    scounter = 0;
                    while (scounter < browseLines) {
                        //Apply the row color
                        if ((scounter & 1) || (browseY == counter)) {
                            pspDebugScreenSetTextColor(0xFF8080BB);
                        } else {
                            pspDebugScreenSetTextColor(0xFFBBBBBB);
                        }

                        sprintf(buffer, "%02hX",
                                *((unsigned char *) ((browseAddress + (counter * browseLines)) + scounter)));
                        pspDebugScreenPuts(buffer);
                        if (browseLines == 8) pspDebugScreenPuts("  ");

                        buffer[3 + scounter] = *((unsigned char *) ((browseAddress + (counter * browseLines)) +
                                                                    scounter));
                        if ((buffer[3 + scounter] <= 0x20) || (buffer[3 + scounter] == 0xFF))
                            buffer[3 + scounter] = '.';

                        scounter++;
                    }

                    //Apply the row color
                    if (counter == browseY) {
                        pspDebugScreenSetTextColor(0xFF8080BB);
                    } else {
                        pspDebugScreenSetTextColor(0xFFBBBBBB);
                    }

                    //Print out the ASCII
                    buffer[3 + browseLines] = 0;
                    pspDebugScreenPuts("  ");
                    pspDebugScreenPuts(&buffer[3]);

                    //Skip a line, draw the pointer =)
                    if (counter == browseY) {
                        //Skip the initial line
                        pspDebugScreenPuts("\n");

                        //Skip the desired amount?
                        pspDebugScreenPuts("    ");
                        if (browseC != 0)//Skip Hex
                        {
                            pspDebugScreenPuts("          ");
                            if (browseC != 1)//Skip Bytes
                            {
                                pspDebugScreenPuts("                                  ");
                                //Skip ASCII
                            }
                        }

                        //Skip the minimalist amount
                        unsigned char tempCounter = browseX;
                        while (tempCounter) {
                            if ((tempCounter != 0) && ((tempCounter % 2) == 0) && (browseLines == 8) &&
                                (browseC == 1)) {
                                pspDebugScreenPuts("   ");
                            } else {
                                pspDebugScreenPuts(" ");
                            }
                            tempCounter--;
                        }

                        //Draw the symbol (Finally!!)
                        if (extSelected[3]) {
                            pspDebugScreenSetTextColor(0xFF0000FF);
                        } else {
                            pspDebugScreenSetTextColor(0xFFFF0000);
                        }
                        pspDebugScreenPuts("^");
                    }

                    //Goto the next cheat down under
                    pspDebugScreenPuts("\n");
                    counter++;
                }

                //Helper
                pspDebugScreenSetTextColor(0xFFFF8000);
                pspDebugScreenSetXY(0, 32);
                pspDebugScreenPuts(">< = Edit On/Off; \\|/ /|\\ <- -> = Cycle/Scroll; [] = Teleport Cursor");
                pspDebugScreenSetXY(0, 33);
                pspDebugScreenPuts("[] + Analog/Digital = Scroll; () = Cancel/Return to Game");
                break;

            case 3://DRAW DECODER
                pspDebugScreenSetTextColor(0xFF808080);
                pspDebugScreenPuts("  Address     Hex       Opcode   Args\n");

                //Write out the RAM
                counter = 0;
                while (counter < 22) {
                    //Apply the row color
                    if (counter == decodeY) {
                        pspDebugScreenSetTextColor(0xFF8080BB);
                    } else {
                        pspDebugScreenSetTextColor(0xFFBBBBBB);
                    }

                    //Print out the address
                    sprintf(buffer, "  0x%08lX  ", (decodeAddress + (counter * 4)) - decodeFormat);
                    pspDebugScreenPuts(buffer);

                    //Print out the dword of memory
                    sprintf(buffer, "%08lX  ", *((unsigned int *) (decodeAddress + (counter * 4))));
                    pspDebugScreenPuts(buffer);

                    //Print out the opcode
                    mipsDecode(*((unsigned int *) (decodeAddress + (counter * 4))));
                    pspDebugScreenPuts(buffer);

                    //Skip a line, draw the pointer =)
                    if (counter == decodeY) {
                        //Skip the initial line
                        pspDebugScreenPuts("\n");

                        //Skip the desired amount?
                        pspDebugScreenPuts("    ");
                        if (decodeC != 0)//Skip Address
                        {
                            pspDebugScreenPuts("          ");
                            //Skip Hex
                        }

                        //Skip the minimalist amount
                        unsigned char tempCounter = decodeX;
                        while (tempCounter) {
                            pspDebugScreenPuts(" ");
                            tempCounter--;
                        }

                        //Draw the symbol (Finally!!)
                        if (extSelected[3]) {
                            pspDebugScreenSetTextColor(0xFF0000FF);
                        } else {
                            pspDebugScreenSetTextColor(0xFFFF0000);
                        }
                        pspDebugScreenPuts("^");
                    }

                    //Goto the next cheat down under
                    pspDebugScreenPuts("\n");
                    counter++;
                }

                //Helper
                pspDebugScreenSetTextColor(0xFFFF8000);
                pspDebugScreenSetXY(0, 32);
                pspDebugScreenPuts(">< = Edit On/Off; \\|/ /|\\ <- -> = Cycle/Scroll;");
                pspDebugScreenSetXY(0, 33);
                pspDebugScreenPuts("[] + Analog/Digital = Scroll; () = Cancel/Return to Game");
                break;

            case 4://Draw Tools
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("\nComing Soon!");

                //Helper
                lineClear(33);
                pspDebugScreenSetTextColor(0xFFFF8000);
                if (trackY == 0) {
                    pspDebugScreenPuts(">< = Edit On/Off; \\|/ /|\\ <- -> = Cycle; () = Cancel");
                } else if (trackY == 3) {
                    pspDebugScreenPuts(">< = Select; () = Cancel");
                } else {
                    pspDebugScreenPuts(">< = Toggle; () = Cancel");
                }
                break;

            case 5://DRAW SETTINGS
                counter = 0;
                while (counter < 12) {
                    if (cheatSelected == counter) {
                        //Highlight the selection
                        pspDebugScreenSetTextColor(0xFF0000FF);
                    } else {
                        //Don't highlight the selection
                        pspDebugScreenSetTextColor(0xFF808080);
                    }
                    switch (counter) {
                        case 0:
                            pspDebugScreenPuts("  Pause Game? ");
                            if (cheatPause) {
                                pspDebugScreenPuts("True\n");
                            } else {
                                pspDebugScreenPuts("False\n");
                            }
                            break;
                        case 1:
                            pspDebugScreenPuts("  Add Empty Cheat?\n");
                            break;
                        case 2:
                            sprintf(buffer, "  Reset Codes? Slot #%d\n", dumpNo);
                            pspDebugScreenPuts(buffer);
                            break;
                        case 3:
                            sprintf(buffer, "  Dump RAM? Slot #%d\n", dumpNo);
                            pspDebugScreenPuts(buffer);
                            break;
                        case 4:
                            sprintf(buffer, "  Bytes per Line in Browser? %d\n", browseLines);
                            pspDebugScreenPuts(buffer);
                            break;
                        case 5:
                            pspDebugScreenPuts("  Real Addressing in Browser? ");
                            if (browseFormat == 0x40000000) {
                                pspDebugScreenPuts("True\n");
                            } else {
                                pspDebugScreenPuts("False\n");
                            }
                            break;
                        case 6:
                            pspDebugScreenPuts("  Real Addressing in Decoder? ");
                            if (decodeFormat == 0x40000000) {
                                pspDebugScreenPuts("True\n");
                            } else {
                                pspDebugScreenPuts("False\n");
                            }
                            break;
                        case 7:
                            pspDebugScreenPuts("  Real Addressing in Logger? ");
                            if (trackFormat == 0x40000000) {
                                pspDebugScreenPuts("True\n");
                            } else {
                                pspDebugScreenPuts("False\n");
                            }
                            break;
                        case 8:
                            sprintf(buffer, "  Cheat Hz? %d/1000 seconds\n", (cheatHz / 1000));
                            pspDebugScreenPuts(buffer);
                            break;
                        case 9:
                            pspDebugScreenPuts("  Save cheats\n");
                            break;

                        case 10:
                            pspDebugScreenPuts("  Enable USB?  ");
                            if (sceUsbGetState() & PSP_USB_ACTIVATED) {
                                pspDebugScreenPuts("True\n");
                            } else {
                                pspDebugScreenPuts("False\n");
                            }
                            break;

                        case 11:
                            pspDebugScreenPuts("  Restart Game\n");
                            break;
                    }
                    counter++;
                }
                //Helper
                pspDebugScreenSetTextColor(0xFFFF8000);
                lineClear(31);
                lineClear(32);
                switch (cheatSelected) {
                    case 0:
                        pspDebugScreenPuts("Pauses the game while cheatER's menu is showing");
                        break;
                    case 1:
                        pspDebugScreenPuts("Adds an empty cheat to the Cheater for you to edit");
                        break;
                    case 2:
                        pspDebugScreenPuts("Uses the selected 'RAM dump' to regenerate OFF codes");
                        break;
                    case 3:
                        pspDebugScreenPuts("Saves the Game's RAM to MemoryStick");
                        break;
                    case 4:
                        pspDebugScreenPuts("Alters the number of bytes displayed in the Browser");
                        break;
                    case 5:
                        pspDebugScreenPuts("If enabled, REAL PSP hardware addresses will be used in Browser");
                        break;
                    case 6:
                        pspDebugScreenPuts("If enabled, REAL PSP hardware addresses will be used in Decoder");
                        break;
                    case 7:
                        pspDebugScreenPuts("If enabled, REAL PSP hardware addresses will be used in Logger");
                        break;
                    case 8:
                        lineClear(31);
                        pspDebugScreenPuts("cheatER only applies a cheat once - set this to anything other");
                        lineClear(32);
                        pspDebugScreenPuts("than 0 to make it apply the same cheat every X amount of seconds");
                        break;
                    case 9:
                        pspDebugScreenPuts("Save your cheats");
                        break;
                    case 10:
                        pspDebugScreenPuts("Enable USB");
                        break;

                    case 11:
                        pspDebugScreenPuts("Restart the Game");
                        break;
                }
                lineClear(33);
                if ((cheatSelected != 8) && (cheatSelected != 3) && (cheatSelected != 2)) {
                    pspDebugScreenPuts(">< = Toggle; () = Cancel/Return to Game");
                } else {
                    pspDebugScreenPuts("<- -> = Decrement/Increment; () = Cancel/Return to Game");
                }
                break;
        }
    }

    if (copyMenu) {
        pspDebugScreenSetXY(25, 15);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c", 0xC9);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFFFF0000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3,
                0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c", 0xC6);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetXY(25, 16);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c", 0xC9);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFFFF0000);
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        sprintf(buffer, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 0x20, 0x20, 0x20, '-', 0x20, 'C', 'o', 'p', 'i', 'e', 'r',
                0x20, '-', 0x20, 0x20, 0x20);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c", 0xC6);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetXY(25, 17);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c", 0xC9);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFFFF0000);
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        sprintf(buffer, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                0x20, 0x20, 0x20, 0x20, 0x20, 0x20);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c", 0xC6);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetXY(25, 18);
        sprintf(buffer, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 0xC9, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3,
                0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC6);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetXY(25, 19);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c%c", 0xC9, 0x20);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        if (copyMenu == 1) {
            pspDebugScreenSetTextColor(0xFF0000FF);
            pspDebugScreenPuts(">");
        } else {
            pspDebugScreenPuts(" ");
        }
        pspDebugScreenSetBackColor(0xFF000000);
        sprintf(buffer, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 'C', 'o', 'p', 'y', 0x20, 'A', 'd', 'd', 'r', 'e', 's', 's',
                0x20, 0x20);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c", 0xC6);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetXY(25, 20);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c%c", 0xC9, 0x20);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        if (copyMenu == 2) {
            pspDebugScreenSetTextColor(0xFF0000FF);
            pspDebugScreenPuts(">");
        } else {
            pspDebugScreenPuts(" ");
        }
        pspDebugScreenSetBackColor(0xFF000000);
        sprintf(buffer, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 'P', 'a', 's', 't', 'e', 0x20, 'A', 'd', 'd', 'r', 'e', 's',
                's', 0x20);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
        sprintf(buffer, "%c", 0xC6);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetXY(25, 21);
        sprintf(buffer, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 0xC9, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xC6);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetXY(25, 22);
        sprintf(buffer, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 0x20, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3,
                0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0x20);
        pspDebugScreenPuts(buffer);
        pspDebugScreenSetBackColor(0xFF000000);
        pspDebugScreenSetTextColor(0xFFFF8000);
    }

    //Take a picture!!!
    /*if(pad.Buttons & PSP_CTRL_START)
  {
    sprintf(buffer, "ms0:/vram%d.raw", dumpNo);

              int fd;
              fd=sceIoOpen(buffer, PSP_O_WRONLY | PSP_O_CREAT, 0777);
              if(fd>0)
              {
  	  					sceIoWrite(fd, vram, 512*272*2);
  	  					sceIoClose(fd);
              }

    dumpNo++;
  }*/
}

int LoadStartModule(char *path) {
    u32 loadResult;
    u32 startResult;
    int status;

    loadResult = sceKernelLoadModule(path, 0, NULL);
    if (loadResult & 0x80000000)
        return -1;
    else
        startResult =
                sceKernelStartModule(loadResult, 0, NULL, &status, NULL);

    if (loadResult != startResult)
        return -2;

    return 0;
}

void start_usb() {
    LoadStartModule("flash0:/kd/semawm.prx");
    LoadStartModule("flash0:/kd/usbstor.prx");
    LoadStartModule("flash0:/kd/usbstormgr.prx");
    LoadStartModule("flash0:/kd/usbstorms.prx");
    LoadStartModule("flash0:/kd/usbstorboot.prx");
    sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
    sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0);
    sceUsbstorBootSetCapacity(0x800000);
    sceUsbActivate(0x1c8);
}

void stop_usb() {
    sceUsbDeactivate(0x1c8);
    sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);
    sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
    sceIoDevctl("fatms0:", 0x0240D81E, NULL, 0, NULL, 0);
}


void restartGame(void)
{
    struct SceKernelLoadExecParam execParam;

    char* currentGamePath = getGamePathFromID(gameId);

    execParam.size = sizeof(execParam);
    execParam.argp = currentGamePath;
    execParam.args = strlen(currentGamePath);
    execParam.key = NULL;

    sceKernelLoadExec(currentGamePath, &execParam);
}

void menuInput() {
    int fd;
    int fd2;
    unsigned int counter = 0;
    unsigned int scounter = 0;
    unsigned int dcounter = 0;
    unsigned int padButtons;
    unsigned char miscType = 0;
    pad.Buttons = 0;
    u32 retVal;
    menuDraw();

    //Loop for input
    while (1) {
        padButtons = pad.Buttons;
        sceCtrlPeekBufferPositive(&pad, 1);

        //Has the HOME button screwed up the VRAM blocks?
        unsigned int a_address = 0;
        unsigned int a_bufferWidth = 0;
        unsigned int a_pixelFormat = 0;
        unsigned int a_sync;

        sceDisplayGetFrameBufferInternal(0, &a_address, &a_bufferWidth, &a_pixelFormat, &a_sync);

        if (a_address == 0) {
            //Stop cheatER
            menuDrawn = 0;
            return;
        }

        //The 1st priority menu
        if (copyMenu) {
            if (pad.Buttons & PSP_CTRL_UP) {
                if (copyMenu != 1) copyMenu = 1;
                menuDraw();
                sceKernelDelayThread(150000);
            } else if (pad.Buttons & PSP_CTRL_DOWN) {
                if (copyMenu != 2) copyMenu = 2;
                menuDraw();
                sceKernelDelayThread(150000);
            }
            if (pad.Buttons & PSP_CTRL_CROSS) {
                if (copyMenu == 1)//Copy
                {
                    if (extMenu) {
                        if (extMenu == 1) {
                            copyData = block[extSelected[0]].address;
                        } else if (extMenu == 2) {
                            if (extSelected[0] > 2) {
                                copyData = searchAddress[extSelected[0] - 3] - 0x40000000;
                            }
                        } else if (extMenu == 3) {
                            if (extSelected[0] > 2) {
                                copyData = searchAddress[extSelected[0] - 3] - 0x40000000;
                            }
                        } else if (extMenu == 4) {
                            if (extSelected[0] > 1) {
                                copyData = searchAddress[extSelected[0] - 2] - 0x40000000;
                            }
                        }
                    } else {
                        if (tabSelected == 3) {
                            copyData = browseAddress + (browseY * browseLines);
                            copyData -= 0x40000000;
                        } else if (tabSelected == 4) {
                            copyData = decodeAddress + (decodeY * 4);
                            copyData -= 0x40000000;
                        } else if (tabSelected == 5) {
                            copyData = trackAddress;
                            copyData -= 0x40000000;
                        }
                    }
                    copyData &= 0xFFFFFFFC;

                    if (copyData < 0x08804000) {
                        copyData = 0x08804000;
                    }
                } else//Paste
                {
                    if (extMenu) {
                        if (extMenu == 1) {
                            if (!(block[extSelected[0]].flags & FLAG_DMA)) block[extSelected[0]].address = copyData;
                        }
                    } else {
                        if (tabSelected == 3) {
                            browseAddress = copyData | 0x40000000;//(browseY * browseLines)+
                            if (browseAddress > (END_ADDRESS - (22 * browseLines))) {
                                browseAddress = (END_ADDRESS - (22 * browseLines));
                            }
                        } else if (tabSelected == 4) {
                            decodeAddress = copyData | 0x40000000;//+(decodeY*4);
                            if (decodeAddress > 0x49FFFFA8) {
                                decodeAddress = 0x49FFFFA8;
                            }
                        } else if (tabSelected == 5) {
                            trackAddress = copyData + 0x40000000;
                        }
                    }
                }
                goto hideCopyMenu;
            }
            if ((padButtons & PSP_CTRL_CIRCLE) && !(pad.Buttons & PSP_CTRL_CIRCLE)) {
            hideCopyMenu:
                pspDebugScreenInitEx(vram, 0, 0);
                copyMenu = 0;
                menuDraw();
                sceKernelDelayThread(150000);
            }
        } else if (extMenu)//Do we use extended menus?
        {
            switch (extMenu) {
                case 1://INPUT EXT CHEAT
                    if (pad.Buttons & PSP_CTRL_SELECT) {
                        copyMenu = 1;
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }
                    if (pad.Buttons & PSP_CTRL_CROSS) {
                        extSelected[3] = !extSelected[3];
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }
                    if (pad.Buttons & PSP_CTRL_SQUARE) {
                        //Don't allow the user to cause a dword/byte misalignment!!!
                        if (!(block[extSelected[0]].address % 4)) {
                            switch (block[extSelected[0]].flags & FLAG_DWORD) {
                                case FLAG_BYTE:
                                    block[extSelected[0]].flags =
                                            (block[extSelected[0]].flags & ~FLAG_DWORD) | FLAG_WORD;
                                    break;
                                case FLAG_WORD:
                                    block[extSelected[0]].flags =
                                            (block[extSelected[0]].flags & ~FLAG_DWORD) | FLAG_DWORD;
                                    break;
                                case FLAG_DWORD:
                                    block[extSelected[0]].flags =
                                            (block[extSelected[0]].flags & ~FLAG_DWORD) | FLAG_BYTE;
                                    break;
                            }
                        }
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }

                    if (extSelected[3]) {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            switch (extSelected[1]) {
                                case 0:
                                    if (block[extSelected[0]].flags & FLAG_DMA) {
                                        //Do nothing =o
                                        break;
                                    } else if (((block[extSelected[0]].flags & FLAG_DWORD) == FLAG_DWORD) &&
                                               (extSelected[2] == 7))//Prevent user from misaligned dwords/bytes
                                    {
                                        block[extSelected[0]].address += 4;
                                    } else if (((block[extSelected[0]].flags & FLAG_DWORD) == FLAG_WORD) &&
                                               (extSelected[2] == 7))//Prevent user from misaligned dwords/bytes
                                    {
                                        block[extSelected[0]].address += 2;
                                    } else {
                                        block[extSelected[0]].address += (1 << (4 * (7 - extSelected[2])));
                                    }
                                    if (block[extSelected[0]].address < 0x08800000) {
                                        block[extSelected[0]].address = 0x08800000;
                                    }
                                    if (block[extSelected[0]].address > 0x09FFFFFF) {
                                        block[extSelected[0]].address = 0x09FFFFFC;
                                    }
                                    if (cheatSaved)//Re-Update the stdVal
                                    {
                                        switch (block[extSelected[0]].flags & FLAG_DWORD) {
                                            case FLAG_BYTE:
                                                block[extSelected[0]].stdVal = *((unsigned char *) (block[extSelected[0]].address));
                                                break;
                                            case FLAG_WORD:
                                                block[extSelected[0]].stdVal = *((unsigned short *) (block[extSelected[0]].address));
                                                break;
                                            case FLAG_DWORD:
                                                block[extSelected[0]].stdVal = *((unsigned int *) (block[extSelected[0]].address));
                                                break;
                                            default:
                                                block[blockTotal].flags |= FLAG_UWORD;
                                        }
                                    }
                                    break;
                                case 1:
                                    if (block[extSelected[0]].flags & FLAG_FREEZE) {
                                        //Do nothing =o
                                        break;
                                    }
                                    block[extSelected[0]].hakVal += (1 << (4 * (7 - extSelected[2])));
                                    break;
                                case 2:
                                    block[extSelected[0]].hakVal += decDelta[extSelected[2]];
                                    break;
                                case 3:
                                    block[extSelected[0]].hakVal += (1 << (8 * (extSelected[2])));
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            switch (extSelected[1]) {
                                case 0:
                                    if (block[extSelected[0]].flags & FLAG_DMA) {
                                        //Do nothing =o
                                        break;
                                    } else if (((block[extSelected[0]].flags & FLAG_DWORD) == FLAG_DWORD) &&
                                               (extSelected[2] == 7))//Prevent user from misaligned dwords/bytes
                                    {
                                        block[extSelected[0]].address -= 4;
                                    } else if (((block[extSelected[0]].flags & FLAG_DWORD) == FLAG_WORD) &&
                                               (extSelected[2] == 7))//Prevent user from misaligned dwords/bytes
                                    {
                                        block[extSelected[0]].address -= 2;
                                    } else {
                                        block[extSelected[0]].address -= (1 << (4 * (7 - extSelected[2])));
                                    }
                                    if (block[extSelected[0]].address < 0x08800000) {
                                        block[extSelected[0]].address = 0x08800000;
                                    }
                                    if (block[extSelected[0]].address > 0x09FFFFFF) {
                                        block[extSelected[0]].address = 0x09FFFFFC;
                                    }
                                    if (cheatSaved)//Re-Update the stdVal
                                    {
                                        switch (block[extSelected[0]].flags & FLAG_DWORD) {
                                            case FLAG_BYTE:
                                                block[extSelected[0]].stdVal = *((unsigned char *) (block[extSelected[0]].address));
                                                break;
                                            case FLAG_WORD:
                                                block[extSelected[0]].stdVal = *((unsigned short *) (block[extSelected[0]].address));
                                                break;
                                            case FLAG_DWORD:
                                                block[extSelected[0]].stdVal = *((unsigned int *) (block[extSelected[0]].address));
                                                break;
                                            default:
                                                block[blockTotal].flags |= FLAG_UWORD;
                                        }
                                    }
                                    break;
                                case 1:
                                    if (block[extSelected[0]].flags & FLAG_FREEZE) {
                                        //Do nothing =o
                                        break;
                                    }
                                    block[extSelected[0]].hakVal -= (1 << (4 * (7 - extSelected[2])));
                                    break;
                                case 2:
                                    block[extSelected[0]].hakVal -= decDelta[extSelected[2]];
                                    break;
                                case 3:
                                    block[extSelected[0]].hakVal -= (1 << (8 * (extSelected[2])));
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    } else {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            if (extSelected[0] > cheat[cheatSelected].block) {
                                extSelected[0]--;
                            } else if (extSelected[0] == cheat[cheatSelected].block) {
                                extSelected[0] = (cheat[cheatSelected].block + cheat[cheatSelected].len) - 1;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            if (extSelected[0] < ((cheat[cheatSelected].block + cheat[cheatSelected].len) - 1)) {
                                extSelected[0]++;
                            } else if (extSelected[0] ==
                                       ((cheat[cheatSelected].block + cheat[cheatSelected].len) - 1)) {
                                extSelected[0] = cheat[cheatSelected].block;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    }

                    if (pad.Buttons & PSP_CTRL_LEFT) {
                        extSelected[2]--;
                        switch (extSelected[1]) {
                            case 0:
                                if ((signed) extSelected[2] == -1) { extSelected[2] = 0; }
                                break;
                            case 1:
                                if ((signed) extSelected[2] == -1) {
                                    extSelected[2] = 7;
                                    extSelected[1]--;
                                }
                                break;
                            case 2:
                                if ((signed) extSelected[2] == -1) {
                                    extSelected[2] = 7;
                                    extSelected[1]--;
                                }
                                break;
                            case 3:
                                if ((signed) extSelected[2] == -1) {
                                    extSelected[2] = 9;
                                    extSelected[1]--;
                                }
                                break;
                        }
                        menuDraw();
                        if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                    } else if (pad.Buttons & PSP_CTRL_RIGHT) {
                        extSelected[2]++;
                        switch (extSelected[1]) {
                            case 0:
                                if (extSelected[2] > 7) {
                                    extSelected[2] = 0;
                                    extSelected[1]++;
                                }
                                break;
                            case 1:
                                if (extSelected[2] > 7) {
                                    extSelected[2] = 0;
                                    extSelected[1]++;
                                }
                                break;
                            case 2:
                                if (extSelected[2] > 9) {
                                    extSelected[2] = 0;
                                    extSelected[1]++;
                                }
                                break;
                            case 3:
                                if (extSelected[2] > 3) { extSelected[2] = 3; }
                                break;
                        }
                        menuDraw();
                        if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                    } else {
                        cheatButtonAgeX = 0;
                    }

                    if ((padButtons & PSP_CTRL_CIRCLE) && !(pad.Buttons & PSP_CTRL_CIRCLE)) {
                        if (extSelected[3]) {
                            extSelected[3] = 0;
                            menuDraw();
                            sceKernelDelayThread(150000);
                        } else {
                            pspDebugScreenInitEx(vram, 0, 0);
                            extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                            extMenu = 0;
                            if (extOpt) {
                                extSelected[0] = extOptArg;
                                pspDebugScreenInitEx(vram, 0, 0);
                                extSelected[1] = extSelected[2] = extSelected[3] = 0;
                                extMenu = 1 + extOpt;
                                cheatSearch = 1;
                            }
                            extOpt = 0;
                            menuDraw();
                            sceKernelDelayThread(150000);
                            break;
                        }
                    }
                    break;

                case 2://INPUT EXT SEARCH
                    if (pad.Buttons & PSP_CTRL_SELECT) {
                        copyMenu = 1;
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }
                    if (pad.Buttons & PSP_CTRL_SQUARE) {
                        if (searchNo == 0)//Don't allow the user to change type later on!!!
                        {
                            switch (searchHistory[0].flags & FLAG_DWORD) {
                                case FLAG_BYTE:
                                    searchHistory[0].flags = (searchHistory[0].flags & ~FLAG_DWORD) | FLAG_WORD;
                                    break;
                                case FLAG_WORD:
                                    searchHistory[0].flags = (searchHistory[0].flags & ~FLAG_DWORD) | FLAG_DWORD;
                                    break;
                                case FLAG_DWORD:
                                    searchHistory[0].flags = (searchHistory[0].flags & ~FLAG_DWORD) | FLAG_BYTE;
                                    break;
                            }
                        }
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }
                    if (pad.Buttons & PSP_CTRL_CROSS) {
                        if (extSelected[0] == 0) {
                            extSelected[3] = !extSelected[3];
                        } else if (extSelected[0] == 1) {
                            //Update the search history!
                            if (searchHistoryCounter < 15) {
                                searchHistoryCounter++;
                            }
                            memmove(&searchHistory[1], &searchHistory[0], sizeof(Block) * (15));
                            searchHistory[1].stdVal = 6;

                            //Move the cursor back
                            extSelected[0] = 0;

                            //Is it the first search?
                            if (searchNo == 0) {
                                //Increment the search
                                searchNo++;
                                searchMax++;

                                //Setup the variables
                                searchResultCounter = 0;

                                //Open the file
                                sprintf(buffer, "ms0:/search%d.dat", searchNo);
                                fd = sceIoOpen(buffer, PSP_O_WRONLY | PSP_O_CREAT, 0777);
                                if (fd > 0) {
                                    //Write out the searcHistory[0] type
                                    switch (searchHistory[0].flags & FLAG_DWORD) {
                                        case FLAG_DWORD:
                                            if (sceIoWrite(fd, "4", 1) != 1) goto ErrorReadExactA;
                                            break;
                                        case FLAG_WORD:
                                            if (sceIoWrite(fd, "2", 1) != 1) goto ErrorReadExactA;
                                            break;
                                        case FLAG_BYTE:
                                            if (sceIoWrite(fd, "1", 1) != 1) goto ErrorReadExactA;
                                            break;
                                    }

                                    //Search!
                                    counter = START_ADDRESS;

                                    //Helper
                                    while (counter < END_ADDRESS) {
                                        //Helper
                                        if (!((counter - START_ADDRESS) & 0xFFFF)) {
                                            if (!cheatPause) sceKernelDelayThread(1500);

                                            lineClear(33);
                                            pspDebugScreenSetTextColor(0xFFFF8000);
                                            sprintf(buffer, "Searching = %02d%%; Hold () to Abort",
                                                    (counter - START_ADDRESS) / ((END_ADDRESS - START_ADDRESS) / 100));
                                            pspDebugScreenPuts(buffer);

                                            sceCtrlPeekBufferPositive(&pad, 1);

                                            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                                                lineClear(33);
                                                pspDebugScreenSetTextColor(0xFFFF8000);
                                                pspDebugScreenPuts("Search Aborted");

                                                do {
                                                    sceKernelDelayThread(150000);
                                                    sceCtrlPeekBufferPositive(&pad, 1);
                                                } while (pad.Buttons & PSP_CTRL_CIRCLE);
                                                break;
                                            }
                                        }

                                        //Check
                                        switch (searchHistory[0].flags & FLAG_DWORD) {
                                            case FLAG_DWORD:
                                                if (*((unsigned int *) (counter)) ==
                                                    (unsigned int) searchHistory[0].hakVal) {
                                                    //Add it
                                                    if (sceIoWrite(fd, &counter, sizeof(unsigned int)) != 4)
                                                        goto ErrorReadExactA;
                                                    if (sceIoWrite(fd, &searchHistory[0].hakVal,
                                                                   sizeof(unsigned int)) != 4)
                                                        goto ErrorReadExactA;
                                                    searchResultCounter++;
                                                }
                                                counter += 4;
                                                break;

                                            case FLAG_WORD:
                                                if (*((unsigned short *) (counter)) ==
                                                    (unsigned short) searchHistory[0].hakVal) {
                                                    //Add it
                                                    if (sceIoWrite(fd, &counter, sizeof(unsigned int)) != 4)
                                                        goto ErrorReadExactA;
                                                    if (sceIoWrite(fd, &searchHistory[0].hakVal,
                                                                   sizeof(unsigned short)) != 2)
                                                        goto ErrorReadExactA;
                                                    searchResultCounter++;
                                                }
                                                counter += 2;
                                                break;

                                            case FLAG_BYTE:
                                                if (*((unsigned char *) (counter)) ==
                                                    (unsigned char) searchHistory[0].hakVal) {
                                                    //Add it
                                                    if (sceIoWrite(fd, &counter, sizeof(unsigned int)) != 4)
                                                        goto ErrorReadExactA;
                                                    if (sceIoWrite(fd, &searchHistory[0].hakVal,
                                                                   sizeof(unsigned char)) != 1)
                                                        goto ErrorReadExactA;
                                                    searchResultCounter++;
                                                }
                                                counter++;
                                                break;
                                        }
                                    }
                                    //Close the file since we are done with the search
                                    sceIoClose(fd);

                                    while (1) {
                                        break;
                                    //ReadShort
                                    ErrorReadExactA:
                                        sceIoClose(fd);
                                        if (searchNo > 0) searchNo--;
                                        sceIoRemove(buffer);

                                        lineClear(33);
                                        pspDebugScreenSetTextColor(0xFF0000FF);
                                        pspDebugScreenPuts("ERROR: MemoryStick out of Space!");
                                        sceKernelDelayThread(3000000);
                                        break;
                                    }
                                } else {
                                    //ERROR - file couldn't be opened - undo search attempt
                                    if (searchNo > 0) searchNo--;
                                }
                            } else//Continue the search with a different exact number
                            {
                                //Increment the search
                                searchNo++;
                                searchMax++;

                                //Open the files
                                sprintf(buffer, "ms0:/search%d.dat", searchNo - 1);
                                fd = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
                                sprintf(buffer, "ms0:/search%d.dat", searchNo);
                                fd2 = sceIoOpen(buffer, PSP_O_WRONLY | PSP_O_CREAT, 0777);
                                if ((fd > 0) && (fd2 > 0)) {
                                    //Get ready to go through and check the addresses
                                    sceIoLseek(fd, 1, SEEK_SET);

                                    //Write out the searcHistory[0] type
                                    switch (searchHistory[0].flags & FLAG_DWORD) {
                                        case FLAG_DWORD:
                                            if (sceIoWrite(fd2, "4", 1) != 1) goto ErrorReadExactB;
                                            break;
                                        case FLAG_WORD:
                                            if (sceIoWrite(fd2, "2", 1) != 1) goto ErrorReadExactB;
                                            break;
                                        case FLAG_BYTE:
                                            if (sceIoWrite(fd2, "1", 1) != 1) goto ErrorReadExactB;
                                            break;
                                    }

                                    //Loop through the list checking each one
                                    counter = searchResultCounter;
                                    searchResultCounter = 0;
                                    while (counter > 0) {
                                        //Load it
                                        sceIoRead(fd, &scounter, sizeof(unsigned int));

                                        //Helper
                                        if (!(counter & 0x3F)) {
                                            if (!cheatPause) sceKernelDelayThread(1500);

                                            lineClear(33);
                                            pspDebugScreenSetTextColor(0xFFFF8000);
                                            sprintf(buffer, "Searching = %02d%%; Hold () to Abort",
                                                    (scounter - START_ADDRESS) / ((END_ADDRESS - START_ADDRESS) / 100));
                                            pspDebugScreenPuts(buffer);

                                            sceCtrlPeekBufferPositive(&pad, 1);

                                            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                                                lineClear(33);
                                                pspDebugScreenSetTextColor(0xFFFF8000);
                                                pspDebugScreenPuts("Search Aborted");

                                                do {
                                                    sceKernelDelayThread(150000);
                                                    sceCtrlPeekBufferPositive(&pad, 1);
                                                } while (pad.Buttons & PSP_CTRL_CIRCLE);
                                                break;
                                            }
                                        }

                                        //Check
                                        switch (searchHistory[0].flags & FLAG_DWORD) {
                                            case FLAG_DWORD:
                                                sceIoLseek(fd, 4, SEEK_CUR);
                                                if (*((unsigned int *) (scounter)) ==
                                                    (unsigned int) searchHistory[0].hakVal) {
                                                    //Add it
                                                    if (sceIoWrite(fd2, &scounter, sizeof(unsigned int)) != 4)
                                                        goto ErrorReadExactB;
                                                    if (sceIoWrite(fd2, &searchHistory[0].hakVal,
                                                                   sizeof(unsigned int)) != 4)
                                                        goto ErrorReadExactB;
                                                    searchResultCounter++;
                                                }
                                                break;

                                            case FLAG_WORD:
                                                sceIoLseek(fd, 2, SEEK_CUR);
                                                if (*((unsigned short *) (scounter)) ==
                                                    (unsigned short) searchHistory[0].hakVal) {
                                                    //Add it
                                                    if (sceIoWrite(fd2, &scounter, sizeof(unsigned int)) != 4)
                                                        goto ErrorReadExactB;
                                                    if (sceIoWrite(fd2, &searchHistory[0].hakVal,
                                                                   sizeof(unsigned short)) != 2)
                                                        goto ErrorReadExactB;
                                                    searchResultCounter++;
                                                }
                                                break;

                                            case FLAG_BYTE:
                                                sceIoLseek(fd, 1, SEEK_CUR);
                                                if (*((unsigned char *) (scounter)) ==
                                                    (unsigned char) searchHistory[0].hakVal) {
                                                    //Add it
                                                    if (sceIoWrite(fd2, &scounter, sizeof(unsigned int)) != 4)
                                                        goto ErrorReadExactB;
                                                    if (sceIoWrite(fd2, &searchHistory[0].hakVal,
                                                                   sizeof(unsigned char)) != 1)
                                                        goto ErrorReadExactB;
                                                    searchResultCounter++;
                                                }
                                                break;
                                        }

                                        //Next
                                        counter--;
                                    }

                                    //Close the files
                                    sceIoClose(fd);
                                    sceIoClose(fd2);

                                    while (1) {
                                        break;
                                    //ReadShort
                                    ErrorReadExactB:
                                        sceIoClose(fd);
                                        sceIoClose(fd2);
                                        if (searchNo > 0) searchNo--;
                                        sceIoRemove(buffer);

                                        lineClear(33);
                                        pspDebugScreenSetTextColor(0xFF0000FF);
                                        pspDebugScreenPuts("ERROR: MemoryStick out of Space!");
                                        sceKernelDelayThread(3000000);
                                        break;
                                    }
                                } else {
                                    //ERROR - files couldn't be opened - undo search attempt
                                    sceIoClose(fd);
                                    sceIoClose(fd2);
                                    if (searchNo > 0) searchNo--;
                                }
                            }
                        } else if (extSelected[0] == 2) {
                            //Reset the search
                            if (searchNo > 0) searchNo--;

                            //Move the cursor back
                            extSelected[0] = 0;
                        } else {
                            //Add the single cheat
                            switch (searchHistory[0].flags & FLAG_DWORD) {
                                case FLAG_DWORD:
                                    cheatNew(4, searchAddress[extSelected[0] - 3] - 0x48800000,
                                             *((unsigned int *) (searchAddress[extSelected[0] - 3])));
                                    break;
                                case FLAG_WORD:
                                    cheatNew(2, searchAddress[extSelected[0] - 3] - 0x48800000,
                                             *((unsigned short *) (searchAddress[extSelected[0] - 3])));
                                    break;
                                case FLAG_BYTE:
                                    cheatNew(1, searchAddress[extSelected[0] - 3] - 0x48800000,
                                             *((unsigned char *) (searchAddress[extSelected[0] - 3])));
                                    break;
                                default:
                                    break;
                            }

                            //Switch to the cheat editor
                            extOptArg = extSelected[0];
                            pspDebugScreenInitEx(vram, 0, 0);
                            extSelected[1] = extSelected[2] = extSelected[3] = 0;
                            extSelected[0] = cheat[cheatTotal - 1].block;
                            cheatSelected = cheatTotal - 1;
                            extMenu = 1;
                            extOpt = 1;
                            menuDraw();
                            sceKernelDelayThread(150000);
                        }

                        //Load the file again, get the sample numbers
                        if (searchNo > 0) {
                            //Open the file
                            sprintf(buffer, "ms0:/search%d.dat", searchNo);
                            fd = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
                            if (fd > 0) {
                                //Get the value size
                                sceIoRead(fd, &miscType, 1);
                                miscType -= '0';

                                switch (miscType) {
                                    case 1:
                                        searchHistory[0].flags = (searchHistory[0].flags & ~FLAG_DWORD) | FLAG_BYTE;
                                        break;
                                    case 2:
                                        searchHistory[0].flags = (searchHistory[0].flags & ~FLAG_DWORD) | FLAG_WORD;
                                        break;
                                    case 4:
                                        searchHistory[0].flags = (searchHistory[0].flags & ~FLAG_DWORD) | FLAG_DWORD;
                                        break;
                                }

                                //Get the file size
                                searchResultCounter = sceIoLseek(fd, 0, SEEK_END);
                                sceIoLseek(fd, 1, SEEK_SET);
                                searchResultCounter--;
                                searchResultCounter /= (sizeof(unsigned int) + miscType);

                                //Only load the first 100
                                if (searchResultCounter > 100) {
                                    for (scounter = 0; scounter < 100; scounter++) {
                                        sceIoRead(fd, &searchAddress[scounter], sizeof(unsigned int));
                                        sceIoLseek(fd, miscType, SEEK_CUR);
                                    }
                                } else {
                                    for (scounter = 0; scounter < searchResultCounter; scounter++) {
                                        sceIoRead(fd, &searchAddress[scounter], sizeof(unsigned int));
                                        sceIoLseek(fd, miscType, SEEK_CUR);
                                    }
                                }

                                //Close the file since we are done with the search
                                sceIoClose(fd);
                            }
                        } else {
                            searchResultCounter = 0;
                        }

                        pspDebugScreenInitEx(vram, 0, 0);
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }

                    if (extSelected[3]) {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            switch (extSelected[1]) {
                                case 0:
                                    searchHistory[0].hakVal += (1 << (4 * (7 - extSelected[2])));
                                    break;
                                case 1:
                                    searchHistory[0].hakVal += decDelta[extSelected[2]];
                                    break;
                                case 2:
                                    searchHistory[0].hakVal += (1 << (8 * (extSelected[2])));
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            switch (extSelected[1]) {
                                case 0:
                                    searchHistory[0].hakVal -= (1 << (4 * (7 - extSelected[2])));
                                    break;
                                case 1:
                                    searchHistory[0].hakVal -= decDelta[extSelected[2]];
                                    break;
                                case 2:
                                    searchHistory[0].hakVal -= (1 << (8 * (extSelected[2])));
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    } else {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            if (extSelected[0] > 0) {
                                extSelected[0]--;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            if (extSelected[0] <
                                (1 + (!!searchNo) + (searchResultCounter > 100 ? 100 : searchResultCounter))) {
                                extSelected[0]++;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    }

                    if (extSelected[0] == 0) {
                        if (pad.Buttons & PSP_CTRL_LEFT) {
                            extSelected[2]--;
                            switch (extSelected[1]) {
                                case 0:
                                    if ((signed) extSelected[2] == -1) { extSelected[2] = 0; }
                                    break;
                                case 1:
                                    if ((signed) extSelected[2] == -1) {
                                        extSelected[2] = 7;
                                        extSelected[1]--;
                                    }
                                    break;
                                case 2:
                                    if ((signed) extSelected[2] == -1) {
                                        extSelected[2] = 9;
                                        extSelected[1]--;
                                    }
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                        } else if (pad.Buttons & PSP_CTRL_RIGHT) {
                            extSelected[2]++;
                            switch (extSelected[1]) {
                                case 0:
                                    if (extSelected[2] > 7) {
                                        extSelected[2] = 0;
                                        extSelected[1]++;
                                    }
                                    break;
                                case 1:
                                    if (extSelected[2] > 9) {
                                        extSelected[2] = 0;
                                        extSelected[1]++;
                                    }
                                    break;
                                case 2:
                                    if (extSelected[2] > 3) { extSelected[2] = 3; }
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                        } else {
                            cheatButtonAgeX = 0;
                        }
                    }

                    if ((padButtons & PSP_CTRL_CIRCLE) && !(pad.Buttons & PSP_CTRL_CIRCLE)) {
                        if (extSelected[3]) {
                            extSelected[3] = 0;
                            menuDraw();
                            sceKernelDelayThread(150000);
                        } else//Search has been aborted!
                        {
                            //Do we need to act like the search didn't even happen???
                            if (searchNo == 0) {
                                cheatSearch = 0;
                            }

                            //Go back
                            cheatSelected = 0;
                            pspDebugScreenInitEx(vram, 0, 0);
                            extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                            extMenu = 0;
                            extOpt = 0;
                            menuDraw();
                            sceKernelDelayThread(150000);
                            break;
                        }
                    }
                    break;

                case 3://INPUT EXT DIFF SEARCH
                    if (pad.Buttons & PSP_CTRL_SELECT) {
                        copyMenu = 1;
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }
                    if (pad.Buttons & PSP_CTRL_CROSS) {
                        if (extSelected[0] == 0) {
                            extSelected[3] = !extSelected[3];
                        } else if (extSelected[0] == 1) {
                            //Update the search history!
                            if (searchHistoryCounter < 15) {
                                searchHistoryCounter++;
                            }
                            memmove(&searchHistory[1], &searchHistory[0], sizeof(Block) * (15));
                            searchHistory[1].stdVal = searchMode;

                            //Move the cursor back
                            extSelected[0] = 0;

                            //Is it the first search?
                            if (searchNo == 0) {
                                //Increment the search
                                searchNo++;
                                searchMax++;

                                //Setup the variables
                                searchResultCounter = 0;

                                //Open the files
                                fd = sceIoOpen("ms0:/search.ram", PSP_O_RDONLY, 0777);
                                sprintf(buffer, "ms0:/search%d.dat", searchNo);
                                fd2 = sceIoOpen(buffer, PSP_O_WRONLY | PSP_O_CREAT, 0777);
                                if ((fd > 0) && (fd2 > 0)) {
                                    //Skip the initial 0x4000 bytes
                                    sceIoLseek(fd, 0x4000, SEEK_SET);

                                    //Write out the searcHistory[0] type
                                    switch (searchHistory[0].flags & FLAG_DWORD) {
                                        case FLAG_DWORD:
                                            if (sceIoWrite(fd2, "4", 1) != 1) goto ErrorReadDiffA;
                                            miscType = 4;
                                            break;
                                        case FLAG_WORD:
                                            if (sceIoWrite(fd2, "2", 1) != 1) goto ErrorReadDiffA;
                                            miscType = 2;
                                            break;
                                        case FLAG_BYTE:
                                            if (sceIoWrite(fd2, "1", 1) != 1) goto ErrorReadDiffA;
                                            miscType = 1;
                                            break;
                                    }

                                    //Get ready
                                    counter = START_ADDRESS;

                                    //Go!
                                    while (counter < END_ADDRESS) {
                                        //Load it
                                        //sceIoRead(fd, &scounter, miscType);
                                        fileBufferRead(&scounter, miscType);

                                        //Helper
                                        if (!((counter - START_ADDRESS) & 0xFFFF)) {
                                            if (!cheatPause) sceKernelDelayThread(1500);

                                            lineClear(33);
                                            pspDebugScreenSetTextColor(0xFFFF8000);
                                            sprintf(buffer, "Searching = %02d%%; Hold () to Abort",
                                                    (counter - START_ADDRESS) / ((END_ADDRESS - START_ADDRESS) / 100));
                                            pspDebugScreenPuts(buffer);

                                            sceCtrlPeekBufferPositive(&pad, 1);

                                            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                                                lineClear(33);
                                                pspDebugScreenSetTextColor(0xFFFF8000);
                                                pspDebugScreenPuts("Search Aborted");

                                                do {
                                                    sceKernelDelayThread(150000);
                                                    sceCtrlPeekBufferPositive(&pad, 1);
                                                } while (pad.Buttons & PSP_CTRL_CIRCLE);
                                                break;
                                            }
                                        }

                                        //Check
                                        switch (searchHistory[0].flags & FLAG_DWORD) {
                                            case FLAG_DWORD:
                                                if (searchMode == 0) {
                                                    if ((unsigned int) scounter != *((unsigned int *) (counter))) break;
                                                } else if (searchMode == 1) {
                                                    if ((unsigned int) scounter == *((unsigned int *) (counter))) break;
                                                } else if (searchMode == 2) {
                                                    if ((unsigned int) scounter >= *((unsigned int *) (counter))) break;
                                                } else if (searchMode == 3) {
                                                    if ((unsigned int) scounter <= *((unsigned int *) (counter))) break;
                                                } else if (searchMode == 4) {
                                                    if ((unsigned int) ((unsigned int) scounter +
                                                                        (unsigned int) searchHistory[0].hakVal) !=
                                                        *((unsigned int *) (counter)))
                                                        break;
                                                } else if (searchMode == 5) {
                                                    if ((unsigned int) ((unsigned int) scounter -
                                                                        (unsigned int) searchHistory[0].hakVal) !=
                                                        *((unsigned int *) (counter)))
                                                        break;
                                                }
                                                scounter = *((unsigned int *) (counter));

                                                //Add it
                                                if (sceIoWrite(fd2, &counter, sizeof(unsigned int)) != 4)
                                                    goto ErrorReadDiffA;
                                                if (sceIoWrite(fd2, &scounter, sizeof(unsigned int)) != 4)
                                                    goto ErrorReadDiffA;
                                                searchResultCounter++;
                                                break;

                                            case FLAG_WORD:
                                                if (searchMode == 0) {
                                                    if ((unsigned short) scounter !=
                                                        *((unsigned short *) (counter)))
                                                        break;
                                                } else if (searchMode == 1) {
                                                    if ((unsigned short) scounter ==
                                                        *((unsigned short *) (counter)))
                                                        break;
                                                } else if (searchMode == 2) {
                                                    if ((unsigned short) scounter >=
                                                        *((unsigned short *) (counter)))
                                                        break;
                                                } else if (searchMode == 3) {
                                                    if ((unsigned short) scounter <=
                                                        *((unsigned short *) (counter)))
                                                        break;
                                                } else if (searchMode == 4) {
                                                    if ((unsigned short) ((unsigned short) scounter +
                                                                          (unsigned short) searchHistory[0].hakVal) !=
                                                        *((unsigned short *) (counter)))
                                                        break;
                                                } else if (searchMode == 5) {
                                                    if ((unsigned short) ((unsigned short) scounter -
                                                                          (unsigned short) searchHistory[0].hakVal) !=
                                                        *((unsigned short *) (counter)))
                                                        break;
                                                }
                                                scounter = *((unsigned short *) (counter));

                                                //Add it
                                                if (sceIoWrite(fd2, &counter, sizeof(unsigned int)) != 4)
                                                    goto ErrorReadDiffA;
                                                if (sceIoWrite(fd2, &scounter, sizeof(unsigned short)) != 2)
                                                    goto ErrorReadDiffA;
                                                searchResultCounter++;
                                                break;

                                            case FLAG_BYTE:
                                                if (searchMode == 0) {
                                                    if ((unsigned char) scounter != *((unsigned char *) (counter)))
                                                        break;
                                                } else if (searchMode == 1) {
                                                    if ((unsigned char) scounter == *((unsigned char *) (counter)))
                                                        break;
                                                } else if (searchMode == 2) {
                                                    if ((unsigned char) scounter >= *((unsigned char *) (counter)))
                                                        break;
                                                } else if (searchMode == 3) {
                                                    if ((unsigned char) scounter <= *((unsigned char *) (counter)))
                                                        break;
                                                } else if (searchMode == 4) {
                                                    if ((unsigned char) ((unsigned char) scounter +
                                                                         (unsigned char) searchHistory[0].hakVal) !=
                                                        *((unsigned char *) (counter)))
                                                        break;
                                                } else if (searchMode == 5) {
                                                    if ((unsigned char) ((unsigned char) scounter -
                                                                         (unsigned char) searchHistory[0].hakVal) !=
                                                        *((unsigned char *) (counter)))
                                                        break;
                                                }
                                                scounter = *((unsigned char *) (counter));

                                                //Add it
                                                if (sceIoWrite(fd2, &counter, sizeof(unsigned int)) != 4)
                                                    goto ErrorReadDiffA;
                                                if (sceIoWrite(fd2, &scounter, sizeof(unsigned char)) != 1)
                                                    goto ErrorReadDiffA;
                                                searchResultCounter++;
                                                break;
                                        }

                                        //Next
                                        counter += miscType;
                                    }

                                    //Close the files
                                    sceIoClose(fd);
                                    sceIoClose(fd2);

                                    while (1) {
                                        break;
                                    //ReadShort
                                    ErrorReadDiffA:
                                        sceIoClose(fd);
                                        sceIoClose(fd2);
                                        if (searchNo > 0) searchNo--;
                                        sceIoRemove(buffer);

                                        lineClear(33);
                                        pspDebugScreenSetTextColor(0xFF0000FF);
                                        pspDebugScreenPuts("ERROR: MemoryStick out of Space!");
                                        sceKernelDelayThread(3000000);
                                        break;
                                    }
                                } else {
                                    //ERROR - files couldn't be opened - undo search attempt
                                    sceIoClose(fd);
                                    sceIoClose(fd2);
                                    if (searchNo > 0) searchNo--;
                                }
                            } else//Continue the search with a different Diff number
                            {
                                //Increment the search
                                searchNo++;
                                searchMax++;

                                //Open the files
                                sprintf(buffer, "ms0:/search%d.dat", searchNo - 1);
                                fd = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
                                sprintf(buffer, "ms0:/search%d.dat", searchNo);
                                fd2 = sceIoOpen(buffer, PSP_O_WRONLY | PSP_O_CREAT, 0777);
                                if ((fd > 0) && (fd2 > 0)) {
                                    //Get ready to go through and check the addresses
                                    sceIoLseek(fd, 1, SEEK_SET);

                                    //Write out the searcHistory[0] type
                                    switch (searchHistory[0].flags & FLAG_DWORD) {
                                        case FLAG_DWORD:
                                            if (sceIoWrite(fd2, "4", 1) != 1) goto ErrorReadDiffB;
                                            break;
                                        case FLAG_WORD:
                                            if (sceIoWrite(fd2, "2", 1) != 1) goto ErrorReadDiffB;
                                            break;
                                        case FLAG_BYTE:
                                            if (sceIoWrite(fd2, "1", 1) != 1) goto ErrorReadDiffB;
                                            break;
                                    }

                                    //Loop through the list checking each one
                                    counter = searchResultCounter;
                                    searchResultCounter = 0;
                                    while (counter > 0) {
                                        //Load it
                                        sceIoRead(fd, &scounter, sizeof(unsigned int));

                                        //Helper
                                        if (!(counter & 0x3F)) {
                                            if (!cheatPause) sceKernelDelayThread(1500);

                                            lineClear(33);
                                            pspDebugScreenSetTextColor(0xFFFF8000);
                                            sprintf(buffer, "Searching = %02d%%; Hold () to Abort",
                                                    (scounter - START_ADDRESS) / ((END_ADDRESS - START_ADDRESS) / 100));
                                            pspDebugScreenPuts(buffer);

                                            sceCtrlPeekBufferPositive(&pad, 1);

                                            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                                                lineClear(33);
                                                pspDebugScreenSetTextColor(0xFFFF8000);
                                                pspDebugScreenPuts("Search Aborted");

                                                do {
                                                    sceKernelDelayThread(150000);
                                                    sceCtrlPeekBufferPositive(&pad, 1);
                                                } while (pad.Buttons & PSP_CTRL_CIRCLE);
                                                break;
                                            }
                                        }

                                        //Check
                                        switch (searchHistory[0].flags & FLAG_DWORD) {
                                            case FLAG_DWORD:
                                                sceIoRead(fd, &dcounter, sizeof(unsigned int));

                                                if (searchMode == 0) {
                                                    if ((unsigned int) dcounter != *((unsigned int *) (scounter)))
                                                        break;
                                                } else if (searchMode == 1) {
                                                    if ((unsigned int) dcounter == *((unsigned int *) (scounter)))
                                                        break;
                                                } else if (searchMode == 2) {
                                                    if ((unsigned int) dcounter >= *((unsigned int *) (scounter)))
                                                        break;
                                                } else if (searchMode == 3) {
                                                    if ((unsigned int) dcounter <= *((unsigned int *) (scounter)))
                                                        break;
                                                } else if (searchMode == 4) {
                                                    if ((unsigned int) ((unsigned int) dcounter +
                                                                        (unsigned int) searchHistory[0].hakVal) !=
                                                        *((unsigned int *) (scounter)))
                                                        break;
                                                } else if (searchMode == 5) {
                                                    if ((unsigned int) ((unsigned int) dcounter -
                                                                        (unsigned int) searchHistory[0].hakVal) !=
                                                        *((unsigned int *) (scounter)))
                                                        break;
                                                }
                                                dcounter = *((unsigned int *) (scounter));

                                                //Add it
                                                if (sceIoWrite(fd2, &scounter, sizeof(unsigned int)) != 4)
                                                    goto ErrorReadDiffB;
                                                if (sceIoWrite(fd2, &dcounter, sizeof(unsigned int)) != 4)
                                                    goto ErrorReadDiffB;
                                                searchResultCounter++;
                                                break;

                                            case FLAG_WORD:
                                                sceIoRead(fd, &dcounter, sizeof(unsigned short));

                                                if (searchMode == 0) {
                                                    if ((unsigned short) dcounter !=
                                                        *((unsigned short *) (scounter)))
                                                        break;
                                                } else if (searchMode == 1) {
                                                    if ((unsigned short) dcounter ==
                                                        *((unsigned short *) (scounter)))
                                                        break;
                                                } else if (searchMode == 2) {
                                                    if ((unsigned short) dcounter >=
                                                        *((unsigned short *) (scounter)))
                                                        break;
                                                } else if (searchMode == 3) {
                                                    if ((unsigned short) dcounter <=
                                                        *((unsigned short *) (scounter)))
                                                        break;
                                                } else if (searchMode == 4) {
                                                    if ((unsigned short) ((unsigned short) dcounter +
                                                                          (unsigned short) searchHistory[0].hakVal) !=
                                                        *((unsigned short *) (scounter)))
                                                        break;
                                                } else if (searchMode == 5) {
                                                    if ((unsigned short) ((unsigned short) dcounter -
                                                                          (unsigned short) searchHistory[0].hakVal) !=
                                                        *((unsigned short *) (scounter)))
                                                        break;
                                                }
                                                dcounter = *((unsigned short *) (scounter));

                                                //Add it
                                                if (sceIoWrite(fd2, &scounter, sizeof(unsigned int)) != 4)
                                                    goto ErrorReadDiffB;
                                                if (sceIoWrite(fd2, &dcounter, sizeof(unsigned short)) != 2)
                                                    goto ErrorReadDiffB;
                                                searchResultCounter++;
                                                break;

                                            case FLAG_BYTE:
                                                sceIoRead(fd, &dcounter, sizeof(unsigned char));

                                                if (searchMode == 0) {
                                                    if ((unsigned char) dcounter !=
                                                        *((unsigned char *) (scounter)))
                                                        break;
                                                } else if (searchMode == 1) {
                                                    if ((unsigned char) dcounter ==
                                                        *((unsigned char *) (scounter)))
                                                        break;
                                                } else if (searchMode == 2) {
                                                    if ((unsigned char) dcounter >=
                                                        *((unsigned char *) (scounter)))
                                                        break;
                                                } else if (searchMode == 3) {
                                                    if ((unsigned char) dcounter <=
                                                        *((unsigned char *) (scounter)))
                                                        break;
                                                } else if (searchMode == 4) {
                                                    if ((unsigned char) ((unsigned char) dcounter +
                                                                         (unsigned char) searchHistory[0].hakVal) !=
                                                        *((unsigned char *) (scounter)))
                                                        break;
                                                } else if (searchMode == 5) {
                                                    if ((unsigned char) ((unsigned char) dcounter -
                                                                         (unsigned char) searchHistory[0].hakVal) !=
                                                        *((unsigned char *) (scounter)))
                                                        break;
                                                }
                                                dcounter = *((unsigned char *) (scounter));

                                                //Add it
                                                if (sceIoWrite(fd2, &scounter, sizeof(unsigned int)) != 4)
                                                    goto ErrorReadDiffB;
                                                if (sceIoWrite(fd2, &dcounter, sizeof(unsigned char)) != 1)
                                                    goto ErrorReadDiffB;
                                                searchResultCounter++;
                                                break;
                                        }

                                        //Next
                                        counter--;
                                    }

                                    //Close the files
                                    sceIoClose(fd);
                                    sceIoClose(fd2);

                                    while (1) {
                                        break;
                                    //ReadShort
                                    ErrorReadDiffB:
                                        sceIoClose(fd);
                                        sceIoClose(fd2);
                                        if (searchNo > 0) searchNo--;
                                        sceIoRemove(buffer);

                                        lineClear(33);
                                        pspDebugScreenSetTextColor(0xFF0000FF);
                                        pspDebugScreenPuts("ERROR: MemoryStick out of Space!");
                                        sceKernelDelayThread(3000000);
                                        break;
                                    }
                                } else {
                                    //ERROR - files couldn't be opened - undo search attempt
                                    sceIoClose(fd);
                                    sceIoClose(fd2);
                                    if (searchNo > 0) searchNo--;
                                }
                            }
                        } else if (extSelected[0] == 2) {
                            //Reset the search
                            if (searchNo > 0) searchNo--;

                            //Move the cursor back
                            extSelected[0] = 0;
                        } else {
                            //Add the single cheat
                            switch (searchHistory[0].flags & FLAG_DWORD) {
                                case FLAG_DWORD:
                                    cheatNew(4, searchAddress[extSelected[0] - 3] - 0x48800000,
                                             *((unsigned int *) (searchAddress[extSelected[0] - 3])));
                                    break;
                                case FLAG_WORD:
                                    cheatNew(2, searchAddress[extSelected[0] - 3] - 0x48800000,
                                             *((unsigned short *) (searchAddress[extSelected[0] - 3])));
                                    break;
                                case FLAG_BYTE:
                                    cheatNew(1, searchAddress[extSelected[0] - 3] - 0x48800000,
                                             *((unsigned char *) (searchAddress[extSelected[0] - 3])));
                                    break;
                                default:
                                    break;
                            }

                            //Switch to the cheat editor
                            extOptArg = extSelected[0];
                            pspDebugScreenInitEx(vram, 0, 0);
                            extSelected[1] = extSelected[2] = extSelected[3] = 0;
                            extSelected[0] = cheat[cheatTotal - 1].block;
                            cheatSelected = cheatTotal - 1;
                            extMenu = 1;
                            extOpt = 2;
                            menuDraw();
                            sceKernelDelayThread(150000);
                        }

                        //Load the file again, get the sample numbers
                        if (searchNo > 0) {
                            //Open the file
                            sprintf(buffer, "ms0:/search%d.dat", searchNo);
                            fd = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
                            if (fd > 0) {
                                //Get the value size
                                sceIoRead(fd, &miscType, 1);
                                miscType -= '0';

                                switch (miscType) {
                                    case 1:
                                        searchHistory[0].flags = (searchHistory[0].flags & ~FLAG_DWORD) | FLAG_BYTE;
                                        break;
                                    case 2:
                                        searchHistory[0].flags = (searchHistory[0].flags & ~FLAG_DWORD) | FLAG_WORD;
                                        break;
                                    case 4:
                                        searchHistory[0].flags = (searchHistory[0].flags & ~FLAG_DWORD) | FLAG_DWORD;
                                        break;
                                }

                                //Get the file size
                                searchResultCounter = sceIoLseek(fd, 0, SEEK_END);
                                sceIoLseek(fd, 1, SEEK_SET);
                                searchResultCounter--;
                                searchResultCounter /= (sizeof(unsigned int) + miscType);

                                //Only load the first 100
                                if (searchResultCounter > 100) {
                                    for (scounter = 0; scounter < 100; scounter++) {
                                        sceIoRead(fd, &searchAddress[scounter], sizeof(unsigned int));
                                        sceIoLseek(fd, miscType, SEEK_CUR);
                                    }
                                } else {
                                    for (scounter = 0; scounter < searchResultCounter; scounter++) {
                                        sceIoRead(fd, &searchAddress[scounter], sizeof(unsigned int));
                                        sceIoLseek(fd, miscType, SEEK_CUR);
                                    }
                                }

                                //Close the file since we are done with the search
                                sceIoClose(fd);
                            }
                        } else {
                            searchResultCounter = 0;
                        }

                        pspDebugScreenInitEx(vram, 0, 0);
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }

                    if (extSelected[3]) {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            switch (extSelected[1]) {
                                case 0:
                                    if (searchMode < 5) searchMode++;
                                    break;
                                case 1:
                                    searchHistory[0].hakVal += (1 << (4 * (7 - extSelected[2])));
                                    break;
                                case 2:
                                    searchHistory[0].hakVal += decDelta[extSelected[2]];
                                    break;
                                case 3:
                                    searchHistory[0].hakVal += (1 << (8 * (extSelected[2])));
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            switch (extSelected[1]) {
                                case 0:
                                    if (searchMode > 0) searchMode--;
                                    break;
                                case 1:
                                    searchHistory[0].hakVal -= (1 << (4 * (7 - extSelected[2])));
                                    break;
                                case 2:
                                    searchHistory[0].hakVal -= decDelta[extSelected[2]];
                                    break;
                                case 3:
                                    searchHistory[0].hakVal -= (1 << (8 * (extSelected[2])));
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    } else {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            if (extSelected[0] > 0) {
                                extSelected[0]--;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            if (extSelected[0] <
                                (1 + (!!searchNo) + (searchResultCounter > 100 ? 100 : searchResultCounter))) {
                                extSelected[0]++;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    }

                    if (extSelected[0] == 0) {
                        if (pad.Buttons & PSP_CTRL_LEFT) {
                            extSelected[2]--;
                            switch (extSelected[1]) {
                                case 0:
                                    if ((signed) extSelected[2] == -1) { extSelected[2] = 0; }
                                    break;
                                case 1:
                                    if ((signed) extSelected[2] == -1) {
                                        extSelected[2] = 0;
                                        extSelected[1]--;
                                    }
                                    break;
                                case 2:
                                    if ((signed) extSelected[2] == -1) {
                                        extSelected[2] = 7;
                                        extSelected[1]--;
                                    }
                                    break;
                                case 3:
                                    if ((signed) extSelected[2] == -1) {
                                        extSelected[2] = 9;
                                        extSelected[1]--;
                                    }
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                        } else if (pad.Buttons & PSP_CTRL_RIGHT) {
                            extSelected[2]++;
                            if (searchMode < 4) {
                                extSelected[2] = 0;
                            } else {
                                switch (extSelected[1]) {
                                    case 0:
                                        if (extSelected[2] > 0) {
                                            extSelected[2] = 0;
                                            extSelected[1]++;
                                        }
                                        break;
                                    case 1:
                                        if (extSelected[2] > 7) {
                                            extSelected[2] = 0;
                                            extSelected[1]++;
                                        }
                                        break;
                                    case 2:
                                        if (extSelected[2] > 9) {
                                            extSelected[2] = 0;
                                            extSelected[1]++;
                                        }
                                        break;
                                    case 3:
                                        if (extSelected[2] > 3) { extSelected[2] = 3; }
                                        break;
                                }
                            }
                            menuDraw();
                            if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                        } else {
                            cheatButtonAgeX = 0;
                        }
                    }

                    if ((padButtons & PSP_CTRL_CIRCLE) && !(pad.Buttons & PSP_CTRL_CIRCLE)) {
                        if (extSelected[3]) {
                            extSelected[3] = 0;
                            menuDraw();
                            sceKernelDelayThread(150000);
                        } else//Search has been aborted!
                        {
                            //Go back
                            pspDebugScreenInitEx(vram, 0, 0);
                            extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                            extMenu = 0;
                            extOpt = 0;
                            menuDraw();
                            sceKernelDelayThread(150000);
                            break;
                        }
                    }
                    break;

                case 4://INPUT EXT TEXT SEARCH
                    if (pad.Buttons & PSP_CTRL_SELECT) {
                        copyMenu = 1;
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }

                    if (pad.Buttons & PSP_CTRL_CROSS) {
                        if (extSelected[0] == 0) {
                            extSelected[3] = !extSelected[3];
                        } else if (extSelected[0] == 1) {
                            //Move the cursor back
                            extSelected[0] = 0;

                            //Setup the variables
                            searchResultCounter = 0;

                            //Search!
                            counter = START_ADDRESS;

                            //Helper
                            while (counter < END_ADDRESS) {
                                //Helper
                                if (!((counter - START_ADDRESS) & 0xFFFF)) {
                                    if (!cheatPause) sceKernelDelayThread(1500);

                                    lineClear(33);
                                    pspDebugScreenSetTextColor(0xFFFF8000);
                                    sprintf(buffer, "Searching = %02d%%; Hold () to Abort",
                                            (counter - START_ADDRESS) / ((END_ADDRESS - START_ADDRESS) / 100));
                                    pspDebugScreenPuts(buffer);

                                    sceCtrlPeekBufferPositive(&pad, 1);

                                    if (pad.Buttons & PSP_CTRL_CIRCLE) {
                                        lineClear(33);
                                        pspDebugScreenSetTextColor(0xFFFF8000);
                                        pspDebugScreenPuts("Search Aborted");

                                        do {
                                            sceKernelDelayThread(150000);
                                            sceCtrlPeekBufferPositive(&pad, 1);
                                        } while (pad.Buttons & PSP_CTRL_CIRCLE);
                                        break;
                                    }
                                }

                                //Check
                                scounter = 0;
                                while (scounter < 50) {
                                    if (counter + scounter <= 0x49FFFFFF) {
                                        unsigned char tempLetter = *((unsigned char *) (counter + scounter));
                                        if ((tempLetter >= 0x61) && (tempLetter <= 0x7A)) tempLetter -= 0x20;
                                        if (tempLetter == (unsigned char) fileBuffer[scounter]) {
                                            scounter++;
                                            if (!fileBuffer[scounter + 1]) {
                                                //Add it
                                                searchAddress[searchResultCounter] = counter;
                                                searchResultCounter++;
                                                break;
                                            }
                                        } else {
                                            if (scounter == 0) {
                                                scounter = 1;
                                            }
                                            break;
                                        }
                                    } else {
                                        scounter++;
                                        break;
                                    }
                                }

                                counter += scounter;

                                if (searchResultCounter == 100) break;
                            }
                        }

                        pspDebugScreenInitEx(vram, 0, 0);
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }

                    if (extSelected[3]) {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            fileBuffer[extSelected[2]]++;
                            switch (fileBuffer[extSelected[2]]) {
                                case 0x01:
                                    fileBuffer[extSelected[2]] = 0x20;
                                    break;
                                case 0x21:
                                    fileBuffer[extSelected[2]] = 0x41;
                                    break;
                                case 0x5B:
                                    fileBuffer[extSelected[2]] = 0x30;
                                    break;
                                case 0x3A:
                                    fileBuffer[extSelected[2]] = 0x21;
                                    break;
                                case 0x30:
                                    fileBuffer[extSelected[2]] = 0x3A;
                                    break;
                                case 0x41:
                                    fileBuffer[extSelected[2]] = 0x5B;
                                    break;
                                case 0x61:
                                    fileBuffer[extSelected[2]] = 0x7B;
                                    break;
                                case 0x7F:
                                    fileBuffer[extSelected[2]] = 0x20;
                                    break;
                            }

                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            fileBuffer[extSelected[2]]--;
                            switch (fileBuffer[extSelected[2]]) {
                                case 0xFF:
                                    fileBuffer[extSelected[2]] = 0x7E;
                                    break;
                                case 0x7A:
                                    fileBuffer[extSelected[2]] = 0x60;
                                    break;
                                case 0x5A:
                                    fileBuffer[extSelected[2]] = 0x40;
                                    break;
                                case 0x39:
                                    fileBuffer[extSelected[2]] = 0x2F;
                                    break;
                                case 0x20:
                                    fileBuffer[extSelected[2]] = 0x39;
                                    break;
                                case 0x2F:
                                    fileBuffer[extSelected[2]] = 0x5A;
                                    break;
                                case 0x40:
                                    fileBuffer[extSelected[2]] = 0x20;
                                    break;
                                case 0x1F:
                                    fileBuffer[extSelected[2]] = 0x7E;
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    } else {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            if (extSelected[0] > 0) {
                                extSelected[0]--;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            if (extSelected[0] < (1 + (searchResultCounter > 100 ? 100 : searchResultCounter))) {
                                extSelected[0]++;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    }

                    if (extSelected[0] == 0) {
                        if (pad.Buttons & PSP_CTRL_SQUARE) {
                            memset(&fileBuffer[extSelected[2] + 1], 0, 49 - extSelected[2]);
                            menuDraw();
                        }

                        if (pad.Buttons & PSP_CTRL_LEFT) {
                            extSelected[2]--;
                            if ((signed) extSelected[2] == -1) { extSelected[2] = 0; }
                            menuDraw();
                            if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                        } else if (pad.Buttons & PSP_CTRL_RIGHT) {
                            extSelected[2]++;
                            if (extSelected[2] == 50) { extSelected[2] = 49; }
                            if (fileBuffer[extSelected[2]] == 0x00) fileBuffer[extSelected[2]] = 'A';
                            menuDraw();
                            if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                        } else {
                            cheatButtonAgeX = 0;
                        }
                    } else if (extSelected[0] >= 2) {
                        if (pad.Buttons & PSP_CTRL_LEFT) {
                            searchAddress[extSelected[0] - 2]--;
                            if (searchAddress[extSelected[0] - 2] < START_ADDRESS) {
                                searchAddress[extSelected[0] - 2] = START_ADDRESS;
                            }

                            menuDraw();
                            if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                        } else if (pad.Buttons & PSP_CTRL_RIGHT) {
                            searchAddress[extSelected[0] - 2]++;
                            if (searchAddress[extSelected[0] - 2] >= END_ADDRESS) {
                                searchAddress[extSelected[0] - 2] = 0x49FFFFFF;
                            }

                            menuDraw();
                            if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                        } else {
                            cheatButtonAgeX = 0;
                        }
                    }

                    if ((padButtons & PSP_CTRL_CIRCLE) && !(pad.Buttons & PSP_CTRL_CIRCLE)) {
                        if (extSelected[3]) {
                            extSelected[3] = 0;
                            menuDraw();
                            sceKernelDelayThread(150000);
                        } else//Search has been aborted!
                        {
                            //Go back
                            cheatSelected = 0;
                            pspDebugScreenInitEx(vram, 0, 0);
                            extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                            extMenu = 0;
                            extOpt = 0;
                            menuDraw();
                            sceKernelDelayThread(150000);
                            break;
                        }
                    }
                    break;
            }
        } else {
            //Overall button inputs
            if ((pad.Buttons & PSP_CTRL_LTRIGGER) && (tabSelected != -1)) {
                pspDebugScreenInitEx(vram, 0, 0);
                cheatSelected = 0;
                extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                extOpt = 0;
                if (tabSelected > 0) {
                    tabSelected--;
                } else if (tabSelected == 0) {
                    tabSelected = 5;
                }

                menuDraw();
                sceKernelDelayThread(150000);
            }
            if ((pad.Buttons & PSP_CTRL_RTRIGGER) && (tabSelected != 6)) {
                pspDebugScreenInitEx(vram, 0, 0);
                cheatSelected = 0;
                extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                extOpt = 0;
                if (tabSelected < 5) {
                    tabSelected++;
                } else if (tabSelected == 5) {
                    tabSelected = 0;
                }

                menuDraw();
                sceKernelDelayThread(150000);
            }
            if ((padButtons & PSP_CTRL_CIRCLE) && !(pad.Buttons & PSP_CTRL_CIRCLE)) {
                //Special case for the memory viewer
                if (extSelected[3]) {
                    extSelected[3] = 0;
                    menuDraw();
                    sceKernelDelayThread(150000);
                } else {
                    //Unregister the O key so that the user mode game doesn't pick it up
                    menuDrawn = 0;
                    return;
                }
            }
            if ((padButtons & PSP_CTRL_NOTE)) {
                lineClear(33);
                pspDebugScreenSetTextColor(0xFF0000FF);
                pspDebugScreenPuts("ERROR: Must press Music Button in game!");
                sceKernelDelayThread(1000000);
                lineClear(33);
                menuDraw();
                continue;
            }

            //This is for the warning message at the start
            if (lolInit) {
                if ((padButtons & PSP_CTRL_START) && !(pad.Buttons & PSP_CTRL_START)) {
                    //Special case for the memory viewer
                    lolInit = 0;
                    pspDebugScreenInitEx(vram, 0, 0);
                    menuDraw();
                    continue;
                }

                pspDebugScreenSetXY(0, 26);
                unsigned int red = 255;
                unsigned int green = 0;
                unsigned int blue = 0;
                unsigned int color = 0xFF000000 | (red << lolValue) | (green << lolValue) | blue << lolValue | lolValue;
                pspDebugScreenSetTextColor(color);
                pspDebugScreenPuts("                        Press [START] to Continue");
                lolValue += lolDirection;
                lolDirection = lolValue % 255 == 0 ? -lolDirection : lolDirection;

                sceKernelDelayThread(20000);
                continue;
            }

            //Choose the appropriate action based on the tabSelected
            //Used for each tab menu. make sure if moving tabs around in
            //the menuDraw function to update this switch statement
            switch (tabSelected) {
                case 0://INPUT CHEATS LIST
                    if (pad.Buttons & PSP_CTRL_UP) {
                        if (cheatSelected > 0) {
                            cheatSelected--;
                        } else if (cheatSelected == 0) {
                            cheatSelected = cheatTotal - 1;
                        }
                        menuDraw();
                        if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                    } else if (pad.Buttons & PSP_CTRL_DOWN) {
                        if (cheatSelected < (cheatTotal - 1)) {
                            cheatSelected++;
                        } else if (cheatSelected == (cheatTotal - 1)) {
                            cheatSelected = 0;
                        }
                        menuDraw();
                        if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                    } else {
                        cheatButtonAgeY = 0;
                    }
                    if (pad.Buttons & PSP_CTRL_CROSS) {
                        cheat[cheatSelected].flags = (cheat[cheatSelected].flags & (~FLAG_SELECTED)) |
                                                     ((~cheat[cheatSelected].flags) & FLAG_SELECTED);
                        cheat[cheatSelected].flags &= ~FLAG_CONSTANT;
                        cheat[cheatSelected].flags |= FLAG_FRESH;

                        menuDraw();
                        cheatApply(0);

                        sceKernelDelayThread(150000);
                    } else if (pad.Buttons & PSP_CTRL_SQUARE) {
                        cheat[cheatSelected].flags = (cheat[cheatSelected].flags & ~FLAG_CONSTANT) |
                                                     (~cheat[cheatSelected].flags & FLAG_CONSTANT);
                        cheat[cheatSelected].flags &= ~FLAG_SELECTED;
                        cheat[cheatSelected].flags |= FLAG_FRESH;

                        menuDraw();
                        cheatApply(0);

                        sceKernelDelayThread(150000);
                    } else if (pad.Buttons & PSP_CTRL_TRIANGLE) {
                        pspDebugScreenInitEx(vram, 0, 0);
                        extSelected[1] = extSelected[2] = extSelected[3] = 0;
                        extSelected[0] = cheat[cheatSelected].block;
                        extMenu = 1;
                        extOpt = 0;
                        menuDraw();

                        sceKernelDelayThread(150000);
                    }
                    break;

                case 1://INPUT SEARCHER
                    if (pad.Buttons & PSP_CTRL_UP) {
                        if (cheatSelected > 0) {
                            cheatSelected--;
                        } else if (cheatSelected == 0) {
                            cheatSelected = (2 + ((!cheatSearch) * 2));
                        }
                        menuDraw();
                        if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                    } else if (pad.Buttons & PSP_CTRL_DOWN) {
                        if (cheatSelected < (2 + ((!cheatSearch) * 2))) {
                            cheatSelected++;
                        } else if (cheatSelected == (2 + ((!cheatSearch) * 2))) {
                            cheatSelected = 0;
                        }
                        menuDraw();
                        if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                    } else {
                        cheatButtonAgeY = 0;
                    }
                    if (pad.Buttons & PSP_CTRL_CROSS) {
                        if (!cheatSearch) {
                            if (cheatSelected == 0) {
                                //Goto Find exact
                                pspDebugScreenInitEx(vram, 0, 0);
                                extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                                extMenu = 2;
                                extOpt = 0;
                                menuDraw();
                                cheatSearch = 1;
                            } else if (cheatSelected == 4) {
                                //Goto Find text
                                searchResultCounter = 0;
                                pspDebugScreenInitEx(vram, 0, 0);
                                memset(fileBuffer, 0, 50);
                                fileBuffer[0] = 'A';
                                extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                                extMenu = 4;
                                extOpt = 0;
                                menuDraw();
                            } else {
                                //Dump a search dump
                                fd = sceIoOpen("ms0:/search.ram", PSP_O_WRONLY | PSP_O_CREAT, 0777);
                                if (fd > 0) {
                                    if (sceIoWrite(fd, (void *) 0x08800000, 0x1800000) == 0x1800000) {
                                        sceIoClose(fd);

                                        cheatSearch = 1;
                                        switch (cheatSelected) {
                                            case 1:
                                                searchHistory[0].flags =
                                                        (searchHistory[0].flags & (~FLAG_DWORD)) | FLAG_BYTE;
                                                break;
                                            case 2:
                                                searchHistory[0].flags =
                                                        (searchHistory[0].flags & (~FLAG_DWORD)) | FLAG_WORD;
                                                break;
                                            case 3:
                                                searchHistory[0].flags =
                                                        (searchHistory[0].flags & (~FLAG_DWORD)) | FLAG_DWORD;
                                                break;
                                        }

                                        pspDebugScreenInitEx(vram, 0, 0);
                                        tabSelected = 1;
                                        cheatSelected = 1;
                                        menuDraw();
                                        lineClear(33);
                                        pspDebugScreenSetTextColor(0xFFFF8000);
                                        pspDebugScreenPuts("Now, resume the game!");

                                        sceKernelDelayThread(3000000);
                                    } else {
                                        sceIoClose(fd);

                                        sceIoRemove("ms0:/search.ram");

                                        lineClear(33);
                                        pspDebugScreenSetTextColor(0xFF0000FF);
                                        pspDebugScreenPuts("ERROR: MemoryStick out of Space!");

                                        sceKernelDelayThread(3000000);
                                    }
                                }
                            }
                        } else {
                            if (cheatSelected == 0) {
                                //Goto Find exact
                                pspDebugScreenInitEx(vram, 0, 0);
                                extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                                extMenu = 2;
                                extOpt = 0;
                                menuDraw();
                            } else if (cheatSelected == 1) {
                                //Goto Find Diff
                                pspDebugScreenInitEx(vram, 0, 0);
                                extSelected[0] = extSelected[1] = extSelected[2] = extSelected[3] = 0;
                                extMenu = 3;
                                extOpt = 0;
                                menuDraw();
                            } else if (cheatSelected == 2) {
                                sceIoRemove("ms0:/search.ram");

                                while (searchMax > 0) {
                                    sprintf(buffer, "ms0:/search%d.dat", searchMax);
                                    sceIoRemove(buffer);
                                    searchMax--;
                                }

                                //Reset fields
                                searchNo = 0;
                                cheatSearch = 0;
                                cheatSelected = 0;
                                searchResultCounter = 0;
                            }
                        }

                        menuDraw();
                        sceKernelDelayThread(150000);
                    }
                    break;

                case 2://INPUT MEM VIEW
                    if (pad.Buttons & PSP_CTRL_SELECT) {
                        copyMenu = 1;
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }
                    if (pad.Buttons & PSP_CTRL_CROSS) {
                        extSelected[3] = !extSelected[3];
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }

                    if (pad.Buttons & PSP_CTRL_LEFT) {
                        browseX--;
                        switch (browseC) {
                            case 0:
                                if ((signed) browseX == -1) { browseX = 0; }
                                break;
                            case 1:
                                if ((signed) browseX == -1) {
                                    browseX = 7;
                                    browseC--;
                                }
                                break;
                            case 2:
                                if ((signed) browseX == -1) {
                                    browseX = (2 * browseLines) - 1;
                                    browseC--;
                                }
                                break;
                        }
                        menuDraw();
                        if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                    } else if (pad.Buttons & PSP_CTRL_RIGHT) {
                        browseX++;
                        switch (browseC) {
                            case 0:
                                if (browseX > 7) {
                                    browseX = 0;
                                    browseC++;
                                }
                                break;
                            case 1:
                                if (browseX > ((2 * browseLines) - 1)) {
                                    browseX = 0;
                                    browseC++;
                                }
                                break;
                            case 2:
                                if (browseX > (browseLines - 1)) { browseX = browseLines - 1; }
                                break;
                        }
                        menuDraw();
                        if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                    } else {
                        cheatButtonAgeX = 0;
                    }

                    if (extSelected[3]) {
                        if (pad.Buttons & PSP_CTRL_SQUARE) {
                            if (browseC == 1) {
                                browseC = 2;
                                browseX = browseX / 2;
                            } else if (browseC == 2) {
                                browseC = 1;
                                browseX = 2 * browseX + 1;
                            }
                            menuDraw();
                            sceKernelDelayThread(150000);
                        }
                        if (pad.Buttons & PSP_CTRL_UP) {
                            switch (browseC) {
                                case 0:
                                    browseAddress += (1 << (4 * (7 - browseX)));
                                    if (browseAddress < START_ADDRESS) {
                                        browseAddress = START_ADDRESS;
                                    }
                                    if (browseAddress > (END_ADDRESS - (22 * browseLines))) {
                                        browseAddress = (END_ADDRESS - (22 * browseLines));
                                    }
                                    break;
                                case 1:
                                    if (browseX & 1) {
                                        *((unsigned char *) ((browseAddress + (browseY * browseLines)) +
                                                             (browseX / 2))) += 0x01;
                                    } else {
                                        *((unsigned char *) ((browseAddress + (browseY * browseLines)) +
                                                             (browseX / 2))) += 0x10;
                                    }
                                    sceKernelDcacheWritebackInvalidateRange(
                                            ((unsigned char *) ((browseAddress + (browseY * browseLines)) +
                                                                (browseX / 2))),
                                            1);
                                    sceKernelIcacheInvalidateRange(
                                            ((unsigned char *) ((browseAddress + (browseY * browseLines)) +
                                                                (browseX / 2))),
                                            1);
                                    break;
                                case 2:
                                    *((unsigned char *) ((browseAddress + (browseY * browseLines)) + browseX)) += 1;
                                    sceKernelDcacheWritebackInvalidateRange(
                                            ((unsigned char *) ((browseAddress + (browseY * browseLines)) + browseX)),
                                            1);
                                    sceKernelIcacheInvalidateRange(
                                            ((unsigned char *) ((browseAddress + (browseY * browseLines)) + browseX)),
                                            1);
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            switch (browseC) {
                                case 0:
                                    browseAddress -= (1 << (4 * (7 - browseX)));
                                    if (browseAddress < START_ADDRESS) {
                                        browseAddress = START_ADDRESS;
                                    }
                                    if (browseAddress > (END_ADDRESS - (22 * browseLines))) {
                                        browseAddress = (END_ADDRESS - (22 * browseLines));
                                    }
                                    break;
                                case 1:
                                    if (browseX & 1) {
                                        *((unsigned char *) ((browseAddress + (browseY * browseLines)) +
                                                             (browseX / 2))) -= 0x01;
                                    } else {
                                        *((unsigned char *) ((browseAddress + (browseY * browseLines)) +
                                                             (browseX / 2))) -= 0x10;
                                    }
                                    sceKernelDcacheWritebackInvalidateRange(
                                            ((unsigned char *) ((browseAddress + (browseY * browseLines)) +
                                                                (browseX / 2))),
                                            1);
                                    sceKernelIcacheInvalidateRange(
                                            ((unsigned char *) ((browseAddress + (browseY * browseLines)) +
                                                                (browseX / 2))),
                                            1);
                                    break;
                                case 2:
                                    *((unsigned char *) ((browseAddress + (browseY * browseLines)) + browseX)) -= 1;
                                    sceKernelDcacheWritebackInvalidateRange(
                                            ((unsigned char *) ((browseAddress + (browseY * browseLines)) + browseX)),
                                            1);
                                    sceKernelIcacheInvalidateRange(
                                            ((unsigned char *) ((browseAddress + (browseY * browseLines)) + browseX)),
                                            1);
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    } else if (pad.Buttons & PSP_CTRL_SQUARE) {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            browseAddress -= browseLines;
                            if (browseAddress < START_ADDRESS) {
                                browseAddress = START_ADDRESS;
                            }
                            if (browseAddress > (END_ADDRESS - (22 * browseLines))) {
                                browseAddress = (END_ADDRESS - (22 * browseLines));
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            browseAddress += browseLines;
                            if (browseAddress < START_ADDRESS) {
                                browseAddress = START_ADDRESS;
                            }
                            if (browseAddress > (END_ADDRESS - (22 * browseLines))) {
                                browseAddress = (END_ADDRESS - (22 * browseLines));
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                        if ((abs((signed) (pad.Ly - 127)) - 40) > 0) {
                            browseAddress += (((signed) (pad.Ly - 127)) / browseLines) * 64;
                            if (browseAddress < START_ADDRESS) {
                                browseAddress = START_ADDRESS;
                            }
                            if (browseAddress > (END_ADDRESS - (22 * browseLines))) {
                                browseAddress = (END_ADDRESS - (22 * browseLines));
                            }
                            menuDraw();
                            sceKernelDelayThread(18750);
                        }
                    } else {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            if (browseY > 0) {
                                browseY--;
                            } else if (browseY == 0) {
                                browseAddress -= browseLines;
                                if (browseAddress < START_ADDRESS) {
                                    browseAddress = START_ADDRESS;
                                }
                                if (browseAddress > (END_ADDRESS - (22 * browseLines))) {
                                    browseAddress = (END_ADDRESS - (22 * browseLines));
                                }
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            if (browseY < 21) {
                                browseY++;
                            } else if (browseY == 21) {
                                browseAddress += browseLines;
                                if (browseAddress < START_ADDRESS) {
                                    browseAddress = START_ADDRESS;
                                }
                                if (browseAddress > (END_ADDRESS - (22 * browseLines))) {
                                    browseAddress = (END_ADDRESS - (22 * browseLines));
                                }
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    }
                    break;

                case 3://INPUT DECODER
                    if (pad.Buttons & PSP_CTRL_SELECT) {
                        copyMenu = 1;
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }
                    if (pad.Buttons & PSP_CTRL_CROSS) {
                        extSelected[3] = !extSelected[3];
                        menuDraw();
                        sceKernelDelayThread(150000);
                    }

                    if (pad.Buttons & PSP_CTRL_LEFT) {
                        decodeX--;
                        switch (decodeC) {
                            case 0:
                                if ((signed) decodeX == -1) { decodeX = 0; }
                                break;
                            case 1:
                                if ((signed) decodeX == -1) {
                                    decodeX = 7;
                                    decodeC--;
                                }
                                break;
                        }
                        menuDraw();
                        if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                    } else if (pad.Buttons & PSP_CTRL_RIGHT) {
                        decodeX++;
                        switch (decodeC) {
                            case 0:
                                if (decodeX > 7) {
                                    decodeX = 0;
                                    decodeC++;
                                }
                                break;
                            case 1:
                                if (decodeX > 7) { decodeX = 7; }
                                break;
                        }
                        menuDraw();
                        if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                    } else {
                        cheatButtonAgeX = 0;
                    }

                    if (extSelected[3]) {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            switch (decodeC) {
                                case 0:
                                    if (decodeX == 7) {
                                        decodeAddress += 4;
                                    } else {
                                        decodeAddress += (1 << (4 * (7 - decodeX)));
                                    }
                                    if (decodeAddress < START_ADDRESS) {
                                        decodeAddress = START_ADDRESS;
                                    }
                                    if (decodeAddress > 0x49FFFFA8) {
                                        decodeAddress = 0x49FFFFA8;
                                    }
                                    break;
                                case 1:
                                    *((unsigned int *) (decodeAddress + (decodeY * 4))) += (1 << (4 * (7 - decodeX)));
                                    sceKernelDcacheWritebackInvalidateRange(
                                            ((unsigned int *) (decodeAddress + (decodeY * 4))), 4);
                                    sceKernelIcacheInvalidateRange(((unsigned int *) (decodeAddress + (decodeY * 4))),
                                                                   4);
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            switch (decodeC) {
                                case 0:
                                    if (decodeX == 7) {
                                        decodeAddress -= 4;
                                    } else {
                                        decodeAddress -= (1 << (4 * (7 - decodeX)));
                                    }
                                    if (decodeAddress < START_ADDRESS) {
                                        decodeAddress = START_ADDRESS;
                                    }
                                    if (decodeAddress > 0x49FFFFA8) {
                                        decodeAddress = 0x49FFFFA8;
                                    }
                                    break;
                                case 1:
                                    *((unsigned int *) (decodeAddress + (decodeY * 4))) -= (1 << (4 * (7 - decodeX)));
                                    sceKernelDcacheWritebackInvalidateRange(
                                            ((unsigned int *) (decodeAddress + (decodeY * 4))), 4);
                                    sceKernelIcacheInvalidateRange(((unsigned int *) (decodeAddress + (decodeY * 4))),
                                                                   4);
                                    break;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    } else if (pad.Buttons & PSP_CTRL_SQUARE) {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            if (decodeAddress > START_ADDRESS) {
                                decodeAddress -= 4;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            if (decodeAddress < 0x49FFFFA8) {
                                decodeAddress += 4;
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                        if ((abs((signed) (pad.Ly - 127)) - 40) > 0) {
                            decodeAddress += (((signed) (pad.Ly - 127)) / browseLines) * browseLines;
                            if (decodeAddress < START_ADDRESS) {
                                decodeAddress = START_ADDRESS;
                            }
                            if (decodeAddress > 0x49FFFFA8) {
                                decodeAddress = 0x49FFFFA8;
                            }
                            menuDraw();
                            sceKernelDelayThread(18750);
                        }
                    } else {
                        if (pad.Buttons & PSP_CTRL_UP) {
                            if (decodeY > 0) {
                                decodeY--;
                            } else if (decodeY == 0) {
                                if (decodeAddress > START_ADDRESS) {
                                    decodeAddress -= 4;
                                }
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else if (pad.Buttons & PSP_CTRL_DOWN) {
                            if (decodeY < 21) {
                                decodeY++;
                            } else if (decodeY == 21) {
                                if (decodeAddress < 0x49FFFFA8) {
                                    decodeAddress += 4;
                                }
                            }
                            menuDraw();
                            if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                            sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                        } else {
                            cheatButtonAgeY = 0;
                        }
                    }
                    break;

                case 4://INPUT TOOLS
                    continue;

                case 5://INPUT SETTINGS
                    if (pad.Buttons & PSP_CTRL_UP) {
                        if (cheatSelected > 0) {
                            cheatSelected--;
                        } else if (cheatSelected == 0) {
                            cheatSelected = 11;
                        }
                        menuDraw();
                        if (cheatButtonAgeY < 13) cheatButtonAgeY++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                    } else if (pad.Buttons & PSP_CTRL_DOWN) {
                        if (cheatSelected < 11) {
                            cheatSelected++;
                        } else if (cheatSelected == 11) {
                            cheatSelected = 0;
                        }
                        menuDraw();
                        if (cheatButtonAgeY < 12) cheatButtonAgeY++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeY));
                    } else {
                        cheatButtonAgeY = 0;
                    }
                    if (pad.Buttons & PSP_CTRL_LEFT) {
                        if ((cheatSelected == 8) && (cheatHz > 0)) {
                            cheatHz -= 15625;
                        }
                        if ((cheatSelected == 3) && (dumpNo > 0)) {
                            dumpNo--;
                        }
                        if ((cheatSelected == 2) && (dumpNo > 0)) {
                            dumpNo--;
                        }
                        menuDraw();
                        if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                    } else if (pad.Buttons & PSP_CTRL_RIGHT) {
                        if (cheatSelected == 8) {
                            cheatHz += 15625;
                        }
                        if (cheatSelected == 3) {
                            dumpNo++;
                        }
                        if (cheatSelected == 2) {
                            dumpNo++;
                        }
                        menuDraw();
                        if (cheatButtonAgeX < 12) cheatButtonAgeX++;
                        sceKernelDelayThread(150000 - (10000 * cheatButtonAgeX));
                    } else {
                        cheatButtonAgeX = 0;
                    }
                    if (pad.Buttons & PSP_CTRL_CROSS) {
                        if (cheatSelected == 0) {
                            cheatPause = !cheatPause;
                            menuDraw();

                            if (cheatPause) {
                                gamePause(thid);
                            } else {
                                gameResume(thid);
                            }
                        } else if (cheatSelected == 1) {
                            cheatNew(4, 0x4000, 0);

                            //Switch to the cheat editor
                            pspDebugScreenInitEx(vram, 0, 0);
                            extSelected[1] = extSelected[2] = extSelected[3] = 0;
                            extSelected[0] = cheat[cheatTotal - 1].block;
                            cheatSelected = cheatTotal - 1;
                            extMenu = 1;
                            extOpt = 1;
                            menuDraw();
                            sceKernelDelayThread(150000);
                        } else if (cheatSelected == 2) {
                            sprintf(buffer, "ms0:/dump%d.ram", dumpNo);

                            fd = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
                            if (fd > 0) {
                                counter = 0;
                                while (counter < cheatTotal) {
                                    scounter = cheat[counter].block;
                                    cheatDMA = 0;
                                    while (scounter < cheat[counter].block + cheat[counter].len) {
                                        if (block[scounter].flags & FLAG_DMA) {
                                            if (block[scounter].hakVal != 0xFFFFFFFF) {
                                                sceIoLseek(fd, block[scounter].hakVal, SEEK_SET);
                                                sceIoRead(fd, &cheatDMA, sizeof(unsigned int));
                                                block[scounter].stdVal = cheatDMA;
                                            } else {
                                                cheatDMA = 0;
                                            }
                                        } else {
                                            sceIoLseek(fd, (cheatDMA + block[scounter].address) - 0x08800000, SEEK_SET);
                                            switch (block[scounter].flags & FLAG_DWORD) {
                                                case FLAG_DWORD:
                                                    sceIoRead(fd, &block[scounter].stdVal, sizeof(unsigned int));
                                                    break;
                                                case FLAG_WORD:
                                                    sceIoRead(fd, &block[scounter].stdVal, sizeof(unsigned short));
                                                    break;
                                                case FLAG_BYTE:
                                                    sceIoRead(fd, &block[scounter].stdVal, sizeof(unsigned char));
                                                    break;
                                            }
                                        }

                                        scounter++;
                                    }

                                    cheatDisable(counter);
                                    counter++;
                                }
                                sceIoClose(fd);

                                cheatStatus = 0;
                            } else {
                                lineClear(33);
                                pspDebugScreenSetTextColor(0xFF0000FF);
                                pspDebugScreenPuts("ERROR: Selected RAM Dump # does not exist!");
                                sceKernelDelayThread(3000000);
                            }

                            menuDraw();
                        } else if (cheatSelected == 3) {
                            sprintf(buffer, "ms0:/dump%d.ram", dumpNo);

                            fd = sceIoOpen(buffer, PSP_O_WRONLY | PSP_O_CREAT, 0777);
                            if (fd > 0) {
                                if (sceIoWrite(fd, (void *) 0x08800000, 0x1800000) == 0x1800000) {
                                    sceIoClose(fd);

                                    dumpNo++;
                                } else {
                                    sceIoClose(fd);

                                    sceIoRemove(buffer);

                                    lineClear(33);
                                    pspDebugScreenSetTextColor(0xFF0000FF);
                                    pspDebugScreenPuts("ERROR: MemoryStick out of Space!");
                                    sceKernelDelayThread(3000000);
                                }
                            }
                            menuDraw();
                            sceKernelDelayThread(150000);//Delay twice
                        } else if (cheatSelected == 4) {
                            if (browseLines == 8) {
                                browseLines = 16;
                            } else {
                                browseLines = 8;
                            }
                            browseC = 0;
                            browseX = 0;
                            browseY = 0;
                            if (browseAddress < START_ADDRESS) {
                                browseAddress = START_ADDRESS;
                            }
                            if (browseAddress > (END_ADDRESS - (22 * browseLines))) {
                                browseAddress = (END_ADDRESS - (22 * browseLines));
                            }
                            menuDraw();
                        } else if (cheatSelected == 5) {
                            browseFormat = (browseFormat == 0x48800000) ? 0x40000000 : 0x48800000;
                            menuDraw();
                        } else if (cheatSelected == 6) {
                            decodeFormat = (decodeFormat == 0x48800000) ? 0x40000000 : 0x48800000;
                            menuDraw();
                        } else if (cheatSelected == 7) {
                            if (trackFormat == 0x48800000) {
                                trackFormat = 0x40000000;
                            } else {
                                trackFormat = 0x48800000;
                            }
                            menuDraw();
                        } else if (cheatSelected == 9) {
                            cheatSave();
                            menuDraw();
                            sceKernelDelayThread(150000);//Delay twice
                        } else if (cheatSelected == 10) {
                            sceUsbGetState() & PSP_USB_ACTIVATED ? stop_usb() : start_usb();
                            menuDraw();
                        } else if (cheatSelected == 11) {
                            restartGame;
                            menuDraw();
                        }
                        sceKernelDelayThread(150000);
                    }
                    break;
            }
        }

        //Stop the game from freezing up
        if (!cheatPause) sceKernelDelayThread(15625);
    }
}

Hook hookB[1] =
        {
                {{0, NULL}, "sceOpenPSID_Service", "sceOpenPSID_driver", 0xc69bebce, NULL},
};

char psid[16];

int sceOpenPSIDGetOpenPSID(char *openpsid) {
    memcpy(openpsid, psid, 16);
    return 0;
}

static const unsigned char patchA[] = {0x21, 0x88, 0x02, 0x3c,//lui v0, $8822
                                       0x21, 0x10, 0x42, 0x34,//ori v0, v0, $0008
                                       0x08, 0x00, 0x40, 0x00,//jr v0
                                       0x00, 0x00, 0x00, 0x00};

#include "headers/screenshot.h"

int mainThread() {
    signed int fd;
    running = 1;
    unsigned int counter = 0;

    //Clear the search history to 0
    memset(&searchHistory, 0, sizeof(Block) * 15);
    searchHistory[0].flags |= FLAG_DWORD;
    searchHistory[1].flags |= FLAG_DWORD;
    searchHistory[2].flags |= FLAG_DWORD;
    searchHistory[3].flags |= FLAG_DWORD;
    searchHistory[4].flags |= FLAG_DWORD;
    searchHistory[5].flags |= FLAG_DWORD;
    searchHistory[6].flags |= FLAG_DWORD;
    searchHistory[7].flags |= FLAG_DWORD;
    searchHistory[8].flags |= FLAG_DWORD;
    searchHistory[9].flags |= FLAG_DWORD;
    searchHistory[10].flags |= FLAG_DWORD;
    searchHistory[11].flags |= FLAG_DWORD;
    searchHistory[12].flags |= FLAG_DWORD;
    searchHistory[13].flags |= FLAG_DWORD;
    searchHistory[14].flags |= FLAG_DWORD;
    searchHistory[15].flags |= FLAG_DWORD;

    //Wait for the kernel to boot
    sceKernelDelayThread(100000);
    while (!sceKernelFindModuleByName("sceKernelLibrary"))
        sceKernelDelayThread(100000);
    sceKernelDelayThread(100000);

    //Find which screen directory to use
    fd = sceIoDopen("ms0:/PICTURE/");
    if (fd > 0) {
        sceIoDclose(fd);

        strcpy(screenPath, "ms0:/PICTURE/screen%d.bmp");
    } else {
        fd = sceIoDopen("ms0:/PSP/PHOTO/");
        if (fd > 0) {
            sceIoDclose(fd);

            strcpy(screenPath, "ms0:/PSP/PHOTO/screen%d.bmp");
        } else {
            if (!sceIoMkdir("ms0:/PICTURE", 0777)) {
                strcpy(screenPath, "ms0:/PICTURE/screen%d.bmp");
            } else {
                screenPath[0] = 0;
            }
        }
    }

    //Set the correct screen number
    screenNo = 0;
    if (screenPath[0] != 0) {
        while (1) {
            sprintf(buffer, screenPath, screenNo);
            fd = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
            if (fd > 0) {
                sceIoClose(fd);
                screenNo++;
            } else {
                break;
            }
        }
    }

    {
        //Generate the patch
        *((unsigned short *) (&patchA[0])) = (unsigned short) (((unsigned int) sceOpenPSIDGetOpenPSID) >> 16);
        *((unsigned short *) (&patchA[4])) = (unsigned short) ((unsigned int) &sceOpenPSIDGetOpenPSID);

        //Find the function we want!
        while (1) {
            int mod = sceKernelFindModuleByName(hookB[0].modname);
            if (mod == NULL) {
                sceKernelDelayThread(100);
                continue;
            }
            break;
        }
        unsigned int hookAddress = moduleFindFunc(
                moduleFindLibrary(sceKernelSearchModuleByName(hookB[0].modname), hookB[0].libname), hookB[0].nid);
        hookAddress = *((unsigned int *) hookAddress);

        //Open the PSID file
        fd = sceIoOpen("ms0:/seplugins/cheatER/cheatERimportant.bin", PSP_O_RDONLY, 0777);
        if (fd > 0) {
            sceIoRead(fd, psid, 16);
            sceIoClose(fd);
        } else {
            fd = sceIoOpen("ms0:/seplugins/cheatER/cheatERimportant.bin", PSP_O_WRONLY | PSP_O_CREAT, 0777);
            if (fd > 0) {
                ((int (*)(char *)) hookAddress)(psid);
                sceIoWrite(fd, psid, 16);
                sceIoClose(fd);
            } else {
                goto skipPatch;
            }
        }

        //Apply the memcpy
        sceKernelDcacheWritebackAll();
        sceKernelIcacheInvalidateAll();

        //Apply the patch
        memcpy(hookAddress, patchA, 16);

        //Apply the memcpy
        sceKernelDcacheWritebackAll();
        sceKernelIcacheInvalidateAll();
    }
skipPatch://Skip the evil patch

    //Find the GAME ID
    do {
        fd = sceIoOpen("disc0:/UMD_DATA.BIN", PSP_O_RDONLY, 0777);
        sceKernelDelayThread(1000);
    } while (fd <= 0);
    sceIoRead(fd, gameId, 10);
    sceIoClose(fd);
    memcpy(&gameDir[22], gameId, 10);

    //Load the cheats
    fd = sceIoOpen(gameDir, PSP_O_RDONLY, 0777);
    if (fd > 0) {
        unsigned int fileSize = sceIoLseek(fd, 0, SEEK_END);
        sceIoLseek(fd, 0, SEEK_SET);
        unsigned int fileOffset = 0;
        unsigned char commentMode = 0;
        unsigned char nameMode = 0;

        while (fileOffset < fileSize) {
            sceKernelDelayThread(1500);

            sceIoRead(fd, &buffer[0], 1);

            if ((buffer[0] == '\r') || (buffer[0] == '\n')) {
                commentMode = 0;
                if (nameMode) {
                    cheatTotal++;
                    nameMode = 0;
                }
            } else if ((buffer[0] == ' ') && (!nameMode)) {
            } else if (buffer[0] == ';') {
                commentMode = 1;
                if (nameMode) {
                    cheatTotal++;
                    nameMode = 0;
                }
            }//Skip comments till next line
            else if (buffer[0] == '#')//Read in the cheat name
            {
                if (cheatTotal >= BLOCK_MAX) { break; }
                cheat[cheatTotal].block = blockTotal;
                cheat[cheatTotal].flags = 0;
                cheat[cheatTotal].len = 0;
                cheat[cheatTotal].name[0] = 0;
                nameMode = 1;
            } else if ((buffer[0] == '!') && (nameMode)) {
                //Cheat's selected by default
                if (cheat[cheatTotal].flags & FLAG_SELECTED)//Two ! = selected for constant on status
                {
                    cheat[cheatTotal].flags |= FLAG_CONSTANT;
                    cheat[cheatTotal].flags &= ~FLAG_SELECTED;
                } else//One ! = selected for music on/off button
                {
                    cheat[cheatTotal].flags |= FLAG_SELECTED;
                }
            } else if ((!commentMode) && (nameMode)) {
                if (nameMode < 32)//1 to 31 = letters, 32=Null terminator
                {
                    cheat[cheatTotal].name[nameMode - 1] = buffer[0];
                    nameMode++;
                    cheat[cheatTotal].name[nameMode - 1] = 0;
                }
            } else if ((!commentMode) && (!nameMode)) {
                //Add 0xAABBCCDD 0xAABBCCDD block
                if (!blockAdd(fd, buffer)) {
                    //No more RAM?
                    if (cheatTotal != 0) {
                        cheatTotal--;
                        break;
                    }
                }
                if (cheatTotal != 0) {
                    cheat[cheatTotal - 1].len++;
                }
            }

            fileOffset = sceIoLseek(fd, 0, SEEK_CUR);
        }
        sceIoClose(fd);
    }

    //Set the VRAM to null, use the current screen
    pspDebugScreenInitEx(0x44000000, 0, 0);
    vram = NULL;

    //Setup the controller
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    //Register the button callbacks
    sceCtrlRegisterButtonCallback(3, triggerKey | menuKey | screenKey, buttonCallback, NULL);

    //Do the loop-de-loop
    while (running) {
        if (vram == NULL) {
            //Has the HOME button been pressed?
            unsigned int a_address = 0;
            unsigned int a_bufferWidth = 0;
            unsigned int a_pixelFormat = 0;
            unsigned int a_sync;

            sceDisplayGetFrameBufferInternal(0, &a_address, &a_bufferWidth, &a_pixelFormat, &a_sync);

            if (a_address) {
                vram = (void *) (0xA0000000 | a_address);
            } else {
                sceDisplayGetMode(&a_pixelFormat, &a_bufferWidth, &a_bufferWidth);
                pspDebugScreenSetColorMode(a_pixelFormat);
                pspDebugScreenSetXY(0, 0);
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPuts("cheatER: Double tap the home button and press vol + & vol - to open menu\n");
            }

            sceKernelDelayThread(1500);
            continue;
        }

        //Handle menu
        if (menuDrawn) {
            //Stop the game from receiving input (USER mode input block)
            sceCtrlSetButtonMasks(0xFFFF, 1); // Mask lower 16bits
            sceCtrlSetButtonMasks(0x10000, 2);// Always return HOME key

            //Setup a custom VRAM
            sceDisplaySetFrameBufferInternal(0, vram, 512, 0, 1);
            pspDebugScreenInitEx(vram, 0, 0);

            //Draw menu
            if (cheatPause) gamePause(thid);
            menuInput();
            gameResume(thid);

            //Return the standard VRAM
            sceDisplaySetFrameBufferInternal(0, 0, 512, 0, 1);

            //Allow the game to receive input
            sceCtrlSetButtonMasks(0x10000, 0);// Unset HOME key
            sceCtrlSetButtonMasks(0xFFFF, 0); // Unset mask
        } else if ((cheatHz != 0) || (cheatFlash)) {
            //Apply cheats
            cheatApply(-1);
            if (cheatFlash > 0) cheatFlash--;
        }

        //Handle screenshot
        if ((screenTime) && (screenPath[0])) {
            screenTime = 0;
            void *block_addr;
            void *frame_addr;
            int frame_width;
            int pixel_format;
            int sync = 1;
            u32 p;

            while (1) {
                sprintf(buffer, screenPath, screenNo);
                fd = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
                if (fd > 0) {
                    sceIoClose(fd);
                    screenNo++;
                } else {
                    break;
                }
            }

            if ((sceDisplayGetFrameBufferInternal(2, &frame_addr, &frame_width, &pixel_format, &sync) < 0) ||
                (frame_addr == NULL)) {
            } else {
                p = (u32) frame_addr;
                if (p & 0x80000000) {
                    p |= 0xA0000000;
                } else {
                    p |= 0x40000000;
                }

                gamePause(thid);
                bitmapWrite((void *) p, NULL, pixel_format, buffer);
                gameResume(thid);

                screenNo++;
            }
        }

        //Wait a certain amount of seconds before reapplying cheats again
        sceKernelDelayThread(!cheatHz ? 500000 : cheatHz);
    }
    return 0;
}

int _start(SceSize args, void *argp) {
    //Load the CFG
    if (cfg[4]) {
        SceModule *mod;
        while (1) {
            mod = sceKernelFindModuleByName(hookA[0].modname);
            if (mod == NULL) {
                sceKernelDelayThread(100);
                continue;
            }
            break;
        }
        moduleHookFunc(&hookA[0].modfunc, sceKernelSearchModuleByName(hookA[0].modname), hookA[0].libname, hookA[0].nid,
                       hookA[0].func);
    }
    memcpy(&triggerKey, cfg + 11, 4);
    memcpy(&menuKey, cfg + 15, 4);
    memcpy(&screenKey, cfg + 20, 4);

    //Create thread
    sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, thread_buf_start, MAX_THREAD, &thread_count_start);
    thid = sceKernelCreateThread("cheatERThread", &mainThread, 0x18, 0x1000, 0, NULL);

    //Start thread
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);

    return 0;
}

int _stop(SceSize args, void *argp) {
    running = 0;
    sceKernelTerminateThread(thid);
    return 0;
}
