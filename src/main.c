#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <pspiofilemgr.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psputility.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("The Cat Is Locked", 0, 7, 6);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(2048);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512
#define SAVE_DIR "ms0:/PSP/SAVEDATA/TCLCK070"
#define SAVE_PATH SAVE_DIR "/SAVE.DAT"
#define SETTINGS_PATH SAVE_DIR "/CONFIG.DAT"
#define SETTINGS_MAGIC 0x54434C53
#define SAVE_MAGIC 0x54434C37
#define SAVE_VERSION 3
#define SAVE_COUNT 5
#define WORLD_COUNT 6
#define LEVELS_PER_WORLD 10

typedef struct { unsigned int color; float x,y,z; } Vertex3D;
typedef struct { float u,v; unsigned int color; float x,y,z; } VertexTex2D;
typedef struct { int valid, world, level, lives, difficulty, hasKey, health; float catX, catZ, keyX, keyZ; char pseudo[24]; } SaveSlot;
typedef struct { unsigned int magic, version; SaveSlot slots[SAVE_COUNT]; } SaveFile;
typedef struct { int valid, world, level, lives, difficulty, hasKey; float catX, catZ, keyX, keyZ; } SaveSlotV2;
typedef struct { unsigned int magic, version; SaveSlotV2 slots[SAVE_COUNT]; } SaveFileV2;
typedef struct { int valid, world, level, lives, difficulty; } SaveSlotV1;
typedef struct { unsigned int magic, version; SaveSlotV1 slots[SAVE_COUNT]; } SaveFileV1;

extern unsigned char tex_main_default[];
extern unsigned char tex_main_play[];
extern unsigned char tex_main_settings[];
extern unsigned char tex_main_quit[];
extern unsigned char tex_play_menu[];
extern unsigned char tex_play_resume[];
extern unsigned char tex_start_adventure[];
extern unsigned char tex_settings[];
extern unsigned char tex_saves[];
extern short menu_audio_pcm[];
extern short menu_audio_pcm_end[];

static unsigned int __attribute__((aligned(16))) displayList[262144];
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

static int screen=0; /* accueil, jouer, paramètres, sauvegardes, jeu, pause, difficulté, défaite, victoire */
static int selected=0, difficulty=1, lives=3, world=1, level=1, hasKey=0, currentSlot=-1;
static int soundOn=1, volume=70, noticeFrames=0, health=100;
static char playerPseudo[24]="Chat";
static float catX=0.0f, catZ=2.65f, keyX=0.0f, keyZ=-1.25f;
static SaveSlot saves[SAVE_COUNT];
static char notice[64];

/* PSP background music: decoded mono PCM, streamed through the audio channel. */
#define MENU_AUDIO_RATE 44100
#define MENU_AUDIO_CHUNK 2048
static volatile int menuAudioDesiredGain=0;
static int menuAudioThreadId=-1;
static short menuAudioBuffer[MENU_AUDIO_CHUNK] __attribute__((aligned(64)));

static int menuAudioThread(SceSize args, void *argp) {
    const int totalSamples=(int)(menu_audio_pcm_end-menu_audio_pcm);
    int position=0;
    int currentGain=0, rampStart=0, rampTarget=0, rampRemaining=0;
    (void)args; (void)argp;
    {
        int channel=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,MENU_AUDIO_CHUNK,PSP_AUDIO_FORMAT_MONO);
        if(channel<0) return 0;
        while(1) {
            int wanted=menuAudioDesiredGain;
            int i;
            if(wanted<0) wanted=0;
            if(wanted>100) wanted=100;
            if(wanted!=rampTarget) {
                rampStart=currentGain;
                rampTarget=wanted;
                rampRemaining=MENU_AUDIO_RATE;
            }
            for(i=0;i<MENU_AUDIO_CHUNK;i++) {
                int sampleGain, sample;
                if(position>=totalSamples) position=0;
                sample=(int)menu_audio_pcm[position++];
                if(rampRemaining>0) {
                    int elapsed=MENU_AUDIO_RATE-rampRemaining;
                    sampleGain=rampStart+((rampTarget-rampStart)*elapsed)/MENU_AUDIO_RATE;
                    rampRemaining--;
                    currentGain=sampleGain;
                } else {
                    currentGain=rampTarget;
                    sampleGain=currentGain;
                }
                menuAudioBuffer[i]=(short)((sample*sampleGain)/100);
            }
            sceAudioOutputBlocking(channel,PSP_AUDIO_VOLUME_MAX,menuAudioBuffer);
        }
    }
    return 0;
}

static void startMenuAudio(void) {
    menuAudioThreadId=sceKernelCreateThread("menu_audio",menuAudioThread,0x18,0x10000,THREAD_ATTR_USER,NULL);
    if(menuAudioThreadId>=0) sceKernelStartThread(menuAudioThreadId,0,NULL);
}

static void drawCube(float x,float y,float z,float sx,float sy,float sz,unsigned int color) {
    int i;
    ScePspFVector3 p={x,y,z}, s={sx,sy,sz};
    for(i=0;i<36;i++) cube[i].color=color;
    sceKernelDcacheWritebackRange(cube,sizeof(cube));
    sceGumPushMatrix();
    sceGumTranslate(&p);
    sceGumScale(&s);
    sceGumDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,36,0,cube);
    sceGumPopMatrix();
}

static void drawTexture(const unsigned char *pixels) {
    VertexTex2D v[2] __attribute__((aligned(16)));
    v[0].u=0.0f; v[0].v=0.0f; v[0].color=0xffffffff;
    v[0].x=0.0f; v[0].y=0.0f; v[0].z=0.0f;
    v[1].u=480.0f; v[1].v=272.0f; v[1].color=0xffffffff;
    v[1].x=480.0f; v[1].y=272.0f; v[1].z=0.0f;
    sceKernelDcacheWritebackRange(v,sizeof(v));
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888,0,0,0);
    sceGuTexImage(0,512,512,512,pixels);
    sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR,GU_LINEAR);
    sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
    sceGuDisable(GU_TEXTURE_2D);
}

static void begin2D(void) {
    sceGuDisable(GU_DEPTH_TEST);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
}

static void drawRect2D(float x1,float y1,float x2,float y2,unsigned int color) {
    Vertex3D v[2] __attribute__((aligned(16)));
    v[0].color=color; v[0].x=x1; v[0].y=y1; v[0].z=0.0f;
    v[1].color=color; v[1].x=x2; v[1].y=y2; v[1].z=0.0f;
    sceKernelDcacheWritebackRange(v,sizeof(v));
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
}

static void drawDigit7Seg(float x,float y,int digit,unsigned int color) {
    static const unsigned char mask[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    static const unsigned char bits[7]={1,2,4,8,16,32,64};
    int i;
    const float w=5.0f,h=2.0f;
    const float seg[7][4]={{0,0,8,2},{8,0,10,7},{8,7,10,14},{0,14,8,16},{-2,7,0,14},{-2,0,0,7},{0,7,8,9}};
    if(digit<0 || digit>9) return;
    for(i=0;i<7;i++) if(mask[digit]&bits[i])
        drawRect2D(x+seg[i][0],y+seg[i][1],x+seg[i][2],y+seg[i][3],color);
    (void)w; (void)h;
}

static void drawHealthBar(void) {
    float width=(health<0?0.0f:(health>100?100.0f:(float)health))*2.05f;
    unsigned int fill=(health>60?0xff79d653:(health>30?0xffffb52e:0xffe5484d));
    int shownLives=(lives<0?9:(lives>9?9:lives));
    begin2D();
    /* HUD follows the supplied Gameplay.png layout: lives, health, key. */
    drawRect2D(5.0f,5.0f,39.0f,37.0f,0xfff5f5f5);
    drawRect2D(8.0f,7.0f,22.0f,21.0f,0xfff04442);
    drawRect2D(21.0f,7.0f,35.0f,21.0f,0xfff04442);
    drawRect2D(11.0f,17.0f,32.0f,28.0f,0xfff04442);
    drawRect2D(16.0f,26.0f,27.0f,34.0f,0xfff04442);
    drawDigit7Seg(18.0f,10.0f,shownLives,0xffffffff);

    drawRect2D(51.0f,7.0f,261.0f,30.0f,0xfff5f5f5);
    drawRect2D(54.0f,10.0f,258.0f,27.0f,0xff343434);
    if(width>0.0f) drawRect2D(54.0f,10.0f,54.0f+width,27.0f,fill);

    drawRect2D(360.0f,5.0f,473.0f,46.0f,0xff626262);
    /* Gold key: round head, shaft and two teeth. */
    drawRect2D(374.0f,15.0f,391.0f,32.0f,0xffffd43b);
    drawRect2D(378.0f,19.0f,387.0f,28.0f,0xff8d6b12);
    drawRect2D(388.0f,25.0f,426.0f,31.0f,0xffffd43b);
    drawRect2D(416.0f,30.0f,422.0f,38.0f,0xffffd43b);
    drawDigit7Seg(436.0f,14.0f,hasKey?1:0,0xffffffff);
    drawRect2D(450.0f,14.0f,453.0f,30.0f,0xffffffff);
}

static void begin3D(void) {
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    sceGumPerspective(52.0f,16.0f/9.0f,0.5f,80.0f);
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    {
        ScePspFVector3 eye={0.0f,7.0f,10.5f};
        ScePspFVector3 target={0.0f,0.45f,0.0f};
        ScePspFVector3 up={0.0f,1.0f,0.0f};
        sceGumLookAt(&eye,&target,&up);
    }
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
}

static void drawCat(void) {
    float x=catX, z=catZ;
    /* A high-contrast, unmistakable feline silhouette: torso, head, ears, paws, tail, face. */
    drawCube(x,0.43f,z,1.00f,0.52f,0.68f,0xffb9b1a7);
    drawCube(x,0.82f,z-0.42f,0.66f,0.62f,0.54f,0xffd9d0c5);
    drawCube(x-0.24f,1.17f,z-0.58f,0.20f,0.36f,0.18f,0xff74645a);
    drawCube(x+0.24f,1.17f,z-0.58f,0.20f,0.36f,0.18f,0xff74645a);
    drawCube(x-0.15f,0.86f,z-0.70f,0.11f,0.12f,0.05f,0xffcaff62);
    drawCube(x+0.15f,0.86f,z-0.70f,0.11f,0.12f,0.05f,0xffcaff62);
    drawCube(x-0.15f,0.86f,z-0.74f,0.035f,0.08f,0.025f,0xff171717);
    drawCube(x+0.15f,0.86f,z-0.74f,0.035f,0.08f,0.025f,0xff171717);
    drawCube(x,0.69f,z-0.75f,0.10f,0.07f,0.04f,0xffff8fa3);
    drawCube(x-0.34f,0.68f,z-0.72f,0.30f,0.025f,0.025f,0xfff8f5f0);
    drawCube(x+0.34f,0.68f,z-0.72f,0.30f,0.025f,0.025f,0xfff8f5f0);
    drawCube(x-0.30f,0.20f,z+0.18f,0.18f,0.34f,0.20f,0xff74645a);
    drawCube(x+0.30f,0.20f,z+0.18f,0.18f,0.34f,0.20f,0xff74645a);
    drawCube(x-0.30f,0.20f,z-0.30f,0.18f,0.34f,0.20f,0xff74645a);
    drawCube(x+0.30f,0.20f,z-0.30f,0.18f,0.34f,0.20f,0xff74645a);
    drawCube(x,0.46f,z+0.62f,0.14f,0.14f,0.76f,0xffd9d0c5);
}

static void drawKey(void) {
    if(hasKey) return;
    drawCube(keyX,0.28f,keyZ,0.62f,0.10f,0.12f,0xffffd34e);
    drawCube(keyX-0.30f,0.28f,keyZ,0.25f,0.25f,0.13f,0xffffd34e);
    drawCube(keyX+0.22f,0.22f,keyZ,0.10f,0.18f,0.14f,0xffffd34e);
    drawCube(keyX+0.34f,0.18f,keyZ,0.09f,0.12f,0.14f,0xffffd34e);
}

static void drawRoom(void) {
    unsigned int wall=0xff747b86, trim=0xffa7adb4;
    drawCube(0.0f,-0.25f,0.0f,8.0f,0.5f,8.0f,0xff88745e);
    drawCube(-4.0f,1.35f,0.0f,0.28f,3.2f,8.0f,wall);
    drawCube(4.0f,1.35f,0.0f,0.28f,3.2f,8.0f,wall);
    drawCube(-2.55f,1.35f,-4.0f,2.9f,3.2f,0.28f,wall);
    drawCube(2.55f,1.35f,-4.0f,2.9f,3.2f,0.28f,wall);
    drawCube(0.0f,2.85f,-4.0f,2.2f,0.22f,0.28f,trim);
    drawCube(0.0f,1.10f,-3.78f,1.45f,2.15f,0.16f,hasKey?0xff7b9b70:0xff70452f);
    drawCube(0.48f,1.05f,-3.66f,0.10f,0.10f,0.10f,0xffffd34e);
    drawCube(-3.82f,1.45f,-1.8f,0.04f,1.55f,1.15f,trim);
    drawCube(3.82f,1.45f,1.7f,0.04f,1.55f,1.15f,trim);
}

static void drawGameplay(void) {
    int pattern=level%3;
    begin3D();
    drawRoom();
    if(pattern==0) {
        drawCube(-1.45f,0.45f,0.0f,0.85f,0.9f,0.85f,0xff80634c);
        drawCube(1.45f,0.45f,-1.0f,0.85f,0.9f,0.85f,0xff80634c);
    } else if(pattern==1) {
        drawCube(0.0f,0.38f,0.35f,1.9f,0.75f,0.65f,0xff80634c);
        drawCube(-2.2f,0.35f,-1.3f,0.55f,0.7f,0.8f,0xff80634c);
    } else {
        drawCube(-1.8f,0.4f,-1.1f,0.7f,0.8f,1.2f,0xff80634c);
        drawCube(1.8f,0.4f,0.2f,0.7f,0.8f,1.2f,0xff80634c);
        drawCube(0.0f,0.35f,1.2f,1.2f,0.7f,0.55f,0xff80634c);
    }
    if(pattern==0) drawCube(0.0f,0.05f,-2.45f,0.95f,0.10f,0.95f,0xffbd4540);
    else if(pattern==1) drawCube(2.55f,0.05f,-2.15f,0.95f,0.10f,0.95f,0xffbd4540);
    else drawCube(-2.65f,0.05f,1.95f,0.95f,0.10f,0.95f,0xffbd4540);
    drawKey();
    drawCat();
}

static void resetPosition(void) {
    catX=0.0f;
    catZ=2.65f;
}

static int firstFreeSave(void) {
    int i;
    for(i=0;i<SAVE_COUNT;i++) if(!saves[i].valid) return i;
    return -1;
}

static void makeSaveDir(void) {
    sceIoMkdir("ms0:/PSP",0777);
    sceIoMkdir("ms0:/PSP/SAVEDATA",0777);
    sceIoMkdir(SAVE_DIR,0777);
}

static void writeSaves(void);

static void writeSaves(void);
static void loadSettings(void);
static void writeSettings(void);

static void loadSaves(void) {
    struct { unsigned int magic, version; } header;
    SaveFile f;
    SaveFileV2 old2;
    SaveFileV1 old;
    SceUID fd;
    int i;
    memset(saves,0,sizeof(saves));
    fd=sceIoOpen(SAVE_PATH,PSP_O_RDONLY,0777);
    if(fd<0) return;
    memset(&header,0,sizeof(header));
    if(sceIoRead(fd,&header,sizeof(header))!=(int)sizeof(header) || header.magic!=SAVE_MAGIC) {
        sceIoClose(fd);
        return;
    }
    if(header.version==SAVE_VERSION) {
        sceIoLseek(fd,0,PSP_SEEK_SET);
        memset(&f,0,sizeof(f));
        if(sceIoRead(fd,&f,sizeof(f))==(int)sizeof(f))
            memcpy(saves,f.slots,sizeof(saves));
    } else if(header.version==2) {
        sceIoLseek(fd,0,PSP_SEEK_SET);
        memset(&old2,0,sizeof(old2));
        if(sceIoRead(fd,&old2,sizeof(old2))==(int)sizeof(old2) && old2.magic==SAVE_MAGIC) {
            for(i=0;i<SAVE_COUNT;i++) {
                saves[i].valid=old2.slots[i].valid; saves[i].world=old2.slots[i].world;
                saves[i].level=old2.slots[i].level; saves[i].lives=old2.slots[i].lives;
                saves[i].difficulty=old2.slots[i].difficulty; saves[i].hasKey=old2.slots[i].hasKey;
                saves[i].catX=old2.slots[i].catX; saves[i].catZ=old2.slots[i].catZ;
                saves[i].keyX=old2.slots[i].keyX; saves[i].keyZ=old2.slots[i].keyZ;
                saves[i].health=100; strncpy(saves[i].pseudo,"Chat",sizeof(saves[i].pseudo)-1);
            }
        }
    } else if(header.version==1) {
        sceIoLseek(fd,0,PSP_SEEK_SET);
        memset(&old,0,sizeof(old));
        if(sceIoRead(fd,&old,sizeof(old))==(int)sizeof(old) && old.magic==SAVE_MAGIC) {
            for(i=0;i<SAVE_COUNT;i++) {
                saves[i].valid=old.slots[i].valid;
                saves[i].world=old.slots[i].world;
                saves[i].level=old.slots[i].level;
                saves[i].lives=old.slots[i].lives;
                saves[i].difficulty=old.slots[i].difficulty;
                saves[i].hasKey=0;
                saves[i].catX=0.0f; saves[i].catZ=2.65f;
                saves[i].keyX=(float)((saves[i].level%3)-1)*1.65f;
                saves[i].keyZ=-1.25f;
                saves[i].health=100; strncpy(saves[i].pseudo,"Chat",sizeof(saves[i].pseudo)-1);
            }
        }
    }
    sceIoClose(fd);
    if(header.version==1 || header.version==2) writeSaves();
}

static void writeSaves(void) {
    SaveFile f;
    SceUID fd;
    makeSaveDir();
    memset(&f,0,sizeof(f));
    f.magic=SAVE_MAGIC; f.version=SAVE_VERSION;
    memcpy(f.slots,saves,sizeof(saves));
    fd=sceIoOpen(SAVE_PATH,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);
    if(fd>=0) { sceIoWrite(fd,&f,sizeof(f)); sceIoClose(fd); }
}

static void writeSettings(void) {
    struct { unsigned int magic; int soundOn, volume; } cfg;
    SceUID fd;
    makeSaveDir();
    cfg.magic=SETTINGS_MAGIC; cfg.soundOn=soundOn; cfg.volume=volume;
    fd=sceIoOpen(SETTINGS_PATH,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);
    if(fd>=0) { sceIoWrite(fd,&cfg,sizeof(cfg)); sceIoClose(fd); }
}

static void loadSettings(void) {
    struct { unsigned int magic; int soundOn, volume; } cfg;
    SceUID fd=sceIoOpen(SETTINGS_PATH,PSP_O_RDONLY,0777);
    if(fd<0) return;
    memset(&cfg,0,sizeof(cfg));
    if(sceIoRead(fd,&cfg,sizeof(cfg))==(int)sizeof(cfg) && cfg.magic==SETTINGS_MAGIC) {
        soundOn=cfg.soundOn?1:0;
        volume=cfg.volume;
        if(volume<0) volume=0;
        if(volume>100) volume=100;
    }
    sceIoClose(fd);
}

static void saveProgress(void) {
    if(currentSlot<0 || currentSlot>=SAVE_COUNT) return;
    saves[currentSlot].valid=1;
    saves[currentSlot].world=world;
    saves[currentSlot].level=level;
    saves[currentSlot].lives=lives;
    saves[currentSlot].difficulty=difficulty;
    saves[currentSlot].hasKey=hasKey;
    saves[currentSlot].health=health;
    memcpy(saves[currentSlot].pseudo,playerPseudo,sizeof(saves[currentSlot].pseudo));
    saves[currentSlot].pseudo[sizeof(saves[currentSlot].pseudo)-1]=0;
    saves[currentSlot].catX=catX; saves[currentSlot].catZ=catZ;
    saves[currentSlot].keyX=keyX; saves[currentSlot].keyZ=keyZ;
    writeSaves();
}

static void startGame(void) {
    int slot=firstFreeSave();
    if(slot<0) {
        strncpy(notice,"Les 5 emplacements sont occupes.",sizeof(notice)-1);
        noticeFrames=180;
        return;
    }
    currentSlot=slot;
    world=1; level=1;
    difficulty=selected;
    lives=(difficulty==0)?5:((difficulty==1)?3:((difficulty==2)?2:((difficulty==3 || difficulty==4)?1:-1)));
    hasKey=0; health=100;
    resetPosition();
    keyX=(float)((level%3)-1)*1.65f;
    keyZ=-1.25f;
    saveProgress();
    screen=4;
}

static void loadSave(int index) {
    if(index<0 || index>=SAVE_COUNT || !saves[index].valid) {
        strncpy(notice,"Cet emplacement est vide.",sizeof(notice)-1);
        noticeFrames=120;
        return;
    }
    currentSlot=index;
    world=saves[index].world;
    level=saves[index].level;
    lives=saves[index].lives;
    difficulty=saves[index].difficulty;
    hasKey=saves[index].hasKey;
    health=saves[index].health;
    strncpy(playerPseudo,saves[index].pseudo,sizeof(playerPseudo)-1);
    catX=saves[index].catX; catZ=saves[index].catZ;
    keyX=saves[index].keyX; keyZ=saves[index].keyZ;
    screen=4;
}

static void loseLife(void) {
    if(lives>0) lives--;
    resetPosition();
    if(lives==0) {
        if(currentSlot>=0) {
            saves[currentSlot].valid=0;
            writeSaves();
        }
        currentSlot=-1;
        screen=7;
        selected=0;
    } else {
        saveProgress();
        strncpy(notice,"Aie ! Retour au debut de la piece.",sizeof(notice)-1);
        noticeFrames=100;
    }
}

static void nextLevel(void) {
    level++;
    if(level>LEVELS_PER_WORLD) { level=1; world++; }
    hasKey=0; health=100;
    resetPosition();
    keyX=(float)((level%3)-1)*1.65f;
    keyZ=-1.25f-(float)((level%2)*0.35f);
    if(world>WORLD_COUNT) { screen=8; selected=0; return; }
    saveProgress();
    strncpy(notice,"Porte ouverte ! Niveau suivant.",sizeof(notice)-1);
    noticeFrames=120;
}

static int blocked(float x,float z) {
    int p=level%3;
    if(x<-3.45f || x>3.45f || z<-3.45f || z>3.45f) return 1;
    if(p==0) {
        if(fabsf(x+1.45f)<0.72f && fabsf(z)<0.72f) return 1;
        if(fabsf(x-1.45f)<0.72f && fabsf(z+1.0f)<0.72f) return 1;
    } else if(p==1) {
        if(fabsf(x)<1.15f && fabsf(z-0.35f)<0.55f) return 1;
        if(fabsf(x+2.2f)<0.42f && fabsf(z+1.3f)<0.52f) return 1;
    } else {
        if(fabsf(x+1.8f)<0.55f && fabsf(z+1.1f)<0.72f) return 1;
        if(fabsf(x-1.8f)<0.55f && fabsf(z-0.2f)<0.72f) return 1;
        if(fabsf(x)<0.75f && fabsf(z-1.2f)<0.45f) return 1;
    }
    return 0;
}

static void updateGameplay(const SceCtrlData *pad,unsigned int press) {
    float dx=0.0f,dz=0.0f;
    if(pad->Lx<105) dx-=0.075f;
    if(pad->Lx>150) dx+=0.075f;
    if(pad->Ly<105) dz-=0.075f;
    if(pad->Ly>150) dz+=0.075f;
    if(pad->Buttons & PSP_CTRL_LEFT) dx-=0.075f;
    if(pad->Buttons & PSP_CTRL_RIGHT) dx+=0.075f;
    if(pad->Buttons & PSP_CTRL_UP) dz-=0.075f;
    if(pad->Buttons & PSP_CTRL_DOWN) dz+=0.075f;
    if(dx!=0.0f && dz!=0.0f) { dx*=0.7071f; dz*=0.7071f; }
    if(!blocked(catX+dx,catZ)) catX+=dx;
    if(!blocked(catX,catZ+dz)) catZ+=dz;
    {
        int p=level%3;
        int onTrap=(p==0 && fabsf(catX)<0.58f && fabsf(catZ+2.45f)<0.58f) ||
                   (p==1 && fabsf(catX-2.55f)<0.58f && fabsf(catZ+2.15f)<0.58f) ||
                   (p==2 && fabsf(catX+2.65f)<0.58f && fabsf(catZ-1.95f)<0.58f);
        if(onTrap) {
            hasKey=0;
            health-=35;
            resetPosition();
            if(health<=0) {
                health=100;
                loseLife();
                if(screen!=4) return;
            } else saveProgress();
            strncpy(notice,"Aie ! Le chat est blesse.",sizeof(notice)-1);
            noticeFrames=100;
        }
    }
    if(!hasKey && fabsf(catX-keyX)<0.52f && fabsf(catZ-keyZ)<0.52f) {
        hasKey=1;
        saveProgress();
        strncpy(notice,"Cle trouvee ! Va vers la porte.",sizeof(notice)-1);
        noticeFrames=150;
    }
    if(hasKey && catZ<-3.0f && fabsf(catX)<0.85f) nextLevel();
    if(press & PSP_CTRL_START) { screen=5; selected=0; }
}

static void openPseudoKeyboard(void) {
    SceUtilityOskParams params;
    SceUtilityOskData data;
    unsigned short initial[24], output[24], description[64];
    const char *label="Entrez votre pseudo";
    int i, status, shuttingDown=0;
    memset(&params,0,sizeof(params));
    memset(&data,0,sizeof(data));
    memset(initial,0,sizeof(initial));
    memset(output,0,sizeof(output));
    memset(description,0,sizeof(description));
    for(i=0;i<23 && playerPseudo[i];i++) initial[i]=(unsigned short)(unsigned char)playerPseudo[i];
    for(i=0;label[i] && i<63;i++) description[i]=(unsigned short)(unsigned char)label[i];
    data.language=PSP_UTILITY_OSK_LANGUAGE_FRENCH;
    data.lines=1;
    data.unk_24=1;
    data.inputtype=PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.desc=description;
    data.intext=initial;
    data.outtextlength=24;
    data.outtext=output;
    data.outtextlimit=20;
    params.base.size=sizeof(params);
    params.base.language=0;
    params.base.buttonSwap=PSP_UTILITY_ACCEPT_CROSS;
    params.base.graphicsThread=17;
    params.base.accessThread=19;
    params.base.fontThread=18;
    params.base.soundThread=16;
    params.datacount=1;
    params.data=&data;
    if(sceUtilityOskInitStart(&params)<0) return;
    while((status=sceUtilityOskGetStatus())!=PSP_UTILITY_DIALOG_NONE) {
        if(status==PSP_UTILITY_DIALOG_VISIBLE) sceUtilityOskUpdate(1);
        else if(status==PSP_UTILITY_DIALOG_QUIT && !shuttingDown) {
            sceUtilityOskShutdownStart();
            shuttingDown=1;
        }
        sceDisplayWaitVblankStart();
    }
    if(data.result==PSP_UTILITY_OSK_RESULT_CHANGED && output[0]) {
        for(i=0;i<23 && output[i];i++) playerPseudo[i]=(char)(output[i]&0xff);
        playerPseudo[i]='\0';
    }
}

static void drawMenu(void) {
    begin2D();
    if(screen==0) {
        const unsigned char *t=selected==0?tex_main_play:(selected==1?tex_main_settings:tex_main_quit);
        drawTexture(t);
    } else if(screen==1) {
        drawTexture(tex_play_menu);
        /* Selection frame follows the two buttons in Menu_JOUER_V2.png. */
        {
            float top=(selected==0?66.0f:166.0f);
            float bottom=top+54.0f;
            drawRect2D(218.0f,top,470.0f,top+3.0f,0xffffffff);
            drawRect2D(218.0f,bottom-3.0f,470.0f,bottom,0xffffffff);
            drawRect2D(218.0f,top,221.0f,bottom,0xffffffff);
            drawRect2D(467.0f,top,470.0f,bottom,0xffffffff);
        }
    } else if(screen==2) {
        /* Use the supplied settings artwork as the actual screen background. */
        drawTexture(tex_settings);
    } else if(screen==3 || screen==9) {
        /* Keep the matching two-panel game-menu artwork behind live content. */
        drawTexture(tex_play_menu);
    } else if(screen==6) {
        drawTexture(tex_start_adventure);
    } else {
        drawTexture(tex_main_default);
    }
}

static void printOverlay(void) {
    int i;
    pspDebugScreenSetTextColor(0xffffffff);
    if(screen==4) {
        pspDebugScreenSetXY(1,1);
        pspDebugScreenPrintf("MONDE %d/%d  -  NIVEAU %02d/%02d",world,WORLD_COUNT,level,LEVELS_PER_WORLD);
        pspDebugScreenSetXY(1,2);
        pspDebugScreenPrintf("PSEUDO : %s  |  CLES : %d/1  |  VIES : ",playerPseudo,hasKey?1:0);
        if(lives<0) pspDebugScreenPrintf("INFINI");
        else pspDebugScreenPrintf("%d",lives);
        pspDebugScreenSetXY(1,3); pspDebugScreenPrintf("SANTE : %d%%",health);
        pspDebugScreenSetXY(1,31);
        pspDebugScreenPrintf("STICK / FLECHES : BOUGER     START : PAUSE");
    } else if(screen==1) {
        pspDebugScreenSetXY(3,30); pspDebugScreenPrintf("HAUT/BAS : CHOISIR   CROIX : VALIDER   ROND : RETOUR");
    } else if(screen==2) {
        pspDebugScreenSetXY(3,31); pspDebugScreenPrintf("CROIX : MODIFIER   GAUCHE/DROITE : VOLUME   ROND : RETOUR");
    } else if(screen==3) {
        for(i=0;i<SAVE_COUNT;i++) {
            pspDebugScreenSetXY(4,7+i*4);
            if(saves[i].valid)
                pspDebugScreenPrintf("%s SLOT %d  -  MONDE %d / NIVEAU %d  -  VIES %d",
                    selected==i?">":" ",i+1,saves[i].world,saves[i].level,saves[i].lives);
            else pspDebugScreenPrintf("%s SLOT %d  -  VIDE",selected==i?">":" ",i+1);
        }
        pspDebugScreenSetXY(4,30); pspDebugScreenPrintf("CROIX : CHARGER   ROND : RETOUR");
    } else if(screen==5) {
        pspDebugScreenSetXY(19,11); pspDebugScreenPrintf("PAUSE");
        pspDebugScreenSetXY(15,15); pspDebugScreenPrintf("%s REPRENDRE",selected==0?">":" ");
        pspDebugScreenSetXY(15,18); pspDebugScreenPrintf("%s RECOMMENCER LE NIVEAU",selected==1?">":" ");
        pspDebugScreenSetXY(12,23); pspDebugScreenPrintf("RETOUR AU MENU PRINCIPAL DESACTIVE");
    } else if(screen==9) {
        pspDebugScreenSetXY(10,10); pspDebugScreenPrintf("CHOISIS TON PSEUDO");
        pspDebugScreenSetXY(10,15); pspDebugScreenPrintf("PSEUDO ACTUEL : %s",playerPseudo);
        pspDebugScreenSetXY(10,20); pspDebugScreenPrintf("CROIX : OUVRIR LE CLAVIER PSP");
        pspDebugScreenSetXY(10,24); pspDebugScreenPrintf("START : VALIDER ET CONTINUER");
    } else if(screen==6) {
        static const char *names[6]={"FACILE - 5 VIES","NORMAL - 3 VIES","DIFFICILE - 2 VIES","EXTREME - 1 VIE","HARDCORE - 1 ESSAI","ENTRAINEMENT - INFINI"};
        pspDebugScreenSetXY(10,5); pspDebugScreenPrintf("CHOISIS TA DIFFICULTE");
        for(i=0;i<6;i++) { pspDebugScreenSetXY(9,9+i*3); pspDebugScreenPrintf("%s %s",selected==i?">":" ",names[i]); }
    } else if(screen==7) {
        pspDebugScreenSetXY(19,12); pspDebugScreenPrintf("PARTIE TERMINEE");
        pspDebugScreenSetXY(10,17); pspDebugScreenPrintf("CROIX : RETOUR AU MENU PRINCIPAL");
    } else if(screen==8) {
        pspDebugScreenSetXY(19,12); pspDebugScreenPrintf("BRAVO !");
        pspDebugScreenSetXY(8,16); pspDebugScreenPrintf("TU AS TERMINE LES 6 MONDES !");
        pspDebugScreenSetXY(10,20); pspDebugScreenPrintf("CROIX : MENU PRINCIPAL");
    }
    if(noticeFrames>0) {
        pspDebugScreenSetTextColor(0xffffff55);
        pspDebugScreenSetXY(3,28);
        pspDebugScreenPrintf("%s",notice);
    }
}

static void drawSettingsWidgets(void) {
    float fill=1.70f*(float)volume;
    begin2D();
    /* The supplied artwork already contains the switch and slider tracks.
       Draw only the live switch knob and the live music-level fill. */
    drawRect2D(soundOn?431.0f:386.0f,72.0f,
               soundOn?449.0f:404.0f,103.0f,0xffeeeeee);
    drawRect2D(279.0f,121.0f,279.0f+fill,137.0f,0xff35b8f4);
    /* The ambience control mirrors the master volume until separate audio
       buses are implemented; do not imply that audio playback exists yet. */
    drawRect2D(213.0f,177.0f,213.0f+2.30f*(float)volume,192.0f,0xffff542d);
}

static void drawFrame(void) {
    sceGuStart(GU_DIRECT,displayList);
    sceGuOffset(2048-(SCREEN_W/2),2048-(SCREEN_H/2));
    sceGuViewport(2048,2048,SCREEN_W,SCREEN_H);
    sceGuScissor(0,0,SCREEN_W,SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuClearColor(0xff252a33);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    if(screen==4) { drawGameplay(); drawHealthBar(); }
    else { drawMenu(); if(screen==2) drawSettingsWidgets(); }
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    {
        void *fb=sceGuSwapBuffers();
        pspDebugScreenSetOffset((int)fb);
        if(screen!=0 || noticeFrames>0) printOverlay();
    }
}

int main(void) {
    unsigned int oldButtons=0;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    pspDebugScreenInit();
    sceGuInit();
    sceGuStart(GU_DIRECT,displayList);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_W);
    sceGuDispBuffer(SCREEN_W,SCREEN_H,(void*)0x88000,BUF_W);
    sceGuDepthBuffer((void*)0x110000,BUF_W);
    sceGuOffset(2048-(SCREEN_W/2),2048-(SCREEN_H/2));
    sceGuViewport(2048,2048,SCREEN_W,SCREEN_H);
    sceGuFinish(); sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    loadSaves();
    loadSettings();
    startMenuAudio();

    while(1) {
        SceCtrlData pad;
        unsigned int press;
        sceCtrlReadBufferPositive(&pad,1);
        press=pad.Buttons & ~oldButtons;
        oldButtons=pad.Buttons;

        if(screen==4) {
            updateGameplay(&pad,press);
        } else if(screen==0) {
            if(press & PSP_CTRL_UP) { selected--; if(selected<0) selected=2; }
            if(press & PSP_CTRL_DOWN) { selected++; if(selected>2) selected=0; }
            if(press & PSP_CTRL_CROSS) {
                if(selected==0) { screen=1; selected=0; }
                else if(selected==1) { screen=2; selected=0; }
                else sceKernelExitGame();
            }
        } else if(screen==1) {
            if(press & PSP_CTRL_UP) selected=0;
            if(press & PSP_CTRL_DOWN) selected=1;
            if(press & PSP_CTRL_CROSS) {
                if(selected==0) { screen=3; selected=0; }
                else { screen=9; selected=0; }
            }
            if(press & PSP_CTRL_CIRCLE) { screen=0; selected=0; }
        } else if(screen==2) {
            if(press & PSP_CTRL_UP) selected=0;
            if(press & PSP_CTRL_DOWN) selected=1;
            if(selected==0 && (press & PSP_CTRL_CROSS)) { soundOn=!soundOn; writeSettings(); }
            if(selected==1 && (press & PSP_CTRL_LEFT)) { volume-=5; if(volume<0) volume=0; writeSettings(); }
            if(selected==1 && (press & PSP_CTRL_RIGHT)) { volume+=5; if(volume>100) volume=100; writeSettings(); }
            if(press & PSP_CTRL_CIRCLE) { screen=0; selected=1; }
        } else if(screen==3) {
            if(press & PSP_CTRL_UP) { selected--; if(selected<0) selected=SAVE_COUNT-1; }
            if(press & PSP_CTRL_DOWN) { selected++; if(selected>=SAVE_COUNT) selected=0; }
            if(press & PSP_CTRL_CROSS) loadSave(selected);
            if(press & PSP_CTRL_CIRCLE) { screen=1; selected=0; }
        } else if(screen==9) {
            if(press & PSP_CTRL_CROSS) openPseudoKeyboard();
            if(press & PSP_CTRL_START) { screen=6; selected=1; }
            if(press & PSP_CTRL_CIRCLE) { screen=1; selected=1; }
        } else if(screen==6) {
            if(press & PSP_CTRL_UP) { selected--; if(selected<0) selected=5; }
            if(press & PSP_CTRL_DOWN) { selected++; if(selected>5) selected=0; }
            if(press & PSP_CTRL_CROSS) startGame();
            if(press & PSP_CTRL_CIRCLE) { screen=1; selected=1; }
        } else if(screen==5) {
            if(press & PSP_CTRL_UP) selected=0;
            if(press & PSP_CTRL_DOWN) selected=1;
            if(press & PSP_CTRL_CROSS) {
                if(selected==0) screen=4;
                else if(selected==1) { hasKey=0; health=100; resetPosition(); saveProgress(); screen=4; }
            }
            if((press & PSP_CTRL_START) || (press & PSP_CTRL_CIRCLE)) screen=4;
        } else if(screen==7 || screen==8) {
            if(press & PSP_CTRL_CROSS) { screen=0; selected=0; }
        }

        /* Menus and pause screens have music; active gameplay fades it out in one second. */
        menuAudioDesiredGain=(screen==4 || !soundOn)?0:volume;
        if(noticeFrames>0) noticeFrames--;
        drawFrame();
    }
    sceKernelExitGame();
    return 0;
}