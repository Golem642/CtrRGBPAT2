#include <stdio.h>  // Basic functions
#include <stdlib.h> // malloc and free
#include <string.h> // String manipulation
#include <dirent.h> // For opendir() and readdir()
#include <unistd.h> // rmdir()
#include <fstream>
#include <sstream>
#include <3ds.h>    // Main 3ds lib (libctru)
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

// TODO:
// - Save/restore
// - Separate settings for awake/asleep
// - Reset only one LED to default
// - Find how to change the "low battery" notification
// - Instant preview ?
// - NFC color receive ?
// - Proper UI ?

typedef struct {
    uint8_t r[32];
    uint8_t g[32];
    uint8_t b[32];
} LED;

typedef struct {
    uint8_t ani[4]; // animation settings : https://www.3dbrew.org/wiki/MCURTC:SetInfoLEDPattern

    // colors
    uint8_t r[32];
    uint8_t g[32];
    uint8_t b[32];
} LED_MCU;

typedef struct {
    uint8_t seq[4];
} LowBatPat;

const char* mainMenu[8] = {
    "Pattern editor",
    "Animation settings",
    "Test animation",
    "Install animation",
    "Load a pattern",
    "Toggle patch",
    "Low battery settings",
    "Shutdown 3DS"
};

const char* settingsMenu[4] = {
    "Change animation speed (delay)",
    "Change animation smoothness",
    "Change loop delay (FF = no loop)",
    "Change blink speed (unused ?)"
};

const char* installMenu[5] = {
    "Boot & Wake up",
    "SpotPass",
    "StreetPass",
    "Join friend game ???",
    "Friend online"
};

const char* installModeMenu[2] = {
    "While in sleep mode",
    "While awake"
};

const char* loadMenu[2] = {
    "Saved patterns (not implemented)",
    "Presets"
};

const char* loadPresetMenu[4] = {
    "Blink",
    "Explode",
    "Static",
    "Rainbow"
};

int currMenu = 0;
int selected = 0;

int nbMain = 8;
int nbSettings = 4;
int nbInstall = 5;
int nbInstallMode = 2;
int nbLoad = 2;
int nbLoadPreset = 4;
int nbEditor = 32;
int nbLowBat = 8;

int nbCmd = nbMain;

int Xtsize = 50;
int Xbsize = 40;
int Ysize = 29;

char keybInput[7] = "";

// defaults
char color_HEX[] = "2200ff"; // Originally 910b0b
char color_copy[10] = "000000"; // 10 to avoid gcc compiling warnings

// LIST OF COOL COLORS I FOUND (because i can)
// - 2200ff : nice purple (default for this app)
// - 910b0b : pink/beige (default for CtrRGBPATTY)
// - 123456 : cool aqua green (could be better but i like it)
// - 1100ff : might be even purpler ? idk
// - 0022ff : light blue
// - i forgor

char anim_speed[] = "2f";
char anim_smooth[] = "5f";
char anim_loop_delay[] = "ff";
char anim_blink_speed[] = "00";

// DEFAULT 3DS SETTINGS :
// - BOOT / WAKE UP : 0x50, 0x50, 0xFF, 0x00
// - SPOTPASS : 0x50, 0x3C, 0xFF, 0x00
// - STREETPASS : 0x50, 0x50, 0xFF, 0x00
// - JOIN FRIEND GAME ??? : 0x68, 0x68, 0xFF, 0x00 (strange friend notification type but different)
// - FRIENDS : 0x50, 0x3C, 0xFF, 0x00
// - LOW BATTERY : 0x55, 0x55, 0x55, 0x55  <- not actual settings, more like the sequence of the LED
uint8_t ANIMDELAY = 0x2F; // 0x50
uint8_t ANIMSMOOTH = 0x5F; // 0x3c
uint8_t LOOPBYTE = 0xFF; // no loop
uint8_t BLINKSPEED = 0x00; // https://www.3dbrew.org/wiki/MCURTC:SetInfoLEDPattern

LowBatPat LBP;

int lbpTemp[32];

int selectedPreset;
int installType;
int installMode;

bool enabled;

bool debugMode = false;

LED customLed;

PrintConsole topscreen, bottomscreen;

static SwkbdState swkbd;
static SwkbdStatusData swkbdStatus;
static SwkbdLearningData swkbdLearning;

static SwkbdCallbackResult hexadecimalCheck(void* user, const char** ppMessage, const char* text, size_t textlen) {
    //char asciiNum[100];
    for (int i = 0; i < (int)textlen; i++) {
        //printf("Char %d is %d\n", i, (int)text[i]);
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'A' && text[i] <= 'F') || (text[i] >= 'a' && text[i] <= 'f'))) {
            *ppMessage = "The text must be in\nhexadecimal";
            return SWKBD_CALLBACK_CONTINUE;
        }
    }
    return SWKBD_CALLBACK_OK;
}

void hexaInput(char* hexaText, int hexaLen, const char* hintText) {
    swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, hexaLen);
    swkbdSetValidation(&swkbd, SWKBD_FIXEDLEN, 0, 0);
    swkbdSetFilterCallback(&swkbd, hexadecimalCheck, NULL);
    swkbdSetFeatures(&swkbd, SWKBD_FIXED_WIDTH);
    swkbdSetInitialText(&swkbd, hexaText);
    swkbdSetHintText(&swkbd, hintText);
    swkbdSetStatusData(&swkbd, &swkbdStatus, false, true);
    swkbdSetLearningData(&swkbd, &swkbdLearning, false, true);
    swkbdInputText(&swkbd, (char*)keybInput, hexaLen + 1);

    if (strcmp(keybInput, "") != 0) {
        strcpy(hexaText, keybInput);
    }
}

int fcopy(const char* source, const char* dest) {
    FILE *f1 = fopen(source, "rb");
    if (f1 == NULL) {
        printf("Cannot open source file\n");
        return 1;
    }
    FILE *f2 = fopen(dest, "wb+");
    if (f2 == NULL) {
        printf("Cannot open destination file\n");
        return 1;
    }
    size_t n, m;
    char buff;
    m = 0;
    do {
        n = fread(&buff, 1, sizeof(buff), f1);
        if (!feof(f1)) m = fwrite(&buff, 1, n, f2);
        else m = 0;
    } while (!feof(f1));
    if (m) {
        perror("Error while copying");
        return 1;
    }
    if (fclose(f2)) {
        perror("Error while closing output file");
        return 1;
    }
    if (fclose(f1)) {
        perror("Error while closing input file");
        return 1;
    }
    return 0;
}

void printAt(int x, int y, const char* text) {
    printf("\x1b[%d;%dH%s", y, x, text);
}

void intRGB(std::string hexCode, int *r, int *g, int *b)
{
    // Remove the hashtag ...
    if(hexCode.at(0) == '#') 
    {
        hexCode = hexCode.erase(0, 1);
    }

    // ... and extract the rgb values.
    std::istringstream(hexCode.substr(0,2)) >> std::hex >> *r;
    std::istringstream(hexCode.substr(2,2)) >> std::hex >> *g;
    std::istringstream(hexCode.substr(4,2)) >> std::hex >> *b;
}

/* Documentation:
    LED struct has RGB patterns for 32 itterations. With this you can make an animation with the LED. (like the one MCU BRICKER does).
*/

void createLED(LED* pattern, std::string hexCode, int selcpat)
{
    // LED datastruct we will be returning
    int r, g, b;

    // Remove the hashtag ...
    if(hexCode.at(0) == '#') 
    {
        hexCode = hexCode.erase(0, 1);
    }

    // ... and extract the rgb values.
    std::istringstream(hexCode.substr(0,2)) >> std::hex >> r;
    std::istringstream(hexCode.substr(2,2)) >> std::hex >> g;
    std::istringstream(hexCode.substr(4,2)) >> std::hex >> b;
    
    if (debugMode) {
        printf("(%d, %d, %d)\n", r, g ,b);
    }
    printf("Writing to RGB struct...\n");

    memset(&pattern->r[0], 0, 32);
    memset(&pattern->g[0], 0, 32);
    memset(&pattern->b[0], 0, 32);

    switch(selcpat)
    {
        case 0: // Blink
            for (int i = 1; i<31; i+=10)
            {
                memset(&pattern->r[i], r, 5); 
                memset(&pattern->g[i], g, 5); 
                memset(&pattern->b[i], b, 5);
            }
            //pattern->r[31] = r;
            //pattern->g[31] = g;
            //pattern->b[31] = b;
        break;
        case 1: // Explode
            for (int i = 1; i<31; i+=10)
            {
                pattern->r[i] = r/i;
                pattern->g[i] = g/i;
                pattern->b[i] = b/i;
            }
            //pattern->r[31] = r;
            //pattern->g[31] = g;
            //pattern->b[31] = b;
        break;

        case 2: // Static
            memset(&pattern->r[0], r, 31); 
            memset(&pattern->g[0], g, 31); 
            memset(&pattern->b[0], b, 31);
        break;

        case 3: // Rainbow (AKA MCU bricker lol)
            pattern->r[0] = 128;
            pattern->r[1] = 103;
            pattern->r[2] = 79;
            pattern->r[3] = 57;
            pattern->r[4] = 38;
            pattern->r[5] = 22;
            pattern->r[6] = 11;
            pattern->r[7] = 3;
            pattern->r[8] = 1;
            pattern->r[9] = 3;
            pattern->r[10] = 11;
            pattern->r[11] = 22;
            pattern->r[12] = 38;
            pattern->r[13] = 57;
            pattern->r[14] = 79;
            pattern->r[15] = 103;
            pattern->r[16] = 128;
            pattern->r[17] = 153;
            pattern->r[18] = 177;
            pattern->r[19] = 199;
            pattern->r[20] = 218;
            pattern->r[21] = 234;
            pattern->r[22] = 245;
            pattern->r[23] = 253;
            pattern->r[24] = 255;
            pattern->r[25] = 253;
            pattern->r[26] = 245;
            pattern->r[27] = 234;
            pattern->r[28] = 218;
            pattern->r[29] = 199;
            pattern->r[30] = 177;
            //pattern->r[31] = r;
            pattern->g[0] = 238;
            pattern->g[1] = 248;
            pattern->g[2] = 254;
            pattern->g[3] = 255;
            pattern->g[4] = 251;
            pattern->g[5] = 242;
            pattern->g[6] = 229;
            pattern->g[7] = 212;
            pattern->g[8] = 192;
            pattern->g[9] = 169;
            pattern->g[10] = 145;
            pattern->g[11] = 120;
            pattern->g[12] = 95;
            pattern->g[13] = 72;
            pattern->g[14] = 51;
            pattern->g[15] = 33;
            pattern->g[16] = 18;
            pattern->g[17] = 8;
            pattern->g[18] = 2;
            pattern->g[19] = 1;
            pattern->g[20] = 5;
            pattern->g[21] = 14;
            pattern->g[22] = 27;
            pattern->g[23] = 44;
            pattern->g[24] = 65;
            pattern->g[25] = 87;
            pattern->g[26] = 111;
            pattern->g[27] = 136;
            pattern->g[28] = 161;
            pattern->g[29] = 184;
            pattern->g[30] = 205;
            //pattern->g[31] = g;
            pattern->b[0] = 18;
            pattern->b[1] = 33;
            pattern->b[2] = 51;
            pattern->b[3] = 72;
            pattern->b[4] = 95;
            pattern->b[5] = 120;
            pattern->b[6] = 145;
            pattern->b[7] = 169;
            pattern->b[8] = 192;
            pattern->b[9] = 212;
            pattern->b[10] = 229;
            pattern->b[11] = 242;
            pattern->b[12] = 251;
            pattern->b[13] = 255;
            pattern->b[14] = 254;
            pattern->b[15] = 248;
            pattern->b[16] = 238;
            pattern->b[17] = 223;
            pattern->b[18] = 205;
            pattern->b[19] = 184;
            pattern->b[20] = 161;
            pattern->b[21] = 136;
            pattern->b[22] = 111;
            pattern->b[23] = 87;
            pattern->b[24] = 64;
            pattern->b[25] = 44;
            pattern->b[26] = 27;
            pattern->b[27] = 14;
            pattern->b[28] = 5;
            pattern->b[29] = 1;
            pattern->b[30] = 2;
            //pattern->b[31] = b;
        break;

        case 4: // Custom
            for (int i = 0; i < 32; i++) {
                pattern->r[i] = customLed.r[i];
                pattern->g[i] = customLed.g[i];
                pattern->b[i] = customLed.b[i];
            }
        break;
    }
}

/*
void restoreConfig(LED_MCU* led) {
    // Select from the installMenu list
}
*/

void writeDefault(FILE* file) {
    
    printf("Writing default configuration...\n");

    LED_MCU defaultNotifs[nbInstall];

    LED_MCU temp;

    // Boot / wake up LED
    temp.ani[0] = 0x50;
    temp.ani[1] = 0x50;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    for (int i = 0; i<32; i++)
    {
        temp.r[i] = 0x00;
        temp.g[i] = 0x00;
        temp.b[i] = 0x00;
    }
    defaultNotifs[0] = temp;

    // SpotPass LED
    temp.ani[0] = 0x50;
    temp.ani[1] = 0x3C;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    for (int i = 0; i<30; i = i + 6)
    {
        temp.r[i] = 0x00;
        temp.r[i+1] = 0x00;
        temp.r[i+2] = 0x00;
        temp.r[i+3] = 0x00;
        temp.r[i+4] = 0x00;
        temp.r[i+5] = 0x00;

        temp.g[i] = 0x00;
        temp.g[i+1] = 0x00;
        temp.g[i+2] = 0x00;
        temp.g[i+3] = 0x00;
        temp.g[i+4] = 0x00;
        temp.g[i+5] = 0x00;

        temp.b[i] = 0x00;
        temp.b[i+1] = 0xFF;
        temp.b[i+2] = 0xF0;
        temp.b[i+3] = 0xDE;
        temp.b[i+4] = 0xBF;
        temp.b[i+5] = 0x8C;
    }
    temp.r[30] = 0x00;
    temp.g[30] = 0x00;
    temp.b[30] = 0x00;

    temp.r[31] = 0x00;
    temp.g[31] = 0x00;
    temp.b[31] = 0x66;

    defaultNotifs[1] = temp;

    // StreetPass LED
    temp.ani[0] = 0x50;
    temp.ani[1] = 0x50;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    for (int i = 0; i<30; i = i + 6)
    {
        for (int j = 0; j < 4; j = j+2) {
            temp.r[i+j] = 0x00;
            temp.g[i+j] = 0x00;
            temp.b[i+j] = 0x00;

            temp.r[i+j+1] = 0x00;
            temp.g[i+j+1] = 0x80;
            temp.b[i+j+1] = 0x00;
        }
        
        for (int j = 4; j < 6; j++) {
            temp.r[i+j] = 0x00;
            temp.g[i+j] = 0x00;
            temp.b[i+j] = 0x00;
        }
    }
    temp.r[30] = 0x00;
    temp.g[30] = 0x00;
    temp.b[30] = 0x00;

    temp.r[31] = 0x00;
    temp.g[31] = 0x33;
    temp.b[31] = 0x00;

    defaultNotifs[2] = temp;

    // Join friend game ??? LED
    temp.ani[0] = 0x68;
    temp.ani[1] = 0x68;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    for (int i = 0; i<24; i = i + 8)
    {
        for (int j = 0; j < 6; j = j + 2) {
            temp.r[i+j] = 0x00;
            temp.g[i+j] = 0x00;
            temp.b[i+j] = 0x00;

            temp.r[i+j+1] = 0xA9;
            temp.g[i+j+1] = 0x24;
            temp.b[i+j+1] = 0x00;
        }

        for (int j = 6; j<8; j++) {
            temp.r[i+j] = 0x00;
            temp.g[i+j] = 0x00;
            temp.b[i+j] = 0x00;
        }
    }
    for (int i = 24; i < 32; i++) {
        temp.r[i] = 0x00;
        temp.g[i] = 0x00;
        temp.b[i] = 0x00;
    }

    defaultNotifs[3] = temp;
    
    // Friends LED
    temp.ani[0] = 0x50;
    temp.ani[1] = 0x3C;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    
    for (int i = 0; i < 30; i = i + 3) {
        temp.r[i] = 0x00;
        temp.g[i] = 0x00;
        temp.b[i] = 0x00;

        temp.r[i+1] = 0xA9;
        temp.g[i+1] = 0x24;
        temp.b[i+1] = 0x00;

        temp.r[i+2] = 0x7E;
        temp.g[i+2] = 0x1B;
        temp.b[i+2] = 0x00;
    }

    for (int i = 30; i<32; i++) {
        temp.r[i] = 0x00;
        temp.g[i] = 0x00;
        temp.b[i] = 0x00;
    }

    defaultNotifs[4] = temp;


    // https://zerosoft.zophar.net/ips.php for documentation of the IPS file format
    // HEADER (5 BYTES)
    fwrite("PATCH", 5, 1, file);

    // OFFSET (3 BYTES)
    fputc(0x00, file);
    fputc(0xA0, file);
    fputc(0xC8, file);

    // SIZE (2 BYTES)
    fputc(0x03, file);
    fputc(0xE8, file); //  1000 BYTES

    // PATCH
    for (int i = 0; i < nbInstall; i++) {

        // DATA (200 BYTES)

        // This part applies for when the console is in sleep mode (to have the LED holding the color)
        // 100 BYTES
        fwrite(&defaultNotifs[i], sizeof(defaultNotifs[i]), 1, file);

        // This part applies for when the console is awake (to avoid having the LED holding the color)
        defaultNotifs[i].r[31] = 0x00;
        defaultNotifs[i].g[31] = 0x00;
        defaultNotifs[i].b[31] = 0x00;

        // 100 BYTES
        fwrite(&defaultNotifs[i], sizeof(defaultNotifs[i]), 1, file);

    }
    // END OF PATCH
    // EOF MARKER (3 BYTES)
    fwrite("EOF", 3, 1, file);
}

void writepatch(LED note, int selectedType = 0, int selectedStatus = 0)
{
    printf("Making directories...\n");
    mkdir("/CtrRGBPAT2", 0777);
    mkdir("/luma", 0777);
    mkdir("/luma/titles", 0777);
    mkdir("/luma/titles/0004013000003502", 0777);
    mkdir("/luma/sysmodules", 0777);

    DIR* dir = opendir("/luma/titles/0004013000003502"); // ! CHANGED IN LAST VERSION TO GO TO /luma/sysmodules !
    DIR* dir2 = opendir("/luma/sysmodules");
    DIR* dir3 = opendir("/CtrRGBPAT2");
    if (dir && dir2 && dir3)
    {
        // was copied/pasted from https://github.com/Pirater12/CustomRGBPattern/blob/master/main.c and then edited
        printf("Writing IPS patch file...\n");

        FILE *file;
        if (!(access("/CtrRGBPAT2/0004013000003502.ips", F_OK) != -1)) {
            if (debugMode) printf("No file detected. New file\n");
            file = fopen("/CtrRGBPAT2/0004013000003502.ips", "wb+");
            writeDefault(file);
        } else {
            struct stat stats;
            if (stat("/CtrRGBPAT2/0004013000003502.ips", &stats) == 0) {
                if (stats.st_size == 5 + (3 + 2) + 200*nbInstall + 3) { // PATCH + (OFFSET + SIZE) + DATA*NOTIFTYPES + EOF
                    if (debugMode) printf("Opening file normally\n");
                    file = fopen("/CtrRGBPAT2/0004013000003502.ips", "rb+");
                } else {
                    if (debugMode) printf("Old file detected. New file\n");
                    file = fopen("/CtrRGBPAT2/0004013000003502.ips", "wb+");
                    writeDefault(file);
                }
            } else {
                if (debugMode) printf("Cannot get file size somehow. New file\n");
                file = fopen("/CtrRGBPAT2/0004013000003502.ips", "wb+");
                writeDefault(file);
            }
        }
        
        LED notif;
        createLED(&notif, std::string(color_HEX), nbLoadPreset); // Will always be the custom pattern
        
        // https://zerosoft.zophar.net/ips.php for documentation of the IPS file format

        // PATCH

        // Boot / wake up : 0x00A0C8 real address is 0x10A0C8
        // SPOTPASS : 0x00A190 real address is 0x10A190
        // STREETPASS : 0x00A258 real address is 0x10A258
        // JOIN FRIEND GAME ??? : 0x00A320 real address is 0x10A320
        // FRIENDS : 0x00A3E8 real address is 0x10A3E8
        // LOW BATTERY : not located in the same place

        // Seek to the correct position in the patch
        fseek(file, 5 + (3 + 2) + 200*selectedType + 100*selectedStatus, 0);

        // DATA (100 BYTES)

        // This part applies the animation for when the console is either sleeping (0) or awake (1) depending on selectedStatus (0 or 1)
        // 4 BYTES
        fputc(ANIMDELAY, file);
        fputc(ANIMSMOOTH, file);
        fputc(LOOPBYTE, file);
        fputc(BLINKSPEED, file);
        // 96 BYTES
        fwrite(&notif, sizeof(notif), 1, file);

        // END OF PATCH

        // close file
        fclose(file);

        // Copy files to activate the patch
        fcopy("/CtrRGBPAT2/0004013000003502.ips", "/luma/titles/0004013000003502/code.ips");
        fcopy("/CtrRGBPAT2/0004013000003502.ips", "/luma/sysmodules/0004013000003502.ips");

        // Check if our files were written
        if( access("/luma/sysmodules/0004013000003502.ips", F_OK) != -1 && access("/luma/titles/0004013000003502/code.ips", F_OK) != -1) 
        {
            printf("Success !\n");
        } 
        else 
        {
            printf("Failed !\n");
        }

        closedir(dir);
        closedir(dir2);
        closedir(dir3);
    }
    else if (ENOENT == errno)
    {
        printf("Directory failed...\n");
    }
}

// Interesting fact : the low battery pattern overwrites everything
void ptmsysmSetInfoLedPattern(LED_MCU pattern)
{
    Handle serviceHandle = 0;
    Result result = srvGetServiceHandle(&serviceHandle, "ptm:sysm");
    if (result != 0) 
    {
        printf("Failed to get service ptm:sysm :(\n");
        return;
    }

    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x8010640;
    memcpy(&ipc[1], &pattern, 0x64);
    svcSendSyncRequest(serviceHandle);
    svcCloseHandle(serviceHandle);
}

void ptmsysmSetBatteryEmptyLedPattern(LowBatPat pattern)
{
    Handle serviceHandle = 0;
    Result result = srvGetServiceHandle(&serviceHandle, "ptm:sysm");
    if (result != 0) 
    {
        printf("Failed to get service ptm:sysm :(\n");
        return;
    }

    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x8040040;
    memcpy(&ipc[1], &pattern, 0x4);
    svcSendSyncRequest(serviceHandle);
    svcCloseHandle(serviceHandle);
}

void mcuhwcSetPowerLedPattern(int state) {
    Handle serviceHandle = 0;
    Result result = srvGetServiceHandle(&serviceHandle, "mcu::HWC");
    if (result != 0) 
    {
        printf("Failed to get service mcu::HWC :(\n");
        return;
    }

    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x60040;
    ipc[1] = state;
    svcSendSyncRequest(serviceHandle);
    svcCloseHandle(serviceHandle);
}

void test_LED(LED pattern)
{
    LED_MCU MCU_PAT;

    for (int i = 0; i<32; i++)
    {
        MCU_PAT.r[i] = pattern.r[i];
        MCU_PAT.g[i] = pattern.g[i];
        MCU_PAT.b[i] = pattern.b[i];
        if (debugMode) {
            printf("(%u, %u, %u)\n", pattern.r[i], pattern.g[i], pattern.b[i]);
        }
    }

    if (debugMode) {
        printf("First color = (%u, %u, %u)\n", pattern.r[0], pattern.g[0], pattern.b[0]);
        printf("Last color = (%u, %u, %u)\n", pattern.r[31], pattern.g[31], pattern.b[31]);
    }

    MCU_PAT.ani[0] = ANIMDELAY;
    MCU_PAT.ani[1] = ANIMSMOOTH;
    MCU_PAT.ani[2] = LOOPBYTE;
    MCU_PAT.ani[3] = BLINKSPEED;

    printf("Testing custom pattern...\n");

    if (debugMode) {
        LBP.seq[0] = ANIMDELAY;
        LBP.seq[1] = ANIMSMOOTH;
        LBP.seq[2] = LOOPBYTE;
        LBP.seq[3] = BLINKSPEED;
        ptmsysmSetBatteryEmptyLedPattern(LBP);
    }
    else ptmsysmSetInfoLedPattern(MCU_PAT);
}

// when done we want LUMA to reload so it can patch with our ips patches
// https://www.3dbrew.org/wiki/PTMSYSM:LaunchFIRMRebootSystem
void PTM_RebootAsync() 
{
    ptmSysmInit();

    Handle serviceHandle = 0;
    Result result = srvGetServiceHandle(&serviceHandle, "ptm:sysm");
    if (result != 0) {
        return;
    }

    u32 *commandBuffer = getThreadCommandBuffer();
    commandBuffer[0] = 0x04090080;
    commandBuffer[1] = 0x00000000;
    commandBuffer[2] = 0x00000000;

    svcSendSyncRequest(serviceHandle);
    svcCloseHandle(serviceHandle);

    ptmSysmExit();
}

void listMenu(int dispOffset)
{
    int colr; //= (int)strtol(color_HEX.substr(0, 2), NULL, 16);
    int colg; //= (int)strtol(color_HEX.substr(2, 2), NULL, 16);
    int colb; //= (int)strtol(color_HEX.substr(4, 2), NULL, 16);
    intRGB(std::string(color_HEX), &colr, &colg, &colb);
    consoleSelect(&topscreen);
    iprintf("\x1b[2J");
    printf("\x1b[0;0H\x1b[30;0m");
    printf("=================== Ctr\e[31mR\e[32mG\e[34mB\e[0mPAT2 ======%s======\n", debugMode ? "[DEBUG]" : "=======");
    switch (currMenu) {
        case 0:
            for (int i = 0; i < nbMain; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), mainMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
        break;

        case 1:
            printf("\e[40;36m(L) to copy, (R) to paste : \e[0m%s\n\n", color_copy);
            for (int i = dispOffset; i < dispOffset+25; i++) {
                printf("\x1b[%sm%s%d: %s%X%s%X%s%X\e[0m \e[48;2;%d;%d;%dm \e[0m %s\n", // RRR GGG BBB ?
                    (i == selected ? "47;30" : "37;40"),
                    (i < 9 ? "0" : ""), i+1,
                    (customLed.r[i] < 16 ? "0" : ""), customLed.r[i],
                    (customLed.g[i] < 16 ? "0" : ""), customLed.g[i],
                    (customLed.b[i] < 16 ? "0" : ""), customLed.b[i],
                    customLed.r[i], customLed.g[i], customLed.b[i],
                    (i == 31 ? "(will hold the color if no looping)" : (i == 0 ? "(triggers only after loop)" : ""))
                );
            }
        break;

        case 2:
            printf("Animation settings\n\n");
            for (int i = 0; i < nbSettings; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), settingsMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n======================\n");
            printf("(B) to go back\n");
        break;

        case 3:
            printf("Install animation - (1/2) Select animation recipient :\n\n");
            for (int i = 0; i < nbInstall; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), installMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n======================\n");
            printf("(B) to cancel\n");
        break;

        case 4:
            printf("Install animation - (2/2) When should it play ?\n\n");
            for (int i = 0; i < nbInstallMode; i++) 
            {
                printf("\x1b[%sm* %s%s%s%s%s\n",
                    (i == selected ? "47;30" : "30;0"),
                    installModeMenu[i],
                    (installType == 0 && i == 1 ? " (unused here)" : ""),
                    (installType == 3 ? " (unused ?)" : ""),
                    (installType == 4 && i == 0 ? " (impossible to get ?)" : ""),
                    (i == selected ? "\x1b[30;0m" : "")
                );
            
            }
            printf("\n======================\n");
            printf("(B) to cancel\n");
        break;

        case 5:
            printf("Load a pattern - Select the source :\n\n");
            for (int i = 0; i < nbLoad; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), loadMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n======================\n");
            printf("(B) to cancel\n");
        break;

        case 6:
            printf("Select the pattern to load :\n");
            printf("\e[33m! Overrides the current pattern in the editor !\e[0m\n\n");
        break;

        case 7:
            printf("Load a pattern - Select the preset to load :\n");
            printf("\e[33m! Overrides the current pattern in the editor !\e[0m\n\n");
            for (int i = 0; i < nbLoadPreset; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), loadPresetMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n======================\n");
            printf("(B) to cancel\n");
        break;

        case 8:
            printf("Low battery settings\n\n");
            printf("Not implemented yet\n");
            /*
            printf("\e[40;36m(A) to toggle on/off\e[0m%s\n\n", color_copy);
            for (int i = dispOffset; i < dispOffset+25; i++) {
                printf("\x1b[%sm%d: %d\e[0m \e[%dm \e[0m\n", // RRR GGG BBB ?
                    (i == selected ? "47;30" : "37;40"),
                    i+1, lbpTemp[i],
                    (lbpTemp[i] == 1 ? "41" : "0")
                );
            */
            printf("\n======================\n");
            printf("(B) to go back\n");
        break;
        default:
            //printf("err\n");
        break;
    }
    consoleSelect(&bottomscreen);
    iprintf("\x1b[2J");
    printf("\x1b[0;0H\x1b[30;0m");
    printf("======================\n\n");
    printf("ANIMATION DELAY : %02X\n", ANIMDELAY);
    printf("ANIMATION SMOOTHNESS : %02X\n", ANIMSMOOTH);
    printf("LOOP DELAY : %02X\n", LOOPBYTE);
    printf("BLINK SPEED : %02X\n", BLINKSPEED);
    printf("ENABLED : %s\e[0m\n", (enabled ? "\e[32mYES" : "\e[31mNO"));
    printf("\n======================\n\n");
    consoleSelect(&topscreen);
}

// This is not the final version, i will include more

int main(int argc, char **argv) 
{
    gfxInitDefault();
    aptSetHomeAllowed(false);

    // Init console for text output
    consoleInit(GFX_TOP, &topscreen);
    consoleInit(GFX_BOTTOM, &bottomscreen);

    consoleSelect(&topscreen);
    
    for (int i = 0; i < 4; i++) { LBP.seq[i] = 0x55; }

    int r, g, b;

    for (int i = 0; i < 32; i++) {
        customLed.r[i] = 0x00;
        customLed.g[i] = 0x00;
        customLed.b[i] = 0x00;
    }

    enabled = (access("/luma/sysmodules/0004013000003502.ips", F_OK) != -1 && access("/luma/titles/0004013000003502/code.ips", F_OK) != -1);

    selected = 0;
    int selOffset = 0;
    bool infoRead = false;  
    printf("Welcome to Ctr\e[31mR\e[32mG\e[34mB\e[0mPAT2 !\n\nThis is a tool allowing you to change the color\nand pattern of the LED when you receive\na notification.\n\nYou can select for which type of notifications\nyou want it to apply from the install menu.\n\nThis is not the final version, i will include\nmore things in future updates.\n\n\nControls :\n- Arrow UP and DOWN to move,\n- (A) to confirm/toggle\n- (B) to go back to the main menu\n- START to reboot\n\n\e[32mPress (A) to continue\e[0m\t\t\e[38;2;255;165;0mVersion 2.4\e[0m\n");
    //listMenu();

    int plstate = 0;


    while (aptMainLoop())
    {
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (aptCheckHomePressRejected()) {
            infoRead = true;
            listMenu(selOffset);
            //consoleSelect(&bottomscreen);
            //printf("Cannot return to the HOME Menu. START to reboot\n");
            printf("\n\nPress HOME again to exit, or START to reboot\n");
            printf("\e[33mYour changes wont be applied until next reboot\e[0m\n");
            aptSetHomeAllowed(true);
            //consoleSelect(&topscreen);
        }

        if (kDown) {
            aptSetHomeAllowed(false);
        }

        if (kDown & KEY_START)
        {
            consoleSelect(&bottomscreen);
            printf("Rebooting the console...\n");
            PTM_RebootAsync();
            //break; // needs changes, crashes the console
        }

        if (kDown & KEY_Y) {
            //debugMode = !debugMode;
            infoRead = true;
            if (currMenu != 0) {
                currMenu = 0;
                nbCmd = nbMain;
                selected = 0;
                selOffset = 0;
            }
            listMenu(selOffset);
        }

        if ((kDown & KEY_X) && debugMode) {
            consoleSelect(&bottomscreen);
            if (access("/CtrRGBPAT2/0004013000003502.ips", F_OK) != -1) {
                FILE *file = fopen("/CtrRGBPAT2/0004013000003502.ips", "wb+");
                writeDefault(file);
                fclose(file);
                listMenu(selOffset);
                printf("Configuration restored to default\n");
            } else {
                listMenu(selOffset);
                printf("No previous configuration detected\n");
            }
            consoleSelect(&topscreen);
        }

        if (kDown & KEY_DDOWN)
        {
            selected=selected+1;
            if (selected-selOffset > 24)
                selOffset = selOffset+1;
            if (selected>nbCmd-1) {
                selected = 0;
                selOffset = 0;
            }
            infoRead = true;
            listMenu(selOffset);
        }

        if (kDown & KEY_DUP)
        {
            selected=selected-1;
            if (selected-selOffset < 0)
                selOffset = selOffset-1;
            if (selected<0) {
                selected = nbCmd-1;
                selOffset = nbCmd-25;
            }
            infoRead = true;
            listMenu(selOffset);
        }

        if ((kDown & KEY_L) && currMenu == 1) {
            sprintf((char *)color_copy, "%s%X%s%X%s%X", (customLed.r[selected] < 16 ? "0" : ""), customLed.r[selected], (customLed.g[selected] < 16 ? "0" : ""), customLed.g[selected], (customLed.b[selected] < 16 ? "0" : ""), customLed.b[selected]);
            listMenu(selOffset);
        }

        if ((kDown & KEY_R) && currMenu == 1) {
            intRGB(std::string(color_copy), &r, &g, &b);
            customLed.r[selected] = r;
            customLed.g[selected] = g;
            customLed.b[selected] = b;
            listMenu(selOffset);
        }

        if (kDown & KEY_B) {
            currMenu = 0;
            nbCmd = nbMain;
            selected = 0;
            selOffset = 0;
            infoRead = true;
            listMenu(selOffset);
        }

        if ((kDown & KEY_L) && debugMode && currMenu == 0)
        {
            consoleSelect(&bottomscreen);
            plstate--;
            if (plstate < 0) plstate = 6;
            listMenu(currMenu);
            printf("-1 to power LED state = %d\n", plstate);
            printf("Sending sync to mcu::HWC...\n");
            mcuhwcSetPowerLedPattern(plstate);
            consoleSelect(&topscreen);
        }

        if ((kDown & KEY_R) && debugMode && currMenu == 0)
        {
            consoleSelect(&bottomscreen);
            plstate++;
            plstate = plstate%7;
            listMenu(currMenu);
            printf("+1 to power LED state = %d\n", plstate);
            printf("Sending sync to mcu::HWC...\n");
            mcuhwcSetPowerLedPattern(plstate);
            consoleSelect(&topscreen);
        }

        if (kDown & KEY_A)
        {
            if (infoRead) {
                switch(currMenu) {
                    case 0: // Main Menu
                        switch(selected)
                        {
                            case 0:
                                currMenu = 1;
                                nbCmd = nbEditor;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                                /*
                                hexaInput((char *)color_HEX, 6, "LED RGB color (in HEX)");
                                listMenu(selOffset);
                                if (debugMode) {
                                    for (int i = 0; i < 6; i++) {
                                        printf("Char %d is %d\n", i, (int)keybInput[i]);
                                    }
                                }
                                */
                            break;

                            case 1:
                                currMenu = 2;
                                nbCmd = nbSettings;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                                /*
                                selectedpat=selectedpat+1;
                                if (selectedpat>PATS-1)
                                    selectedpat = 0;
                                listMenu(selOffset);
                                */
                            break;

                            case 2:
                                LED test_notification;
                                listMenu(selOffset);
                                consoleSelect(&bottomscreen);
                                createLED(&test_notification, std::string(color_HEX), nbLoadPreset); // Will always be the custom pattern
                                test_LED(test_notification);
                                consoleSelect(&topscreen);
                            break;

                            case 3:
                                //LED notification;
                                //createLED(&notification, std::string(color_HEX), staticend, selectedpat);
                                //enabled = true;
                                currMenu = 3;
                                nbCmd = nbInstall;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                                //writepatch(notification);
                            break;

                            case 4:
                                currMenu = 5;
                                nbCmd = nbLoad;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                            break;

                            case 5:
                                consoleSelect(&bottomscreen);
                                printf("Toggling state, please wait...\n");
                                // copyFile code and stuff
                                mkdir("/CtrRGBPAT2", 0777);
                                if (enabled) {
                                    remove("/luma/titles/0004013000003502/code.ips");
                                    remove("/luma/sysmodules/0004013000003502.ips");
                                } else if (access("/CtrRGBPAT2/0004013000003502.ips", F_OK) != -1) {
                                    fcopy("/CtrRGBPAT2/0004013000003502.ips", "/luma/titles/0004013000003502/code.ips");
                                    fcopy("/CtrRGBPAT2/0004013000003502.ips", "/luma/sysmodules/0004013000003502.ips");
                                } else {
                                    selected = 7;
                                    listMenu(selOffset);
                                    printf("Unable to toggle state. Write IPS patch first\n");
                                    break;
                                }
                                enabled = !enabled;
                                consoleSelect(&topscreen);
                                listMenu(selOffset);
                                consoleSelect(&bottomscreen);
                                printf("Patch is now %s\n", (enabled ? "enabled" : "disabled"));
                                consoleSelect(&topscreen);
                            break;

                            case 6:
                                currMenu = 8;
                                nbCmd = nbLowBat;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                            break;

                            case 7:
                                ptmSysmInit();
                                PTMSYSM_ShutdownAsync(0);
                                ptmSysmExit();
                            break;

                            default:
                                consoleSelect(&bottomscreen);
                                printf("err\n");
                                consoleSelect(&topscreen);
                            break;
                        }
                    break;

                    case 1: // Editor
                    {
                        char hexCustom[10]; // 10 to avoid gcc compiling warnings
                        r = customLed.r[selected];
                        g = customLed.g[selected];
                        b = customLed.b[selected];
                        sprintf((char *)hexCustom, "%s%X%s%X%s%X", (customLed.r[selected] < 16 ? "0" : ""), customLed.r[selected], (customLed.g[selected] < 16 ? "0" : ""), customLed.g[selected], (customLed.b[selected] < 16 ? "0" : ""), customLed.b[selected]);
                        hexaInput((char *)hexCustom, 6, "LED RGB color (in HEX)");
                        intRGB(std::string(hexCustom), &r, &g, &b);
                        customLed.r[selected] = r;
                        customLed.g[selected] = g;
                        customLed.b[selected] = b;
                        listMenu(selOffset);
                    break;
                    }

                    case 2: // Settings
                        switch(selected)
                        {
                            case 0:
                                hexaInput((char *)anim_speed, 2, "Animation speed (delay)");
                                ANIMDELAY = (uint8_t)strtol(anim_speed, NULL, 16);
                                listMenu(selOffset);
                            break;
                            case 1:
                                hexaInput((char *)anim_smooth, 2, "Animation smoothness");
                                ANIMSMOOTH = (uint8_t)strtol(anim_smooth, NULL, 16);
                                listMenu(selOffset);
                            break;
                            case 2:
                                hexaInput((char *)anim_loop_delay, 2, "Animation loop delay");
                                LOOPBYTE = (uint8_t)strtol(anim_loop_delay, NULL, 16);
                                listMenu(selOffset);
                            break;
                            case 3:
                                hexaInput((char *)anim_blink_speed, 2, "Animation blink speed");
                                BLINKSPEED = (uint8_t)strtol(anim_blink_speed, NULL, 16);
                                listMenu(selOffset);
                            break;
                            default:
                                printf("err\n");
                            break;
                        }
                    break;

                    case 3: // Install
                        installType = selected;
                        currMenu = 4;
                        nbCmd = nbInstallMode;
                        selected = 0;
                        selOffset = 0;
                        listMenu(selOffset);
                    break;

                    case 4: // Install mode
                        LED notification;
                        //createLED(&notification, std::string(color_HEX), staticend, selectedpat);
                        enabled = true;
                        installMode = selected;
                        currMenu = 0;
                        nbCmd = nbMain;
                        listMenu(selOffset);
                        consoleSelect(&bottomscreen);
                        writepatch(notification, installType, installMode);
                        consoleSelect(&topscreen);
                    break;

                    case 5: // Loading
                        switch(selected)
                        {
                            case 0:
                                //currMenu = 6;
                                //nbCmd = nbLoad;
                                //selected = 0;
                                //selOffset = 0;
                                listMenu(selOffset);
                            break;
                            case 1:
                                currMenu = 7;
                                nbCmd = nbLoadPreset;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                            break;
                            default:
                                printf("err\n");
                            break;
                        }
                    break;

                    case 7: // Load preset
                        if (selected != nbLoadPreset-1) {
                            currMenu = 999;
                            listMenu(selOffset);
                            printf("Enter the base color for the preset :\n");
                            hexaInput((char *)color_HEX, 6, "LED RGB color (in HEX)");
                        }
                        selectedPreset = selected;
                        currMenu = 0;
                        nbCmd = nbMain;
                        selected = 0;
                        selOffset = 0;
                        listMenu(selOffset);
                        consoleSelect(&bottomscreen);
                        printf("Loading preset '%s'...\n", loadPresetMenu[selectedPreset]);
                        LED preset;
                        createLED(&preset, std::string(color_HEX), selectedPreset);
                        for (int i = 0; i < 32; i++) {
                            customLed.r[i] = preset.r[i];
                            customLed.g[i] = preset.g[i];
                            customLed.b[i] = preset.b[i];
                        }
                        printf("Preset loaded\n");
                        consoleSelect(&topscreen);

                    case 8: // Low battery settings
                        
                    break;

                    default:
                        printf("err\n");
                    break;
                }
                
            } else {
                infoRead = true;
                listMenu(selOffset);
            }
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
		gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}