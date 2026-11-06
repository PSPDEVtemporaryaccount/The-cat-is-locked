#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <string.h>
#include <stdio.h>

PSP_MODULE_INFO("The Cat Is Locked", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(2048);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

typedef struct { unsigned int color; float x, y, z; } Vertex3D;
typedef struct { unsigned int color; float x, y, z; } Vertex2D;

static unsigned int __attribute__((aligned(16))) displayList[262144];
static int menu = 0; /* 0 accueil, 1 jouer, 2 paramètres, 3 confirmation, 4 jeu */
static int selected = 0;
static int hasSave = 0;
static int soundOn = 1;
static float catX = 0.0f, catZ = 0.0f;
static int keyFound = 0;
static int keysCollected = 0;
static int keyPicked[2] = {0,0};
static void *renderBuffer = (void*)0;
static int health = 100;
static int trapTriggered[4] = {0,0,0,0};
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
    /* Objets de gameplay visibles : clés dorées et plaques de pièges rouges. */
    drawCube(keyPos[0][0],.12f,keyPos[0][1],.22f,.12f,.22f,0xffffd34e);
    drawCube(keyPos[1][0],.12f,keyPos[1][1],.22f,.12f,.22f,0xffffd34e);
    drawCube(trapPos[0][0],-.14f,trapPos[0][1],.75f,.07f,.75f,trapTriggered[0]?0xff555555:0xffc83232);
    drawCube(trapPos[1][0],-.14f,trapPos[1][1],.75f,.07f,.75f,trapTriggered[1]?0xff555555:0xffc83232);
    drawCube(trapPos[2][0],-.14f,trapPos[2][1],.75f,.07f,.75f,trapTriggered[2]?0xff555555:0xffc83232);
    drawCube(trapPos[3][0],-.14f,trapPos[3][1],.75f,.07f,.75f,trapTriggered[3]?0xff555555:0xffc83232);
}

static void drawCat(void) {
    /* Silhouette feline moins "lama" : corps allonge, tete, oreilles, quatre pattes et queue. */
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

static void drawMenuPanel(void) {
    /* Passage en 2D pour dessiner une vraie interface par-dessus la pièce. */
    sceGuDisable(GU_DEPTH_TEST);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();

    drawRect(82,24,398,250,0xe91b2430);
    drawRect(88,30,392,244,0xff29394a);
    drawRect(96,38,384,88,0xffb85d35);
    drawRect(96,94,384,238,0xff182431);

    if(menu==0) {
        int i;
        for(i=0;i<3;i++) {
            unsigned int c=(selected==i)?0xffd88a45:0xff344b60;
            drawRect(132,111+i*39,348,142+i*39,c);
            if(selected==i) drawRect(132,111+i*39,139,142+i*39,0xffffd36a);
        }
    } else {
        int count=(menu==1)?2:(menu==2?3:2);
        int i;
        for(i=0;i<count;i++) {
            unsigned int c=(selected==i)?0xffd88a45:0xff344b60;
            drawRect(116,112+i*39,364,143+i*39,c);
            if(selected==i) drawRect(116,112+i*39,123,143+i*39,0xffffd36a);
        }
    }
    sceGuEnable(GU_DEPTH_TEST);
}

static void drawGameHud(void) {
    sceGuDisable(GU_DEPTH_TEST);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
    drawRect(12,12,178,28,0xd9000000);
    drawRect(18,17,172,23,0xff333333);
    if(health>0) drawRect(18,17,18+(154.0f*(float)health/100.0f),23,health>50?0xff39c85a:(health>25?0xffffb52e:0xffe83d3d));
    drawRect(12,34,70,48,0xd9000000);
    drawRect(20,37,36,45,keysCollected>0?0xffffd34e:0xff555555);
    drawRect(42,37,58,45,keysCollected>1?0xffffd34e:0xff555555);
    sceGuEnable(GU_DEPTH_TEST);
}

static void drawFrame(void) {
    begin3D();
    drawRoom();
    if(menu==4) { drawCat(); drawGameHud(); }
    else {
        /* Silhouette d'ambiance provisoire dans le décor du menu. */
        drawCube(0,.22f,1.4f,.75f,.45f,.55f,0xff38312d);
        drawCube(0,.58f,1.0f,.42f,.38f,.38f,0xff4b423c);
    }
    if(menu!=4) drawMenuPanel();
    sceGuFinish();
    sceGuSync(0,0);
    /* Dessiner les libellés dans le buffer GU courant, juste avant le swap. */
    pspDebugScreenSetOffset((int)renderBuffer);
    printMenu();
    sceDisplayWaitVblankStart();
    renderBuffer=sceGuSwapBuffers();
}

static void printMenu(void) {
    pspDebugScreenSetXY(13,5);
    pspDebugScreenSetTextColor(0xffffffff);
    pspDebugScreenPrintf("THE CAT IS LOCKED");
    pspDebugScreenSetXY(17,7);
    pspDebugScreenPrintf("BETA 0.1.1");

    if(menu==0) {
        pspDebugScreenSetXY(20,14); pspDebugScreenPrintf("%s JOUER",selected==0?">":" ");
        pspDebugScreenSetXY(20,19); pspDebugScreenPrintf("%s PARAMETRES",selected==1?">":" ");
        pspDebugScreenSetXY(20,24); pspDebugScreenPrintf("%s QUITTER",selected==2?">":" ");
        pspDebugScreenSetXY(12,31); pspDebugScreenPrintf("CROIX : VALIDER     HAUT/BAS : CHOISIR");
    } else if(menu==1) {
        pspDebugScreenSetXY(20,14); pspDebugScreenPrintf("%s REPRISE",selected==0?">":" ");
        pspDebugScreenSetXY(20,19); pspDebugScreenPrintf("%s COMMENCER L'AVENTURE",selected==1?">":" ");
        pspDebugScreenSetXY(16,31); pspDebugScreenPrintf("CROIX : VALIDER       ROND : RETOUR");
    } else if(menu==2) {
        pspDebugScreenSetXY(17,14); pspDebugScreenPrintf("%s VIBREUR : INDISPONIBLE",selected==0?">":" ");
        pspDebugScreenSetXY(17,19); pspDebugScreenPrintf("%s SONS : %s",selected==1?">":" ",soundOn?"ON":"OFF");
        pspDebugScreenSetXY(17,24); pspDebugScreenPrintf("%s REINITIALISER LES SAUVEGARDES",selected==2?">":" ");
        pspDebugScreenSetXY(16,31); pspDebugScreenPrintf("GAUCHE/DROITE : MODIFIER   ROND : RETOUR");
    } else if(menu==3) {
        pspDebugScreenSetXY(15,14); pspDebugScreenPrintf("EFFACER TOUTE LA PROGRESSION ?");
        pspDebugScreenSetXY(19,19); pspDebugScreenPrintf("%s NON",selected==0?">":" ");
        pspDebugScreenSetXY(19,24); pspDebugScreenPrintf("%s OUI, TOUT EFFACER",selected==1?">":" ");
    } else {
        pspDebugScreenSetXY(1,1); pspDebugScreenPrintf("NIVEAU 1 - LA PREMIERE PIECE");
        pspDebugScreenSetXY(1,2); pspDebugScreenPrintf("Fleches : bouger   Croix : chercher   Rond : menu");
        pspDebugScreenSetXY(1,3); pspDebugScreenPrintf("CLES : %d/2  |  PORTE : %s",keysCollected,keyFound?"OUVERTE":"VERROUILLEE");
        pspDebugScreenSetXY(1,4); pspDebugScreenPrintf("VIE : %d%%",health);
    }
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

    while(1) {
        SceCtrlData pad;
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
                    if((dx*dx+dz*dz)<0.36f && !keyPicked[i]) { keyPicked[i]=1; keysCollected++; }
                }
                for(i=0;i<4;i++) {
                    float dx=catX-trapPos[i][0], dz=catZ-trapPos[i][1];
                    if(!trapTriggered[i] && (dx*dx+dz*dz)<0.30f) {
                        trapTriggered[i]=1;
                        health-=25;
                        if(health<0) health=0;
                    }
                }
                keyFound=(keysCollected>=2);
            }
            if(press & PSP_CTRL_CIRCLE) { menu=0; selected=0; }
            if(catX < -4.0f) catX=-4.0f;
            if(catX > 4.0f) catX=4.0f;
            if(catZ < -4.2f) catZ=-4.2f;
            if(catZ > 3.5f) catZ=3.5f;
        } else {
            int max=(menu==0)?2:((menu==1)?1:((menu==2)?2:1));
            if(press & PSP_CTRL_UP) selected--;
            if(press & PSP_CTRL_DOWN) selected++;
            if(selected<0) selected=max;
            if(selected>max) selected=0;

            if(menu==2 && (press & (PSP_CTRL_LEFT|PSP_CTRL_RIGHT)) && selected==1)
                soundOn=!soundOn;

            if(press & PSP_CTRL_CIRCLE) {
                if(menu==0) { /* déjà à l'accueil */ }
                else if(menu==3) { menu=2; selected=0; }
                else { menu=0; selected=0; }
            }
            if(press & PSP_CTRL_CROSS) {
                if(menu==0) {
                    if(selected==0) { menu=1; selected=0; }
                    else if(selected==1) { menu=2; selected=0; }
                    else sceKernelExitGame();
                } else if(menu==1) {
                    if(selected==0) { if(hasSave) menu=4; }
                    else { catX=0; catZ=0; keyFound=0; keysCollected=0; keyPicked[0]=0; keyPicked[1]=0; health=100; for(int i=0;i<4;i++) trapTriggered[i]=0; hasSave=1; menu=4; }
                } else if(menu==2) {
                    if(selected==2) { menu=3; selected=0; }
                } else if(menu==3) {
                    if(selected==1) { hasSave=0; keyFound=0; keysCollected=0; keyPicked[0]=0; keyPicked[1]=0; health=100; for(int i=0;i<4;i++) trapTriggered[i]=0; catX=0; catZ=0; }
                    menu=2; selected=0;
                }
            }
        }

        drawFrame();
    }
    sceKernelExitGame();
    return 0;
}
