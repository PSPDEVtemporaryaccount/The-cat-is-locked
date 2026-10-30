#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

PSP_MODULE_INFO("The Cat Is Locked", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(2048);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272
#define PI 3.14159265f

typedef struct { unsigned int color; float x, y, z; } Vertex;
static unsigned int __attribute__((aligned(16))) list[262144];
static int menu = 0; /* 0 main, 1 play, 2 settings, 3 reset confirm, 4 game */
static int selected = 0;
static int hasSave = 0;
static int vibration = 0;
static int soundOn = 1;
static int volume = 7;
static int resetConfirm = 0;
static float catX = 0.0f, catZ = 0.0f;
static int keyFound = 0;

static Vertex cube[] = {
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
    sceGumTranslate(&p); sceGumScale(&s);
    sceGumDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,
                    36,0,cube);
    sceGumPopMatrix();
}
static void begin3D(void) {
    sceGuStart(GU_DIRECT,list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,(void*)0x88000,BUF_WIDTH);
    sceGuOffset(2048-(SCR_WIDTH/2),2048-(SCR_HEIGHT/2));
    sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST); sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL); sceGuFrontFace(GU_CW); sceGuShadeModel(GU_SMOOTH);
    sceGuClearColor(0xffb87942); sceGuClearDepth(0); sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    sceGumPerspective(48.0f,16.0f/9.0f,0.5f,100.0f);
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    ScePspFVector3 eye={0.0f,5.5f,10.0f}, target={0.0f,0.5f,0.0f}, up={0.0f,1.0f,0.0f};
    sceGumLookAt(&eye,&target,&up);
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
}
static void drawRoom(void) {
    drawCube(0,-.35f,0,10,.35f,10,0xffeeeeee);
    drawCube(0,1.7f,-5,10,4, .25f,0xffa64f30);
    drawCube(-5,1.7f,0,.25f,4,10,0xffbd633d);
    drawCube(5,1.7f,0,.25f,4,10,0xffbd633d);
    drawCube(0,3.7f,0,10,.2f,10,0xffd98a55);
    /* door at the far wall, with a dark lock plate */
    drawCube(0,1.05f,-4.82f,1.5f,2.2f,.08f,0xff493023);
    drawCube(0.48f,1.05f,-4.72f,.10f,.16f,.08f,0xffffd36a);
    /* windows */
    drawCube(-2.7f,2.3f,-4.78f,1.6f,1.0f,.06f,0xff7ac9e8);
    drawCube(2.7f,2.3f,-4.78f,1.6f,1.0f,.06f,0xff7ac9e8);
}
static void drawCat(void) {
    /* Stylised low-poly 3D cat made from connected volumes for the first prototype. */
    drawCube(catX,.25f,catZ,.85f,.55f,.48f,0xff38312d); /* body */
    drawCube(catX,.63f,catZ-.34f,.48f,.43f,.43f,0xff4b423c); /* head */
    drawCube(catX-.15f,.95f,catZ-.47f,.13f,.27f,.12f,0xff4b423c); /* ear */
    drawCube(catX+.15f,.95f,catZ-.47f,.13f,.27f,.12f,0xff4b423c);
    drawCube(catX-.23f,.12f,catZ-.12f,.14f,.38f,.14f,0xff272321);
    drawCube(catX+.23f,.12f,catZ-.12f,.14f,.38f,.14f,0xff272321);
    drawCube(catX,.32f,catZ+.48f,.12f,.12f,.55f,0xff38312d); /* tail */
}
static void drawGame(void) {
    begin3D(); drawRoom(); drawCat();
    sceGuFinish(); sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}
static void printMenu(void) {
    pspDebugScreenSetXY(1,1); pspDebugScreenSetTextColor(0xffffffff);
    pspDebugScreenPrintf("THE CAT IS LOCKED  |  BETA 0.1.0\n\n");
    if(menu==0) {
        pspDebugScreenPrintf("          [ JOUER ]\n\n");
        pspDebugScreenPrintf("          [ PARAMETRES ]\n\n");
        pspDebugScreenPrintf("          [ QUITTER ]\n\n");
        pspDebugScreenPrintf("\n  Haut/Bas : choisir   X : valider");
    } else if(menu==1) {
        pspDebugScreenPrintf("JOUER\n\n");
        pspDebugScreenPrintf("%s Reprise\n\n",selected==0?">":" ");
        pspDebugScreenPrintf("%s Commencer l'aventure\n\n",selected==1?">":" ");
        pspDebugScreenPrintf("\n  O : retour");
    } else if(menu==2) {
        pspDebugScreenPrintf("PARAMETRES\n\n");
        pspDebugScreenPrintf("%s Vibreur : %s%s\n\n",selected==0?">":" ",vibration?"ON":"OFF",
                             (sceKernelDevkitVersion() && 0)?" (indisponible)":"");
        pspDebugScreenPrintf("%s Sons : %s\n",selected==1?">":" ",soundOn?"ON":"OFF");
        pspDebugScreenPrintf("  Volume : [");
        for(int i=0;i<10;i++) pspDebugScreenPrintf("%c",i<volume?'#':'-');
        pspDebugScreenPrintf("]\n\n");
        pspDebugScreenPrintf("%s Reinitialiser toutes les sauvegardes\n\n",selected==2?">":" ");
        pspDebugScreenPrintf("\n  Gauche/Droite : modifier   O : retour");
    } else if(menu==3) {
        pspDebugScreenPrintf("REINITIALISER LA PROGRESSION ?\n\n");
        pspDebugScreenPrintf("%s Non, revenir aux parametres\n",selected==0?">":" ");
        pspDebugScreenPrintf("%s Oui, tout effacer\n",selected==1?">":" ");
    } else {
        pspDebugScreenPrintf("NIVEAU 1 - LA PREMIERE PIECE\n");
        pspDebugScreenPrintf("X : chercher la cle   O : menu\n");
        pspDebugScreenPrintf("Cle : %s\n",keyFound?"trouvee":"introuvable");
        pspDebugScreenPrintf("Deplacement : fleches directionnelles\n");
    }
}
int main(void) {
    sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    pspDebugScreenInit();
    sceGuInit();
    sceGuStart(GU_DIRECT,list); sceGuFinish(); sceGuSync(0,0);
    sceDisplayWaitVblankStart(); sceGuDisplay(GU_TRUE);
    while(1) {
        SceCtrlData pad; sceCtrlReadBufferPositive(&pad,1);
        unsigned int b=pad.Buttons;
        static unsigned int old=0; unsigned int press=b & ~old; old=b;
        if(menu==4) {
            if(press & PSP_CTRL_LEFT) catX-=.15f;
            if(press & PSP_CTRL_RIGHT) catX+=.15f;
            if(press & PSP_CTRL_UP) catZ-=.15f;
            if(press & PSP_CTRL_DOWN) catZ+=.15f;
            if(press & PSP_CTRL_CROSS) keyFound=1;
            if(press & PSP_CTRL_CIRCLE) { menu=0; selected=0; }
            if(catX < -4.0f) catX=-4.0f; if(catX>4.0f) catX=4.0f;
            if(catZ < -3.5f) catZ=-3.5f; if(catZ>3.5f) catZ=3.5f;
        } else {
            if(press & PSP_CTRL_UP) selected--;
            if(press & PSP_CTRL_DOWN) selected++;
            if(menu==0) { if(selected<0)selected=2; if(selected>2)selected=0; }
            if(menu==1) { if(selected<0)selected=1; if(selected>1)selected=0; }
            if(menu==2) { if(selected<0)selected=3; if(selected>3)selected=0; }
            if(menu==3) { if(selected<0)selected=1; if(selected>1)selected=0; }
            if(menu==2 && (press & (PSP_CTRL_LEFT|PSP_CTRL_RIGHT))) {
                if(selected==0) vibration=!vibration;
                if(selected==1) soundOn=!soundOn;
                if(selected==2) { if(press&PSP_CTRL_LEFT) volume--; else volume++; if(volume<0)volume=0;if(volume>10)volume=10; }
            }
            if(press & PSP_CTRL_CIRCLE) {
                if(menu==0) { /* no-op */ } else if(menu==3) menu=2; else { menu=0; selected=0; }
            }
            if(press & PSP_CTRL_CROSS) {
                if(menu==0) { if(selected==0){menu=1;selected=0;} else if(selected==1){menu=2;selected=0;} else sceKernelExitGame(); }
                else if(menu==1) { if(selected==0){ if(hasSave){menu=4;} } else {catX=0;catZ=0;keyFound=0;hasSave=1;menu=4;} }
                else if(menu==2) { if(selected==3){menu=3;selected=0;} }
                else if(menu==3) { if(selected==1){hasSave=0;keyFound=0;catX=0;catZ=0;} menu=2;selected=0; }
            }
        }
        drawGame();
        printMenu();
        sceDisplayWaitVblankStart();
    }
    sceKernelExitGame(); return 0;
}
