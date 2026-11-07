#include <pspkernel.h>
#include <psploadexec.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

PSP_MODULE_INFO("The Cat Is Locked", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(2048);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272
#define SAVE_DIR "ms0:/PSP/SAVEDATA/TCLCK0001"
#define SAVE_PATH SAVE_DIR "/DATA.BIN"
#define SAVE_MAGIC 0x54434C4B
#define SAVE_VERSION 2
#define SAVE_SLOTS 5

typedef struct { unsigned int color; float x, y, z; } Vertex3D;
typedef struct { unsigned int color; float x, y, z; } Vertex2D;
typedef struct { float u, v; unsigned int color; float x, y, z; } VertexTex2D;

typedef struct {
    int valid;
    int level;
    int world;
    int lives;
    int difficulty;
    float catX;
    float catZ;
    int health;
    int keysCollected;
    int keyPicked[2];
    int trapTriggered[4];
} SaveSlot;

typedef struct {
    unsigned int magic;
    unsigned int version;
    SaveSlot slots[SAVE_SLOTS];
} SaveFile;

extern unsigned char menu_main_texture[];
extern unsigned char menu_play_resume_texture[];
extern unsigned char menu_play_new_texture[];
extern unsigned char menu_settings_texture[];
extern unsigned char menu_resume_texture[];
extern unsigned char menu_arrow_left_texture[];
extern unsigned char menu_arrow_right_texture[];

static unsigned int __attribute__((aligned(16))) displayList[262144];
static int menu = 0; /* 0 accueil, 1 jouer, 2 paramètres, 3 confirmation, 4 jeu, 5 reprise */
static int selected = 0;
static int soundOn = 1;
static int volume = 70;
static float catX = 0.0f, catZ = 0.0f;
static int keyFound = 0;
static int keysCollected = 0;
static int keyPicked[2] = {0,0};
static int health = 100;
static int difficulty = 1;
static int lives = 3;
static int currentWorld = 1;
static int currentLevel = 1;
static int trainingMode = 0;
static int trapTriggered[4] = {0,0,0,0};
static int currentSlot = -1;
static SaveSlot saveSlots[SAVE_SLOTS];
static int messageCode = 0;
static int messageFrames = 0;
static void *renderBuffer = (void*)0;

static const float keyPos[2][2] = {{-2.5f,-2.2f},{2.5f,-2.2f}};
static const float trapPos[4][2] = {{-2.0f,0.6f},{-0.7f,1.6f},{0.8f,0.6f},{2.0f,1.6f}};

static Vertex3D cube[36] __attribute__((aligned(16))) = {
 {0xffffffff,-.5f,-.5f,-.5f},{0xffffffff,.5f,-.5f,-.5f},{0xffffffff,.5f,.5f,-.5f},
 {0xffffffff,-.5f,-.5f,-.5f},{0xffffffff,.5f,.5f,-.5f},{0xffffffff,-.5f,.5f,-.5f},
 {0xffffffff,.5f,-.5f,.5f},{0xffffffff,-.5f,-.5f,.5f},{0xffffffff,-.5f,.5f,.5f},
 {0xffffffff,.5f,-.5f,.5f},{0xffffffff,-.5f,.5f,.5f},{0xffffffff,.5f,.5f,.5f},
 {0xffffffff,-.5f,-.5f,.5f},{0xffffffff,-.5f,-.5f,-.5f},{0xffffffff,-.5f,.5f,-.5f},
 {0xffffffff,-.5f,-.5f,.5f},{0xffffffff,-.5f,.5f,-.5f},{0xffffffff,-.5f,.5f,.5f},
 {0xffffffff,.5f,-.5f,-.5f},{0xffffffff,.5f,-.5f,.5f},{0xffffffff,.5f,.5f,.5f},
 {0xffffffff,.5f,-.5f,-.5f},{0xffffffff,.5f,.5f,.5f},{0xffffffff,.5f,.5f,-.5f},
 {0xffffffff,-.5f,.5f,-.5f},{0xffffffff,.5f,.5f,-.5f},{0xffffffff,.5f,.5f,.5f},
 {0xffffffff,-.5f,.5f,-.5f},{0xffffffff,.5f,.5f,.5f},{0xffffffff,-.5f,.5f,.5f},
 {0xffffffff,-.5f,-.5f,-.5f},{0xffffffff,-.5f,-.5f,.5f},{0xffffffff,.5f,-.5f,.5f},
 {0xffffffff,-.5f,-.5f,-.5f},{0xffffffff,.5f,-.5f,.5f},{0xffffffff,.5f,-.5f,-.5f}
};

static void drawCube(float x,float y,float z,float sx,float sy,float sz,unsigned int color) {
    int i;
    for(i=0;i<36;i++) cube[i].color=color;
    sceKernelDcacheWritebackRange(cube,sizeof(cube));
    sceGumPushMatrix();
    ScePspFVector3 p={x,y,z}, s={sx,sy,sz};
    sceGumTranslate(&p);
    sceGumScale(&s);
    sceGumDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,36,0,cube);
    sceGumPopMatrix();
}

static void drawRect(float x1,float y1,float x2,float y2,unsigned int color) {
    Vertex2D v[2] __attribute__((aligned(16)));
    v[0].color=color; v[0].x=x1; v[0].y=y1; v[0].z=0.0f;
    v[1].color=color; v[1].x=x2; v[1].y=y2; v[1].z=0.0f;
    sceKernelDcacheWritebackRange(v,sizeof(v));
    sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
}

static void drawTriangle(float x1,float y1,float x2,float y2,float x3,float y3,unsigned int color) {
    Vertex2D v[3] __attribute__((aligned(16)));
    v[0].color=color; v[0].x=x1; v[0].y=y1; v[0].z=0.0f;
    v[1].color=color; v[1].x=x2; v[1].y=y2; v[1].z=0.0f;
    v[2].color=color; v[2].x=x3; v[2].y=y3; v[2].z=0.0f;
    sceKernelDcacheWritebackRange(v,sizeof(v));
    sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,3,0,v);
}

static void drawArrowSprite(const unsigned char *pixels,float x,float y) {
    VertexTex2D v[2] __attribute__((aligned(16)));
    v[0].u=0.0f; v[0].v=0.0f; v[0].color=0xffffffff;
    v[0].x=x; v[0].y=y; v[0].z=0.0f;
    v[1].u=128.0f; v[1].v=64.0f; v[1].color=0xffffffff;
    v[1].x=x+38.0f; v[1].y=y+19.0f; v[1].z=0.0f;
    sceKernelDcacheWritebackRange(v,sizeof(v));
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888,0,0,0);
    sceGuTexImage(0,128,64,128,pixels);
    sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR,GU_LINEAR);
    sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
    sceGuDisable(GU_TEXTURE_2D);
}
static void drawArrowRight(float x,float y) { drawArrowSprite(menu_arrow_right_texture,x,y); }
static void drawArrowLeft(float x,float y) { drawArrowSprite(menu_arrow_left_texture,x,y); }

static void begin2D(void) {
    sceGuDisable(GU_DEPTH_TEST);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
}

static void drawTexture(const unsigned char *pixels,int textureHeight,int imageHeight,float y) {
    VertexTex2D v[2] __attribute__((aligned(16)));
    v[0].u=0.0f; v[0].v=0.0f; v[0].color=0xffffffff;
    v[0].x=0.0f; v[0].y=y; v[0].z=0.0f;
    v[1].u=480.0f; v[1].v=(float)imageHeight; v[1].color=0xffffffff;
    v[1].x=480.0f; v[1].y=y+(float)imageHeight; v[1].z=0.0f;
    sceKernelDcacheWritebackRange(v,sizeof(v));
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888,0,0,0);
    sceGuTexImage(0,512,textureHeight,512,pixels);
    sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR,GU_LINEAR);
    sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
    sceGuDisable(GU_TEXTURE_2D);
}

/* Clean gameplay prototype: deliberately 2D first, to establish a reliable visible frame.
   Menus remain unchanged. This is a scaffold for rebuilding gameplay incrementally. */
static void drawGameplayScreen(void) {
    begin2D();

    /* Room backdrop and simple perspective-inspired walls/floor */
    drawRect(0,0,480,272,0xff292f3b);
    drawRect(0,0,480,145,0xff424d61);
    drawRect(0,145,480,272,0xff80664f);
    drawRect(0,145,480,153,0xffb7a18a);
    drawRect(32,42,448,145,0xff6b7180);
    drawRect(40,50,440,137,0xff8c929f);
    drawRect(40,50,440,57,0xffaab0ba);
    drawRect(40,130,440,137,0xff737987);

    /* Door clearly visible at the far end */
    drawRect(194,62,286,145,0xff3c2b22);
    drawRect(201,68,279,145,0xff8b5d3b);
    drawRect(207,74,273,139,0xffa8784e);
    drawRect(259,104,267,112,0xffffd36a);

    /* Cat placeholder: a readable stylized sprite, moved by the D-pad/stick */
    {
        float sx=catX*34.0f+240.0f;
        float sy=catZ*20.0f+190.0f;
        drawRect(sx-18,sy-17,sx+18,sy+12,0xff5a514b);
        drawRect(sx-13,sy-31,sx+13,sy-8,0xff5a514b);
        drawTriangle(sx-13,sy-26,sx-8,sy-39,sx-2,sy-26,0xff302b28);
        drawTriangle(sx+2,sy-26,sx+8,sy-39,sx+13,sy-26,0xff302b28);
        drawRect(sx-8,sy-21,sx-4,sy-17,0xffffd36a);
        drawRect(sx+4,sy-21,sx+8,sy-17,0xffffd36a);
        drawRect(sx-12,sy+9,sx-5,sy+20,0xff302b28);
        drawRect(sx+5,sy+9,sx+12,sy+20,0xff302b28);
        drawRect(sx+16,sy-1,sx+29,sy+4,0xff5a514b);
    }

    /* Minimal HUD; no legacy keys, traps, health, or progression mechanics. */
    drawRect(8,8,174,32,0xd9000000);
    drawRect(14,14,168,26,0xffd8d8d8);
    drawRect(18,18,164,22,0xff4c9c5a);
    drawRect(8,238,200,264,0xd9000000);
    drawRect(14,244,194,258,0xff333b48);
}
static void drawMenuScreen(void) {
    begin2D();
    if(menu==0) {
        drawTexture(menu_main_texture,512,272,0.0f);
        if(selected==0) { drawArrowRight(20,151); drawArrowLeft(180,151); }
        else if(selected==1) { drawArrowRight(20,190); drawArrowLeft(180,190); }
        else { drawArrowRight(20,229); drawArrowLeft(180,229); }
    } else if(menu==1) {
        if(selected==0) drawTexture(menu_play_resume_texture,512,272,0.0f);
        else drawTexture(menu_play_new_texture,512,272,0.0f);
    } else if(menu==2) {
        drawTexture(menu_settings_texture,512,272,0.0f);
        if(selected==0) { drawArrowRight(239,76); drawArrowLeft(431,76); }
        else if(selected==1) { drawArrowRight(239,132); drawArrowLeft(431,132); }
        else { drawArrowRight(239,181); drawArrowLeft(431,181); }
        drawRect(291,141,454,145,0xff777777);
        if(volume>0) drawRect(291,141,291+(163.0f*(float)volume/100.0f),145,0xffffd34e);
        drawRect(415,83,454,103,soundOn?0xff3c8c38:0xff555555);
        drawRect(soundOn?438:418,86,soundOn?450:430,100,0xffd8d8d8);
    } else if(menu==3) {
        drawTexture(menu_settings_texture,512,272,0.0f);
        drawRect(52,65,428,210,0xff242a30);
        drawRect(58,71,422,204,0xff4b5158);
        drawRect(112,127,368,155,selected==0?0xff9a6637:0xff354352);
        drawRect(112,165,368,193,selected==1?0xff9a6637:0xff354352);
        if(selected==0) { drawArrowRight(80,132); drawArrowLeft(370,132); }
        else { drawArrowRight(80,170); drawArrowLeft(370,170); }
    } else if(menu==5) {
        int i;
        drawTexture(menu_resume_texture,512,272,0.0f);
        for(i=0;i<SAVE_SLOTS;i++) {
            static const float rowY[SAVE_SLOTS]={33.0f,73.0f,121.0f,169.0f,217.0f};
            float y=rowY[i];
            if(selected==i) { drawArrowRight(245,y); drawArrowLeft(420,y); }
        }
    }
}

static void makeSaveDirectory(void) {
    sceIoMkdir("ms0:/PSP",0777);
    sceIoMkdir("ms0:/PSP/SAVEDATA",0777);
    sceIoMkdir(SAVE_DIR,0777);
}

static void loadAllSaves(void) {
    SaveFile file;
    SceUID fd;
    memset(saveSlots,0,sizeof(saveSlots));
    fd=sceIoOpen(SAVE_PATH,PSP_O_RDONLY,0777);
    if(fd<0) return;
    memset(&file,0,sizeof(file));
    if(sceIoRead(fd,&file,sizeof(file))==(int)sizeof(file) &&
       file.magic==SAVE_MAGIC && file.version==SAVE_VERSION) {
        memcpy(saveSlots,file.slots,sizeof(saveSlots));
    }
    sceIoClose(fd);
}

static void saveAllSaves(void) {
    SaveFile file;
    SceUID fd;
    makeSaveDirectory();
    memset(&file,0,sizeof(file));
    file.magic=SAVE_MAGIC;
    file.version=SAVE_VERSION;
    memcpy(file.slots,saveSlots,sizeof(saveSlots));
    fd=sceIoOpen(SAVE_PATH,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);
    if(fd<0) return;
    sceIoWrite(fd,&file,sizeof(file));
    sceIoClose(fd);
}

static int findEmptySlot(void) {
    int i;
    for(i=0;i<SAVE_SLOTS;i++) if(!saveSlots[i].valid) return i;
    return -1;
}

static void resetGameState(void) {
    int i;
    catX=0.0f; catZ=0.0f;
    keyFound=0; keysCollected=0; health=100;
    currentWorld=1; currentLevel=1;
    trainingMode=(difficulty==5);
    lives=(difficulty==0)?5:((difficulty==1)?3:((difficulty==2)?2:((difficulty==3)?1:((difficulty==4)?1:-1))));
    keyPicked[0]=0; keyPicked[1]=0;
    for(i=0;i<4;i++) trapTriggered[i]=0;
}

static void storeCurrentSlot(void) {
    SaveSlot *slot;
    if(currentSlot<0 || currentSlot>=SAVE_SLOTS) return;
    slot=&saveSlots[currentSlot];
    slot->valid=1;
    slot->level=currentLevel;
    slot->world=currentWorld;
    slot->lives=lives;
    slot->difficulty=difficulty;
    slot->catX=catX;
    slot->catZ=catZ;
    slot->health=health;
    slot->keysCollected=keysCollected;
    slot->keyPicked[0]=keyPicked[0];
    slot->keyPicked[1]=keyPicked[1];
    memcpy(slot->trapTriggered,trapTriggered,sizeof(trapTriggered));
    saveAllSaves();
}

static void startNewGame(void) {
    int slot=findEmptySlot();
    if(slot<0) {
        messageCode=1;
        messageFrames=180;
        return;
    }
    resetGameState();
    currentSlot=slot;
    storeCurrentSlot();
    menu=4;
}

static void loadSaveSlot(int slotIndex) {
    SaveSlot *slot;
    if(slotIndex<0 || slotIndex>=SAVE_SLOTS || !saveSlots[slotIndex].valid) {
        messageCode=2;
        messageFrames=150;
        return;
    }
    slot=&saveSlots[slotIndex];
    currentSlot=slotIndex;
    currentLevel=slot->level; currentWorld=slot->world;
    lives=slot->lives; difficulty=slot->difficulty; trainingMode=(difficulty==5);
    catX=slot->catX; catZ=slot->catZ;
    health=slot->health; keysCollected=slot->keysCollected;
    keyPicked[0]=slot->keyPicked[0]; keyPicked[1]=slot->keyPicked[1];
    memcpy(trapTriggered,slot->trapTriggered,sizeof(trapTriggered));
    keyFound=(keysCollected>=2);
    menu=4;
}

static void resetAllSaves(void) {
    memset(saveSlots,0,sizeof(saveSlots));
    currentSlot=-1;
    sceIoRemove(SAVE_PATH);
}

static void printMenuText(void) {
    int i;
    pspDebugScreenSetTextColor(0xffffffff);
    if(menu==6) {
        static const char *names[6]={"FACILE - 5 VIES","NORMAL - 3 VIES","DIFFICILE - 2 VIES","EXTREME - 1 VIE","HARDCORE - 1 ESSAI","ENTRAINEMENT - INFINI"};
        pspDebugScreenSetXY(10,7); pspDebugScreenPrintf("CHOISIR LA DIFFICULTE");
        for(i=0;i<6;i++) { pspDebugScreenSetXY(8,11+i*3); pspDebugScreenPrintf("%s %s",selected==i?">":" ",names[i]); }
    } else if(menu==7) {
        pspDebugScreenSetXY(18,13); pspDebugScreenPrintf("GAME OVER");
        pspDebugScreenSetXY(7,17); pspDebugScreenPrintf("CROIX : RETOUR AU MENU PRINCIPAL");
    } else if(menu==3) {
        pspDebugScreenSetXY(10,10);
        pspDebugScreenPrintf("REINITIALISER TOUTES LES SAUVEGARDES ?");
        pspDebugScreenSetXY(21,17);
        pspDebugScreenPrintf("%s NON",selected==0?">":" ");
        pspDebugScreenSetXY(21,22);
        pspDebugScreenPrintf("%s OUI, TOUT EFFACER",selected==1?">":" ");
    } else if(menu==5) {
        for(i=0;i<SAVE_SLOTS;i++) {
            static const int rowLines[SAVE_SLOTS]={6,11,17,23,29};
            pspDebugScreenSetXY(35,rowLines[i]);
            if(saveSlots[i].valid)
                pspDebugScreenPrintf("Niveau %d, Monde %d",saveSlots[i].level,saveSlots[i].world);
            else
                pspDebugScreenPrintf("Emplacement vide");
        }
    } else if(menu==4) {
        pspDebugScreenSetXY(1,1); pspDebugScreenPrintf("GAMEPLAY BASE PROPRE - MONDE %d / NIVEAU %d",currentWorld,currentLevel);
        pspDebugScreenSetXY(1,2); pspDebugScreenPrintf("D-PAD / STICK : DEPLACER LE CHAT | ROND : MENU");
    }
    if(messageFrames>0) {
        pspDebugScreenSetTextColor(0xffffff00);
        pspDebugScreenSetXY(8,30);
        if(messageCode==1) pspDebugScreenPrintf("5 SAUVEGARDES MAXIMUM : REINITIALISEZ POUR RECOMMENCER");
        else if(messageCode==2) pspDebugScreenPrintf("CET EMPLACEMENT EST VIDE");
    }
}

static void begin3D(void) {
    sceGuStart(GU_DIRECT,displayList);
    sceGuOffset(2048-(SCR_WIDTH/2),2048-(SCR_HEIGHT/2));
    sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuClearColor(0xff000000);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(48.0f,16.0f/9.0f,0.5f,100.0f);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    ScePspFVector3 eye={0.0f,5.5f,10.0f}, target={0.0f,0.5f,0.0f}, up={0.0f,1.0f,0.0f};
    sceGumLookAt(&eye,&target,&up);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

static void drawFrame(void) {
    if(menu==4) {
        begin3D();
        drawGameplayScreen();
    } else {
        begin3D();
        drawMenuScreen();
    }
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);

    /* Keep debug text on the newly selected render buffer, as in PSPSDK samples. */
    sceDisplayWaitVblankStart();
    renderBuffer=sceGuSwapBuffers();
    if(menu==3 || menu==4 || menu==5 || menu==6 || menu==7 || messageFrames>0) {
        pspDebugScreenSetOffset((int)renderBuffer);
        printMenuText();
    }
}

static int exitCallback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    return 0;
}

static int callbackThread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int callbackId=sceKernelCreateCallback("Exit Callback",exitCallback,NULL);
    if(callbackId>=0) sceKernelRegisterExitCallback(callbackId);
    sceKernelSleepThreadCB();
    return 0;
}

static void setupCallbacks(void) {
    int threadId=sceKernelCreateThread("Exit Callback Thread",callbackThread,0x11,0x1000,0,0);
    if(threadId>=0) sceKernelStartThread(threadId,0,0);
}

int main(void) {
    setupCallbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    pspDebugScreenInit();
    sceGuInit();
    sceGuStart(GU_DIRECT,displayList);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,(void*)0x88000,BUF_WIDTH);
    sceGuDepthBuffer((void*)0x110000,BUF_WIDTH);
    sceGuOffset(2048-(SCR_WIDTH/2),2048-(SCR_HEIGHT/2));
    sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
    sceGuFinish(); sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    loadAllSaves();

    while(1) {
        SceCtrlData pad;
        int changed=0;
        sceCtrlReadBufferPositive(&pad,1);
        unsigned int b=pad.Buttons;
        static unsigned int old=0;
        unsigned int press=b & ~old;
        old=b;

        if(menu==4) {
            /* Fresh minimal gameplay controls: movement only; Circle returns to menu. */
            if((b & PSP_CTRL_LEFT) || pad.Lx<78) catX-=.06f;
            if((b & PSP_CTRL_RIGHT) || pad.Lx>177) catX+=.06f;
            if((b & PSP_CTRL_UP) || pad.Ly<78) catZ-=.06f;
            if((b & PSP_CTRL_DOWN) || pad.Ly>177) catZ+=.06f;
            if(catX < -5.0f) catX=-5.0f;
            if(catX > 5.0f) catX=5.0f;
            if(catZ < -5.0f) catZ=-5.0f;
            if(catZ > 3.0f) catZ=3.0f;
            if(press & PSP_CTRL_CIRCLE) {
                menu=0;
                selected=0;
            }
        } else {
            int max=(menu==0)?2:((menu==1 || menu==3)?1:((menu==2)?2:((menu==6)?5:4)));
            if(press & PSP_CTRL_UP) selected--;
            if(press & PSP_CTRL_DOWN) selected++;
            if(selected<0) selected=max;
            if(selected>max) selected=0;

            if(menu==2 && selected==0 && (press & (PSP_CTRL_LEFT|PSP_CTRL_RIGHT|PSP_CTRL_CROSS)))
                soundOn=!soundOn;
            if(menu==2 && selected==1 && (press & PSP_CTRL_LEFT)) { volume-=5; if(volume<0) volume=0; }
            if(menu==2 && selected==1 && (press & PSP_CTRL_RIGHT)) { volume+=5; if(volume>100) volume=100; }

            if(press & PSP_CTRL_CIRCLE) {
                if(menu==0) { }
                else if(menu==3) { menu=2; selected=0; }
                else if(menu==6) { menu=1; selected=1; }
                else { menu=0; selected=0; }
            }
            if(press & PSP_CTRL_CROSS) {
                if(menu==0) {
                    if(selected==0) { menu=1; selected=0; }
                    else if(selected==1) { menu=2; selected=0; }
                    else sceKernelExitGame();
                } else if(menu==1) {
                    if(selected==0) { menu=5; selected=0; }
                    else { menu=6; selected=1; }
                } else if(menu==6) {
                    difficulty=selected;
                    startNewGame();
                } else if(menu==7) {
                    resetGameState(); currentSlot=-1; menu=0; selected=0;
                } else if(menu==2) {
                    if(selected==2) { menu=3; selected=0; }
                } else if(menu==3) {
                    if(selected==1) resetAllSaves();
                    menu=2; selected=0;
                } else if(menu==5) {
                    loadSaveSlot(selected);
                }
            }
        }
        if(messageFrames>0) messageFrames--;
        drawFrame();
    }
    sceKernelExitGame();
    return 0;
}
