#include <stdio.h>  // Basic functions
#include <stdlib.h> // malloc and free
#include <string.h> // String manipulation
#include <dirent.h> // For opendir() and readdir()
#include <unistd.h> // rmdir()
#include <fstream>
#include <sstream>
#include <3ds.h>    // Main 3ds lib (libctru)
#include <sys/types.h>
#include <sys/time.h>

// TODO:
// - Save/restore from/to file
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

int currMenu = 0;
int selected = 0;
int selectedX = 0;

const int nbMain = 8; // currMenu == 0
const int nbEditor = 32; // currMenu == 1
const int nbEditorX = 4; // currMenu == 1
const int nbEditorDisp = 22; // currMenu == 1
const int nbSettings = 4; // currMenu == 2
const int nbInstall = 5; // currMenu == 3 || 43 || 44
const int nbInstallMode = 2; // currMenu == 31 || 431
const int nbLoad = 4; // currMenu == 4
const int nbLoadPreset = 4; // currMenu == 42
const int nbLowBat = 3; // currMenu == 5
const int nbLowBatX = 32; // currMenu == 5

int nbMenus[999] = {0};

int nbCmd = nbMain;
int nbCmdX = 1;
int maxDisp = 0;

const char* mainMenu[nbMain] = {
    "Pattern editor",
    "Animation settings",
    "Test animation",
    "Install animation",
    "Load a pattern",
    "Toggle patch",
    "Low battery settings",
    "Shutdown 3DS"
};

const char* settingsMenu[nbSettings] = {
    "Animation speed (delay)",
    "Animation smoothness",
    "Loop delay (FF = no loop)",
    "Blink speed (unused ?)"
};

const char* installMenu[nbInstall] = {
    "Boot & Wake up",
    "SpotPass",
    "StreetPass",
    "Join friend game ??",
    "Friend online"
};

const char* installModeMenu[nbInstallMode] = {
    "While in sleep mode",
    "While awake"
};

const char* loadMenu[nbLoad] = {
    "Saved patterns (not implemented)",
    "Presets",
    "Currently installed",
    "Default animations"
};

const char* loadPresetMenu[nbLoadPreset] = {
    "Blink",
    "Explode",
    "Static",
    "Rainbow"
};

const char* lowBatMenu[nbLowBat] = {
    " ",
    "Preview",
    "Apply"
};

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
// - LOW BATTERY : 0x55, 0x55, 0x55, 0x55  <- not actual settings, more like the default sequence of the LED
uint8_t ANIMDELAY = 0x2F; // old : 0x50
uint8_t ANIMSMOOTH = 0x5F; // old : 0x3c
uint8_t LOOPBYTE = 0xFF; // FF = no loop
uint8_t BLINKSPEED = 0x00; // https://www.3dbrew.org/wiki/MCURTC:SetInfoLEDPattern

LowBatPat LBP;

int lbpTemp[32];

int selectedPreset;
int installType;
int installMode;

const int KHELD_MS = 600;

bool enabled;

bool previewColors = false;

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

static SwkbdCallbackResult rgbDecimalCheck(void* user, const char** ppMessage, const char* text, size_t textlen) {
    if (atoi(text) < 0 || atoi(text) > 255) {
        *ppMessage = "Min: 0\nMax: 255";
        return SWKBD_CALLBACK_CONTINUE;
    }
    return SWKBD_CALLBACK_OK;
}

void numInput(char* numText, int numLen, const char* hintText) {
    swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, numLen);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetFilterCallback(&swkbd, rgbDecimalCheck, NULL);
    swkbdSetFeatures(&swkbd, SWKBD_FIXED_WIDTH);
    swkbdSetInitialText(&swkbd, numText);
    swkbdSetHintText(&swkbd, hintText);
    swkbdSetStatusData(&swkbd, &swkbdStatus, false, true);
    swkbdSetLearningData(&swkbd, &swkbdLearning, false, true);
    swkbdInputText(&swkbd, (char*)keybInput, numLen + 1);

    if (strcmp(keybInput, "") != 0) {
        strcpy(numText, keybInput);
    }
}

void initMenus() {
    nbMenus[0] = nbMain;
    nbMenus[1] = nbEditor;
    nbMenus[2] = nbSettings;
    nbMenus[3] = nbInstall;
    nbMenus[31] = nbInstallMode;
    nbMenus[4] = nbLoad;
    nbMenus[42] = nbLoadPreset;
    nbMenus[43] = nbInstall;
    nbMenus[431] = nbInstallMode;
    nbMenus[44] = nbInstall;
    nbMenus[5] = nbLowBat;
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

time_t getMs() {
    struct timeval tp;
	gettimeofday(&tp, NULL);
    return (time_t)(tp.tv_sec * 1000 + tp.tv_usec / 1000);
}

int goBack(int menu) {
    if (menu < 9) {
        menu = 0;
    } else {
        menu = menu/10;
    }
    return menu;
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
            pattern->r[31] = 152;
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
            pattern->g[31] = 221;
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
            pattern->b[31] = 10;
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


int loadFromFile(char* path, LED_MCU* led, int type, int status) {
    FILE *file;
    printf("Loading from file...\n");
    if (!(access(path, F_OK) != -1)) {
        printf("File does not exists\n");
        return -1;
    } else {
        struct stat stats;
        if (stat(path, &stats) == 0) {
            if (stats.st_size == 5 + (3 + 2) + 200*nbInstall + 3) { // PATCH + (OFFSET + SIZE) + DATA*NOTIFTYPES + EOF
                if (debugMode) printf("Opening file...\n");
                file = fopen(path, "rb");
            } else {
                printf("Invalid file format\n");
                return -3;
            }
        } else {
            printf("Cannot obtain file size\n");
            return -2;
        }
    }

    fseek(file, 5 + (3 + 2) + 200*type + 100*status, 0);
    fread(led, sizeof(LED_MCU), 1, file);
    fclose(file);
    printf("Animation loaded successfully\n");
    return 0;
}


LED_MCU get_default_pattern(int type, int status) {
    LED_MCU temp;

    switch (type) {
        case 0: // Boot / wake up LED
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
        break;

        case 1: // SpotPass LED
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
        break;

        case 2: // StreetPass LED
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
        break;

        case 3: // Join friend game ??? LED
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
        break;

        case 4: // Friends LED
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
        break;

        default:
            temp.ani[0] = 0x00;
            temp.ani[1] = 0x00;
            temp.ani[2] = 0xFF;
            temp.ani[3] = 0x00;

            for (int i = 0; i < 32; i++) {
                temp.r[i] = 0x00;
                temp.g[i] = 0x00;
                temp.b[i] = 0x00;
            }
        break;
    }
    
    if (status == 1) {
        temp.r[31] = 0x00;
        temp.g[31] = 0x00;
        temp.b[31] = 0x00;
    }

    return temp;
}

void writeDefault(FILE* file) {
    
    printf("Writing default configuration...\n");

    LED_MCU defaultNotifs[nbInstall];

    //LED_MCU temp;
    
    // Boot / wake up LED
    defaultNotifs[0] = get_default_pattern(0, 0);

    // SpotPass LED
    defaultNotifs[1] = get_default_pattern(1, 0);

    // StreetPass LED
    defaultNotifs[2] = get_default_pattern(2, 0);

    // Join friend game ??? LED
    defaultNotifs[3] = get_default_pattern(3, 0);
    
    // Friends LED
    defaultNotifs[4] = get_default_pattern(4, 0);


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

// Interesting fact : the low battery pattern is shown above everything and will pause the currently running animation while active
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

void test_LED(LED pattern, uint8_t a_d = ANIMDELAY, uint8_t a_s = ANIMSMOOTH, uint8_t l_b = LOOPBYTE, uint8_t b_s = BLINKSPEED)
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

    MCU_PAT.ani[0] = a_d;
    MCU_PAT.ani[1] = a_s;
    MCU_PAT.ani[2] = l_b;
    MCU_PAT.ani[3] = b_s;

    //printf("Testing custom pattern...\n");

    if (debugMode) {
        LBP.seq[0] = ANIMDELAY;
        LBP.seq[1] = ANIMSMOOTH;
        LBP.seq[2] = LOOPBYTE;
        LBP.seq[3] = BLINKSPEED;
        ptmsysmSetBatteryEmptyLedPattern(LBP);
    }
    else ptmsysmSetInfoLedPattern(MCU_PAT);
}

void reset_LED() {
    LED_MCU reset;
    reset = get_default_pattern(99, 0);
    ptmsysmSetInfoLedPattern(reset);
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
    reset_LED();
    int colr; //= (int)strtol(color_HEX.substr(0, 2), NULL, 16);
    int colg; //= (int)strtol(color_HEX.substr(2, 2), NULL, 16);
    int colb; //= (int)strtol(color_HEX.substr(4, 2), NULL, 16);
    intRGB(std::string(color_HEX), &colr, &colg, &colb);
    consoleSelect(&topscreen);
    iprintf("\x1b[2J");
    printf("\e[0;0H\e[30;0m");
    printf("=================== Ctr\e[31mR\e[32mG\e[34mB\e[0mPAT2 ======%s======\n\n", debugMode ? "[DEBUG]" : "=======");
    switch (currMenu) {
        case 0: // Main menu
            for (int i = 0; i < nbMain; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), mainMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
        break;

        case 1: // Pattern editor
            if (previewColors) {
                LED color_preview;
                for (int i = 0; i < 32; i++) {
                    color_preview.r[i] = customLed.r[selected];
                    color_preview.g[i] = customLed.g[selected];
                    color_preview.b[i] = customLed.b[selected];
                }
                test_LED(color_preview, 0xFF, 0x00, 0x00, 0x00);
            }    

            intRGB(std::string(color_copy), &colr, &colg, &colb);
            printf("Pattern editor\n\n");
            printf("\e[40;36m(Y) to %s selected color preview\n(L) to copy, (R) to paste : \e[0m%s \e[48;2;%d;%d;%dm \e[0m\n\n", (previewColors ? "disable" : "enable"), color_copy, colr, colg, colb);
            for (int i = dispOffset; i < dispOffset+nbEditorDisp && i < nbEditor; i++) {
                printf("\e[%sm%s%d: %s%X%s%X%s%X\e[0m \e[48;2;%d;%d;%dm \e[0m  %s",
                    (i == selected && selectedX == 0 ? "47;30" : "37;40"),
                    (i < 9 ? "0" : ""), i+1,
                    (customLed.r[i] < 16 ? "0" : ""), customLed.r[i],
                    (customLed.g[i] < 16 ? "0" : ""), customLed.g[i],
                    (customLed.b[i] < 16 ? "0" : ""), customLed.b[i],
                    customLed.r[i], customLed.g[i], customLed.b[i],
                    (i != selected ? (
                        i == 31 ? "(will hold the color if no looping)" : (i == 0 ? "(triggers only after loop)" : "")
                    ) : "")
                );
                if (i == selected) {
                    printf("%s%s%03d\e[0m  %s%s%03d\e[0m  %s%s%03d\e[0m  %s",
                        "\e[1;41m \e[0m",
                        (selectedX == 1 ? "\e[47;30m" : ""),
                        customLed.r[i],
                        "\e[1;42m \e[0m",
                        (selectedX == 2 ? "\e[47;30m" : ""),
                        customLed.g[i],
                        "\e[1;44m \e[0m",
                        (selectedX == 3 ? "\e[47;30m" : ""),
                        customLed.b[i],
                        (i == 31 || i == 0 ? "..." : "")
                    );
                }
                printf("\n");
            }
        break;

        case 2: // Animation settings
            printf("Animation settings\n\n");
            for (int i = 0; i < nbSettings; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), settingsMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n===================\n\n");
            printf("(B) to go back\n");
        break;

        case 3: // Install menu (destination selection)
            printf("Install animation (1/2)\n\tSelect animation recipient :\n\n");
            for (int i = 0; i < nbInstall; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), installMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n===================\n\n");
            printf("(B) to cancel\n");
        break;

        case 31: // Install menu (status selection)
            printf("Install animation (2/2)\n\tWhen should it play ?\n\n");
            for (int i = 0; i < nbInstallMode; i++) 
            {
                printf("\x1b[%sm* %s%s%s%s%s\n",
                    (i == selected ? "47;30" : "30;0"),
                    installModeMenu[i],
                    (installType == 0 && i == 1 ? " (unused)" : ""),
                    (installType == 3 ? " (unused ?)" : ""),
                    (installType == 4 && i == 0 ? " (impossible to get ?)" : ""),
                    (i == selected ? "\x1b[30;0m" : "")
                );
            
            }
            printf("\n===================\n\n");
            printf("(B) to go back\n");
        break;

        case 4: // Load menu
            printf("Load a pattern - Select the source :\n\n");
            for (int i = 0; i < nbLoad; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), loadMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n===================\n\n");
            printf("(B) to cancel\n");
        break;

        case 41: // Load from file menu
            printf("Select the pattern to load :\n");
            printf("\e[38;5;214m! Overrides the current pattern in the editor !\e[0m\n\e[40;36m(Y) to preview the selected pattern\e[0m\n\n");
        break;

        case 42: // Load preset menu
            printf("Load a pattern - Select the preset to load :\n");
            printf("\e[38;5;214m! Overrides the current pattern in the editor !\e[0m\n\e[40;36m(Y) to preview the selected pattern\e[0m\n\n");
            for (int i = 0; i < nbLoadPreset; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), loadPresetMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n===================\n\n");
            printf("(B) to go back\n");
        break;

        case 43: // Load currently installed menu (source selection)
            printf("Load a pattern - Currently installed (1/2)\n\tSelect animation origin :\n");
            printf("\e[38;5;214m! Overrides the pattern & settings in the editor !\n\n");
            for (int i = 0; i < nbInstall; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), installMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n===================\n\n");
            printf("(B) to go back\n");
        break;

        case 431: // Load currently installed menu (status selection)
            printf("Load a pattern - Currently installed (2/2)\n\tSelect animation status :\n");
            printf("\e[38;5;214m! Overrides the pattern & settings in the editor !\n\n");
            for (int i = 0; i < nbInstallMode; i++) 
            {
                printf("\x1b[%sm* %s%s\n", (i == selected ? "47;30" : "30;0"), installModeMenu[i], (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n===================\n\n");
            printf("(B) to go back\n");
        break;

        case 44: // Load default animation menu
            printf("Load a pattern - Select the default to load :\n");
            printf("\e[38;5;214m! Overrides the pattern & settings in the editor !\e[0m\n\e[40;36m(Y) to preview the selected default animation\e[0m\n\n");
            for (int i = 0; i < nbInstall; i++) 
            {
                printf("\x1b[%sm* %s%s%s\n", (i == selected ? "47;30" : "30;0"), installMenu[i], (i == 0 ? " (= empty)" : ""), (i == selected ? "\x1b[30;0m" : ""));
            }
            printf("\n===================\n\n");
            printf("(B) to go back\n");
        break;

        case 5: // Low battery settings
            printf("Low battery settings\n\n");
            //printf("Not implemented yet\n");
            
            printf("\e[40;36mNavigate with LEFT/RIGHT, (A) to toggle on/off\e[0m\n\n");
            
            for (int n = 0; n < nbLowBat; n++) {
                if (n == 0) {
                    printf(lowBatMenu[n]);
                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 8; j++) {
                            printf("\e[%sm%d\e[0m\e[1B\e[1D\e[%sm \e[0m\e[1A", (n == selected && i*8+j == selectedX ? "47;30" : "0"), (LBP.seq[i] & (1 << j)) >> j, ((LBP.seq[i] & (1 << j)) >> j) ? "1;41" : "0");
                        }
                    }
                    printf("\n\n\n");
                } else {
                    printf("\x1b[%sm* %s%s\n", (n == selected ? "47;30" : "30;0"), lowBatMenu[n], (n == selected ? "\x1b[30;0m" : ""));
                }
            }
            

            printf("\n===================\n\n");
            printf("(B) to go back\n");
        break;
        default:
            //printf("err\n");
        break;
    }
    consoleSelect(&bottomscreen);
    iprintf("\x1b[2J");
    printf("\x1b[0;0H\x1b[30;0m");
    printf("=============== Settings ===============\n\n");
    printf("ANIMATION DELAY :\t\t  %02X (%d)\n", ANIMDELAY, ANIMDELAY);
    printf("ANIMATION SMOOTHNESS : %02X (%d)\n", ANIMSMOOTH, ANIMSMOOTH);
    printf("LOOP DELAY :\t\t\t  %02X (%d)\n", LOOPBYTE, LOOPBYTE);
    printf("BLINK SPEED :\t\t\t  %02X (%d)\n", BLINKSPEED, BLINKSPEED);
    printf("ENABLED :\t\t\t\t  %s\e[0m\n", (enabled ? "\e[32mYES" : "\e[31mNO"));
    printf("\n================= Logs =================\n\n");
    //consoleSelect(&topscreen);
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
    int ret;
    LED temp_led;
    LED_MCU temp_mcu;
    char hexCustom[10]; // 10 to avoid gcc compiling warnings
    char rgbVal[5];
    time_t startHold = 0;
    u32 kHeld = 0;
    bool notSoFast = false; // Avoid held keys going too fast in the menus

    initMenus();

    for (int i = 0; i < 32; i++) {
        customLed.r[i] = 0x00;
        customLed.g[i] = 0x00;
        customLed.b[i] = 0x00;
    }

    enabled = (access("/luma/sysmodules/0004013000003502.ips", F_OK) != -1 && access("/luma/titles/0004013000003502/code.ips", F_OK) != -1);

    selected = 0;
    int selOffset = 0;
    bool infoRead = false;  
    printf("Welcome to Ctr\e[31mR\e[32mG\e[34mB\e[0mPAT2 !\n\nThis is a tool allowing you to change the color\nand pattern of the LED when you receive\na notification.\n\nYou can select for which type of notifications\nyou want it to apply from the install menu.\n\nThis is not the final version, i will include\nmore things in future updates.\n\n\n\e[40;36mControls :\n- Arrow UP and DOWN to move,\n- (A) to confirm/edit/toggle\n- (B) to cancel/go back to the main menu\n- START to reboot\e[0m\n\n\e[32mPress (A) to continue\e[0m\t\t\e[38;2;255;165;0mVersion 2.5\e[0m\n");
    //listMenu();

    int plstate = 0;


    while (aptMainLoop())
    {
        hidScanInput();

        u32 kDown = hidKeysDown();
        u32 kUp = hidKeysUp();

        if (aptCheckHomePressRejected()) {
            infoRead = true;
            listMenu(selOffset);
            //printf("Cannot return to the HOME Menu. START to reboot\n");
            consoleSelect(&topscreen);
            printf("\n\nPress HOME again to exit, or START to reboot\n");
            printf("\e[38;5;208mYour changes wont be applied until next reboot\e[0m\n");
            aptSetHomeAllowed(true);
            consoleSelect(&bottomscreen);
        }

        if (kDown) {
            aptSetHomeAllowed(false);
        }

        if (kDown & KEY_START)
        {
            printf("Rebooting the console...\n");
            PTM_RebootAsync();
            //break; // needs changes, crashes the console
        }

        if (kDown & KEY_Y) { // Generally used for previewing things
            switch (currMenu) {
                case 0:
                    //debugMode = !debugMode;
                    infoRead = true;
                    if (currMenu != 0) {
                        currMenu = 0;
                        nbCmd = nbMain;
                        selected = 0;
                        selOffset = 0;
                    }
                    listMenu(selOffset);
                break;

                case 1:
                    previewColors = !previewColors;
                    listMenu(selOffset);
                    printf("Selected color preview: %s\n", previewColors ? "enabled" : "disabled");
                break;

                case 42:
                    printf("Previewing '%s' preset...\n", loadPresetMenu[selected]);
                    createLED(&temp_led, std::string(color_HEX), selected);
                    test_LED(temp_led);
                    printf("\e[40;36mPerform any action to stop the preview\e[0m\n");
                break;

                case 431:
                    // Will do later
                break;

                case 44:
                    printf("Previewing '%s' LED...\n", installMenu[selected]);
                    temp_mcu = get_default_pattern(selected, 0);
                    for (int i = 0; i < 32; i++) {
                        temp_led.r[i] = temp_mcu.r[i];
                        temp_led.g[i] = temp_mcu.g[i];
                        temp_led.b[i] = temp_mcu.b[i];
                    }
                    test_LED(temp_led, temp_mcu.ani[0], temp_mcu.ani[1], temp_mcu.ani[2], temp_mcu.ani[3]);
                    printf("\e[40;36mPerform any action to stop the preview\e[0m\n");
                break;

                default:
                break;

            }
        }

        if ((kDown & KEY_X) && debugMode) {
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
        }

        if (kDown & KEY_DOWN) {
            startHold = getMs();
            kHeld |= KEY_DOWN;
        }

        if (kUp & KEY_DOWN) {
            startHold = 0;
            kHeld &= ~KEY_DOWN;
        }

        if (kDown & KEY_DOWN || (getMs()-startHold > KHELD_MS && kHeld & KEY_DOWN && !notSoFast))
        {
            selected++;
            if (selected-selOffset > maxDisp-2 && selected < nbCmd-1)
                selOffset++;
            if (selected>nbCmd-1) {
                selected = 0;
                selOffset = 0;
            }
            infoRead = true;
            listMenu(selOffset);
        }

        if (kDown & KEY_UP) {
            startHold = getMs();
            kHeld |= KEY_UP;
        }

        if (kUp & KEY_UP) {
            startHold = 0;
            kHeld &= ~KEY_UP;
        }

        if (kDown & KEY_UP || (getMs()-startHold > KHELD_MS && kHeld & KEY_UP && !notSoFast))
        {
            selected--;
            if (selected-selOffset < 1 && selected > 0)
                selOffset--;
            if (selected<0) {
                selected = nbCmd-1;
                selOffset = nbCmd-maxDisp;
            }
            infoRead = true;
            listMenu(selOffset);
        }

        if (kDown & KEY_LEFT) {
            startHold = getMs();
            kHeld |= KEY_LEFT;
        }

        if (kUp & KEY_LEFT) {
            startHold = 0;
            kHeld &= ~KEY_LEFT;
        }

        if (kDown & KEY_LEFT || (getMs()-startHold > KHELD_MS && kHeld & KEY_LEFT && !notSoFast)) {
            selectedX--;
            if (selectedX < 0)
                selectedX = nbCmdX-1;
            infoRead = true;
            listMenu(selOffset);
        }

        if (kDown & KEY_RIGHT) {
            startHold = getMs();
            kHeld |= KEY_RIGHT;
        }

        if (kUp & KEY_RIGHT) {
            startHold = 0;
            kHeld &= ~KEY_RIGHT;
        }

        if (kDown & KEY_RIGHT || (getMs()-startHold > KHELD_MS && kHeld & KEY_RIGHT && !notSoFast)) {
            selectedX++;
            if (selectedX > nbCmdX-1)
                selectedX = 0;
            infoRead = true;
            listMenu(selOffset);
        }

        if (kHeld && getMs()-startHold > KHELD_MS) {
            notSoFast = !notSoFast;
        } else {
            notSoFast = false;
        }

        if (kDown & KEY_L) {
            switch (currMenu) {
                case 0: // Main menu
                    if (debugMode) {
                        plstate--;
                        if (plstate < 0) plstate = 6;
                        listMenu(currMenu);
                        printf("-1 to power LED state = %d\n", plstate);
                        printf("Sending sync to mcu::HWC...\n");
                        mcuhwcSetPowerLedPattern(plstate);
                    }
                break;
                case 1: // Pattern editor
                    sprintf((char *)color_copy, "%s%X%s%X%s%X", (customLed.r[selected] < 16 ? "0" : ""), customLed.r[selected], (customLed.g[selected] < 16 ? "0" : ""), customLed.g[selected], (customLed.b[selected] < 16 ? "0" : ""), customLed.b[selected]);
                    listMenu(selOffset);
                break;
                default:
                break;
            }
        }

        if (kDown & KEY_R) {
            switch (currMenu) {
                case 0: // Main menu
                    if (debugMode) {
                        plstate++;
                        plstate = plstate%7;
                        listMenu(currMenu);
                        printf("+1 to power LED state = %d\n", plstate);
                        printf("Sending sync to mcu::HWC...\n");
                        mcuhwcSetPowerLedPattern(plstate);
                    }
                break;
                case 1: // Pattern editor
                    intRGB(std::string(color_copy), &r, &g, &b);
                    customLed.r[selected] = r;
                    customLed.g[selected] = g;
                    customLed.b[selected] = b;
                    listMenu(selOffset);
                break;
                default:
                break;
            }
        }

        if (kDown & KEY_B) {
            currMenu = goBack(currMenu);
            nbCmd = nbMenus[currMenu];
            nbCmdX = 1;
            selected = 0;
            selOffset = 0;
            selectedX = 0;
            infoRead = true;
            listMenu(selOffset);
        }


        if (kDown & KEY_A)
        {
            if (infoRead) {
                switch(currMenu) {
                    case 0: // Main Menu
                        switch(selected)
                        {
                            case 0: // Go to pattern editor
                                currMenu = 1;
                                nbCmd = nbEditor;
                                nbCmdX = nbEditorX;
                                selected = 1; // intentional
                                selectedX = 0;
                                selOffset = 0;
                                maxDisp = nbEditorDisp;
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

                            case 1: // Go to animation settings
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

                            case 2: // Test current pattern
                                listMenu(selOffset);
                                createLED(&temp_led, std::string(color_HEX), nbLoadPreset); // Will always be the custom pattern
                                test_LED(temp_led);
                                printf("\e[40;36mPerform any action to stop the preview\e[0m\n");
                            break;

                            case 3: // Install animation
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

                            case 4: // Load a pattern
                                currMenu = 4;
                                nbCmd = nbLoad;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                            break;

                            case 5: // Toggle the patch
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
                                    selected = 3;
                                    listMenu(selOffset);
                                    printf("Unable to toggle state.\nWrite IPS patch first\n");
                                    break;
                                }
                                enabled = !enabled;
                                listMenu(selOffset);
                                printf("Patch is now %s\n", (enabled ? "enabled" : "disabled"));
                            break;

                            case 6: // Go to low battery settings
                                currMenu = 5;
                                nbCmd = nbLowBat;
                                nbCmdX = nbLowBatX;
                                selected = 0;
                                selectedX = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                            break;

                            case 7: // Shutdown
                                ptmSysmInit();
                                PTMSYSM_ShutdownAsync(0);
                                ptmSysmExit();
                            break;

                            default:
                                printf("err\n");
                            break;
                        }
                    break;

                    case 1: // Editor
                    {
                        switch (selectedX) {
                            case 0: // Hexadecimal color
                                r = customLed.r[selected];
                                g = customLed.g[selected];
                                b = customLed.b[selected];
                                sprintf((char *)hexCustom, "%s%X%s%X%s%X", (customLed.r[selected] < 16 ? "0" : ""), customLed.r[selected], (customLed.g[selected] < 16 ? "0" : ""), customLed.g[selected], (customLed.b[selected] < 16 ? "0" : ""), customLed.b[selected]);
                                hexaInput((char *)hexCustom, 6, "LED RGB color (in HEX)");
                                intRGB(std::string(hexCustom), &r, &g, &b);
                                customLed.r[selected] = r;
                                customLed.g[selected] = g;
                                customLed.b[selected] = b;
                            break;

                            case 1: // Red value
                                sprintf((char *)rgbVal, "%d", customLed.r[selected]);
                                numInput((char *)rgbVal, 3, "LED Red value (0-255)");
                                customLed.r[selected] = (uint8_t)atoi(rgbVal);
                            break;
                            
                            case 2: // Green value
                                sprintf((char *)rgbVal, "%d", customLed.g[selected]);
                                numInput((char *)rgbVal, 3, "LED Green value (0-255)");
                                customLed.g[selected] = (uint8_t)atoi(rgbVal);
                            break;

                            case 3: // Blue value
                                sprintf((char *)rgbVal, "%d", customLed.b[selected]);
                                numInput((char *)rgbVal, 3, "LED Blue value (0-255)");
                                customLed.b[selected] = (uint8_t)atoi(rgbVal);
                            break;

                            default:
                            break;
                        }
                        listMenu(selOffset);
                    break;
                    }

                    case 2: // Settings
                        switch(selected)
                        {
                            case 0: // Animation delay
                                hexaInput((char *)anim_speed, 2, "Animation speed (delay)");
                                ANIMDELAY = (uint8_t)strtol(anim_speed, NULL, 16);
                                listMenu(selOffset);
                            break;

                            case 1: // Animation smoothness
                                hexaInput((char *)anim_smooth, 2, "Animation smoothness");
                                ANIMSMOOTH = (uint8_t)strtol(anim_smooth, NULL, 16);
                                listMenu(selOffset);
                            break;

                            case 2: // Animation loop delay
                                hexaInput((char *)anim_loop_delay, 2, "Animation loop delay");
                                LOOPBYTE = (uint8_t)strtol(anim_loop_delay, NULL, 16);
                                listMenu(selOffset);
                            break;

                            case 3: // Animation blink speed
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
                        currMenu = 31;
                        nbCmd = nbInstallMode;
                        selected = 0;
                        selOffset = 0;
                        listMenu(selOffset);
                    break;

                    case 31: // Install mode (while sleeping or awake)
                        //createLED(&temp_led, std::string(color_HEX), staticend, selectedpat);
                        enabled = true;
                        installMode = selected;
                        currMenu = 0;
                        nbCmd = nbMain;
                        listMenu(selOffset);
                        writepatch(temp_led, installType, installMode);
                    break;

                    case 4: // Pattern loading
                        switch(selected)
                        {
                            case 0: // Saved patterns
                                //currMenu = 41;
                                //nbCmd = nbLoad;
                                //selected = 0;
                                //selOffset = 0;
                                listMenu(selOffset);
                            break;

                            case 1: // Presets
                                currMenu = 42;
                                nbCmd = nbLoadPreset;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                            break;
                            
                            case 2: // Currently installed
                                currMenu = 43;
                                nbCmd = nbInstall;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                            break;
                            
                            case 3: // Default animations
                                currMenu = 44;
                                nbCmd = nbInstall;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                            break;
                            
                            default:
                                printf("err\n");
                            break;
                        }
                    break;

                    case 42: // Load preset
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
                        printf("Loading '%s' preset...\n", loadPresetMenu[selectedPreset]);
                        createLED(&temp_led, std::string(color_HEX), selectedPreset);
                        for (int i = 0; i < 32; i++) {
                            customLed.r[i] = temp_led.r[i];
                            customLed.g[i] = temp_led.g[i];
                            customLed.b[i] = temp_led.b[i];
                        }
                        printf("Preset loaded\n");
                    break;

                    case 43: // Load from currently installed (source selection)
                        installType = selected;
                        currMenu = 431;
                        nbCmd = nbInstallMode;
                        selected = 0;
                        selOffset = 0;
                        listMenu(selOffset);
                    break;

                    case 431: // Load from currently installed (status selection)
                        installMode = selected;
                        currMenu = 0;
                        nbCmd = nbMain;
                        selected = 0;
                        selOffset = 0;
                        listMenu(selOffset);
                        ret = loadFromFile((char*)"/luma/sysmodules/0004013000003502.ips", &temp_mcu, installType, installMode);
                        if (ret == 0) {
                            for (int i = 0; i < 32; i++) {
                                customLed.r[i] = temp_mcu.r[i];
                                customLed.g[i] = temp_mcu.g[i];
                                customLed.b[i] = temp_mcu.b[i];
                            }
                            ANIMDELAY = temp_mcu.ani[0];
                            ANIMSMOOTH = temp_mcu.ani[1];
                            LOOPBYTE = temp_mcu.ani[2];
                            BLINKSPEED = temp_mcu.ani[3];
                            printf("Pattern '%s' loaded\n", installMenu[installType]);
                        } else if (ret == -1) {
                            printf("Install an animation first\n");
                        } else {
                            printf("Error while loading pattern\n");
                        }
                    break;

                    case 44: // Load default animation
                        selectedPreset = selected;
                        currMenu = 0;
                        nbCmd = nbMain;
                        selected = 0;
                        selOffset = 0;
                        listMenu(selOffset);
                        printf("Loading '%s' default...\n", installMenu[selectedPreset]);
                        temp_mcu = get_default_pattern(selectedPreset, 0);
                        for (int i = 0; i < 32; i++) {
                            customLed.r[i] = temp_mcu.r[i];
                            customLed.g[i] = temp_mcu.g[i];
                            customLed.b[i] = temp_mcu.b[i];
                        }
                        ANIMDELAY = temp_mcu.ani[0];
                        ANIMSMOOTH = temp_mcu.ani[1];
                        LOOPBYTE = temp_mcu.ani[2];
                        BLINKSPEED = temp_mcu.ani[3];
                        printf("Default animation loaded\n");
                    break;

                    case 5: // Low battery settings
                        switch (selected) {
                            case 0: // Low battery sequence
                                if (LBP.seq[selectedX/8] & (1 << (selectedX%8))) {
                                    LBP.seq[selectedX/8] &= ~(1 << (selectedX%8));
                                } else {
                                    LBP.seq[selectedX/8] |= (1 << (selectedX%8));
                                }
                                listMenu(selOffset);
                            break;
                            
                            case 1: // Preview low battery pattern
                                printf("Previewing low battery pattern...\n");
                                for (int i = 0; i < 4; i++) {
                                    for (int j = 0; j < 8; j++) {
                                        if (i*8+j+1 < 32) {
                                            temp_led.r[i*8+j+1] = ((LBP.seq[i] & (1 << j)) >> j) ? 0xFF : 0x00;
                                            temp_led.g[i*8+j+1] = 0x00;
                                            temp_led.b[i*8+j+1] = 0x00;
                                        }
                                    }
                                }
                                // Because this uses a normal notification LED, and those only play the very first color when looped
                                temp_led.r[0] = ((LBP.seq[3] & (1 << 7)) >> 7) ? 0xFF : 0x00;
                                temp_led.g[0] = 0x00;
                                temp_led.b[0] = 0x00;
                                
                                test_LED(temp_led, 0x40, 0x00, 0x00, 0x00);
                                printf("\e[40;36mPerform any action to stop the preview\e[0m\n");
                            break;

                            case 2: // Apply low battery pattern
                                reset_LED();
                                printf("Applying low battery pattern...\n");
                                ptmsysmSetBatteryEmptyLedPattern(LBP);
                                printf("Pattern applied ! No need to reboot\n");
                            break;

                            default:
                            break;
                        }
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