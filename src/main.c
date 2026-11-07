#include <pspkernel.h>
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
#define SAVE_VERSION 1
#define SAVE_SLOTS 5

typedef struct { unsigned int color; float x, y, z; } Vertex3D;
typedef struct { unsigned int color; float x, y, z; } Vertex2D;
typedef struct { float u, v; unsigned int color; float x, y, z; } VertexTex2D;

typedef struct {
    int valid;
    int level;
    int world;
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
static int trapTriggered[4] = {0,0,0,0};
static int currentSlot = -1;
static SaveSlot saveSlots[SAVE_SLOTS];
static int messageCode = 0;
static int messageFrames = 0;
static void *renderBuffer = (void*)0;

static const float keyPos[2][2] = {{-2.5f,-2.2f},{2.5f,-2.2f}};
static const float trapPos[4][2] = {{-2.0f,0.6f},{-0.7f,1.6f},{0.8f,0.6f},{2.0f,1.6f}};

static Vertex3D cube[36] = {
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
    sceGumPushMatrix();
    ScePspFVector3 p={x,y,z}, s={sx,sy,sz};
    sceGumTranslate(&p);
    sceGumScale(&s);
    sceGumDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,36,0,cube);
    sceGumPopMatrix();
}

static void drawRect(float x1,float y1,float x2,float y2,unsigned int color) {
    Vertex2D v[2];
    v[0].color=color; v[0].x=x1; v[0].y=y1; v[0].z=0.0f;
    v[1].color=color; v[1].x=x2; v[1].y=y2; v[1].z=0.0f;
    sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
}

static void drawTriangle(float x1,float y1,float x2,float y2,float x3,float y3,unsigned int color) {
    Vertex2D v[3];
    v[0].color=color; v[0].x=x1; v[0].y=y1; v[0].z=0.0f;
    v[1].color=color; v[1].x=x2; v[1].y=y2; v[1].z=0.0f;
    v[2].color=color; v[2].x=x3; v[2].y=y3; v[2].z=0.0f;
    sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,3,0,v);
}

static void drawArrowRight(float x,float y) {
    unsigned int gold=0xffffd34e;
    drawRect(x,y+7,x+19,y+11,gold);
    drawTriangle(x+17,y,x+30,y+9,x+17,y+18,gold);
}

static void drawArrowLeft(float x,float y) {
    unsigned int gold=0xffffd34e;
    drawRect(x+11,y+7,x+30,y+11,gold);
    drawTriangle(x+13,y,x,y+9,x+13,y+18,gold);
}

static void begin2D(void) {
    sceGuDisable(GU_DEPTH_TEST);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
}

static void drawTexture(const unsigned char *pixels,int textureHeight,int imageHeight,float y) {
    VertexTex2D v[2];
    v[0].u=0.0f; v[0].v=0.0f; v[0].color=0xffffffff;
    v[0].x=0.0f; v[0].y=y; v[0].z=0.0f;
    v[1].u=480.0f; v[1].v=(float)imageHeight; v[1].color=0xffffffff;
    v[1].x=480.0f; v[1].y=y+(float)imageHeight; v[1].z=0.0f;
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888,0,0,0);
    sceGuTexImage(0,512,textureHeight,512,pixels);
    sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR,GU_LINEAR);
    sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
    sceGuDisable(GU_TEXTURE_2D);
}

static void drawRoom(void) {
    drawCube(0,-.35f,0,10,.35f,10,0xffeeeeee);
    drawCube(-2.9f,1.7f,-5,4.2f,4,.25f,0xffa64f30);
    drawCube(2.9f,1.7f,-5,4.2f,4,.25f,0xffa64f30);
    drawCube(0,3.0f,-5,1.6f,1.4f,.25f,0xffa64f30);
    drawCube(-5,1.7f,0,.25f,4,10,0xffbd633d);
    drawCube(5,1.7f,0,.25f,4,10,0xffbd633d);
    drawCube(0,3.7f,0,10,.2f,10,0xffd98a55);
    drawCube(-.82f,1.05f,-4.82f,.12f,2.2f,.12f,0xff493023);
    drawCube(.82f,1.05f,-4.82f,.12f,2.2f,.12f,0xff493023);
    drawCube(0,2.15f,-4.82f,1.75f,.12f,.12f,0xff493023);
    drawCube(0,1.05f,-4.88f,1.55f,2.05f,.08f,0xff805233);
    drawCube(.48f,1.05f,-4.72f,.10f,.16f,.08f,0xffffd36a);
    drawCube(-2.7f,2.3f,-4.78f,1.6f,1.0f,.06f,0xff7ac9e8);
    drawCube(2.7f,2.3f,-4.78f,1.6f,1.0f,.06f,0xff7ac9e8);
    if(!keyPicked[0]) drawCube(keyPos[0][0],.12f,keyPos[0][1],.22f,.12f,.22f,0xffffd34e);
    if(!keyPicked[1]) drawCube(keyPos[1][0],.12f,keyPos[1][1],.22f,.12f,.22f,0xffffd34e);
    drawCube(trapPos[0][0],-.14f,trapPos[0][1],.75f,.07f,.75f,trapTriggered[0]?0xff555555:0xffc83232);
    drawCube(trapPos[1][0],-.14f,trapPos[1][1],.75f,.07f,.75f,trapTriggered[1]?0xff555555:0xffc83232);
    drawCube(trapPos[2][0],-.14f,trapPos[2][1],.75f,.07f,.75f,trapTriggered[2]?0xff555555:0xffc83232);
    drawCube(trapPos[3][0],-.14f,trapPos[3][1],.75f,.07f,.75f,trapTriggered[3]?0xff555555:0xffc83232);
}

static void drawCat(void) {
    drawCube(catX,.43f,catZ,.72f,.42f,1.15f,0xff5a514b);
    drawCube(catX,.72f,catZ-.62f,.52f,.48f,.50f,0xff5a514b);
    drawCube(catX-.19f,1.02f,catZ-.80f,.13f,.30f,.13f,0xff302b28);
    drawCube(catX+.19f,1.02f,catZ-.80f,.13f,.30f,.13f,0xff302b28);
    drawCube(catX-.14f,.78f,catZ-.87f,.06f,.06f,.04f,0xffffd36a);
    drawCube(catX+.14f,.78f,catZ-.87f,.06f,.06f,.04f,0xffffd36a);
    drawCube(catX,.63f,catZ-.91f,.07f,.05f,.05f,0xffc98791);
    drawCube(catX-.24f,.18f,catZ-.40f,.14f,.32f,.16f,0xff302b28);
    drawCube(catX+.24f,.18f,catZ-.40f,.14f,.32f,.16f,0xff302b28);
    drawCube(catX-.24f,.18f,catZ+.40f,.14f,.32f,.16f,0xff302b28);
    drawCube(catX+.24f,.18f,catZ+.40f,.14f,.32f,.16f,0xff302b28);
    drawCube(catX,.50f,catZ+.70f,.10f,.10f,.45f,0xff5a514b);
    drawCube(catX,.58f,catZ+1.02f,.09f,.09f,.22f,0xff5a514b);
}

static void drawGameHud(void) {
    begin2D();
    drawRect(12,12,178,28,0xd9000000);
    drawRect(18,17,172,23,0xff333333);
    if(health>0) drawRect(18,17,18+(154.0f*(float)health/100.0f),23,health>50?0xff39c85a:(health>25?0xffffb52e:0xffe83d3d));
    drawRect(12,34,70,48,0xd9000000);
    drawRect(20,37,36,45,keysCollected>0?0xffffd34e:0xff555555);
    drawRect(42,37,58,45,keysCollected>1?0xffffd34e:0xff555555);
}

static void drawMenuScreen(void) {
    begin2D();
    if(menu==0) {
        drawTexture(menu_main_texture,256,256,8.0f);
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
    keyPicked[0]=0; keyPicked[1]=0;
    for(i=0;i<4;i++) trapTriggered[i]=0;
}

static void storeCurrentSlot(void) {
    SaveSlot *slot;
    if(currentSlot<0 || currentSlot>=SAVE_SLOTS) return;
    slot=&saveSlots[currentSlot];
    slot->valid=1;
    slot->level=1;
    slot->world=1;
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
    if(menu==3) {
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
        pspDebugScreenSetXY(1,1); pspDebugScreenPrintf("NIVEAU 1 - LA PREMIERE PIECE");
        pspDebugScreenSetXY(1,2); pspDebugScreenPrintf("Fleches/stick : bouger   Rond : sauvegarder/menu");
        pspDebugScreenSetXY(1,3); pspDebugScreenPrintf("CLES : %d/2  |  PORTE : %s",keysCollected,keyFound?"OUVERTE":"VERROUILLEE");
        pspDebugScreenSetXY(1,4); pspDebugScreenPrintf("VIE : %d%%",health);
    }
    if(messageFrames>0) {
        pspDebugScreenSetTextColor(0xffffff00);
        pspDebugScreenSetXY(8,30);
        if(messageCode==1) pspDebugScreenPrintf("5 SAUVEGARDES MAXIMUM : REINITIALISEZ POUR RECOMMENCER");
        else if(messageCode==2) pspDebugScreenPrintf("CET EMPLACEMENT EST VIDE");
    }
}

static void drawFrame(void) {
    begin3D();
    if(menu==4) {
        drawRoom();
        drawCat();
        drawGameHud();
    } else {
        drawMenuScreen();
    }
    sceGuFinish();
    sceGuSync(0,0);
    if(menu==3 || menu==4 || menu==5 || messageFrames>0) {
        pspDebugScreenSetOffset((int)renderBuffer);
        printMenuText();
    }
    sceDisplayWaitVblankStart();
    renderBuffer=sceGuSwapBuffers();
}

int main(void) {
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
            if((b & PSP_CTRL_LEFT) || pad.Lx<78) catX-=.045f;
            if((b & PSP_CTRL_RIGHT) || pad.Lx>177) catX+=.045f;
            if((b & PSP_CTRL_UP) || pad.Ly<78) catZ-=.045f;
            if((b & PSP_CTRL_DOWN) || pad.Ly>177) catZ+=.045f;
            {
                int i;
                for(i=0;i<2;i++) {
                    float dx=catX-keyPos[i][0], dz=catZ-keyPos[i][1];
                    if((dx*dx+dz*dz)<0.36f && !keyPicked[i]) {
                        keyPicked[i]=1; keysCollected++; changed=1;
                    }
                }
                for(i=0;i<4;i++) {
                    float dx=catX-trapPos[i][0], dz=catZ-trapPos[i][1];
                    if(!trapTriggered[i] && (dx*dx+dz*dz)<0.30f) {
                        trapTriggered[i]=1;
                        health-=25;
                        if(health<0) health=0;
                        changed=1;
                    }
                }
                keyFound=(keysCollected>=2);
            }
            if(changed) storeCurrentSlot();
            if(press & PSP_CTRL_CIRCLE) {
                storeCurrentSlot();
                menu=0; selected=0;
            }
            if(catX < -4.0f) catX=-4.0f;
            if(catX > 4.0f) catX=4.0f;
            if(catZ < -4.2f) catZ=-4.2f;
            if(catZ > 3.5f) catZ=3.5f;
        } else {
            int max=(menu==0)?2:((menu==1 || menu==3)?1:((menu==2)?2:4));
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
                else { menu=0; selected=0; }
            }
            if(press & PSP_CTRL_CROSS) {
                if(menu==0) {
                    if(selected==0) { menu=1; selected=0; }
                    else if(selected==1) { menu=2; selected=0; }
                    else sceKernelExitGame();
                } else if(menu==1) {
                    if(selected==0) { menu=5; selected=0; }
                    else { startNewGame(); }
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
