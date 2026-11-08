#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/console.h>
#include <ogc/isfs.h>
#include <fat.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rfl.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static GXRModeObj *rmode;
static void *xfb[2];
static int fb = 0;
static void *fifo;

#define NBG 6
#define NLUZ 5

/* NAND explorer */
#define DIR_MAX 24
#define DIR_NAME_LEN 13
#define DIR_STRIDE 320
#define NAND_DIRS 4

static const char *nand_paths[NAND_DIRS] = {
    "/shared2",
    "/shared2/menu",
    "/shared2/menu/VC",
    "/shared2/menu/FaceLib"
};

static char nand_names[NAND_DIRS][DIR_STRIDE]
    ATTRIBUTE_ALIGN(32);

static u32 nand_counts[NAND_DIRS];
static int nand_results[NAND_DIRS];

/* SD resource paths */
#define SD_PATHS 3

static const char *sd_paths[SD_PATHS] = {
    "sd:/RFL_Res.dat",
    "sd:/FaceLib/RFL_Res.dat",
    "sd:/apps/MiiVisor/RFL_Res.dat"
};

static int sd_ready = 0;
static int sd_result = -999;
static int sd_resource_loaded = 0;
static char resource_path[128] = "Ninguno";
static int nand_rfl_error = -999;

static const GXColor BG[NBG] = {
    {20,24,40,255},
    {240,240,240,255},
    {90,170,90,255},
    {200,80,80,255},
    {0,0,0,255},
    {135,206,235,255}
};

typedef struct {
    f32 x,y,z;
    GXColor col,amb;
    int lit;
} Luz;

static const Luz LUZ[NLUZ] = {
    {0,6,30,{255,255,255,255},{110,110,110,255},1},
    {-25,5,12,{255,215,170,255},{60,60,70,255},1},
    {0,35,8,{255,255,255,255},{50,50,60,255},1},
    {28,-2,14,{255,60,200,255},{0,35,70,255},1},
    {0,0,0,{255,255,255,255},{255,255,255,255},0}
};

static RFLModel model;
static int model_ok = 0;
static int rfl_error = -999;
static int make_error = -999;

/* -------------------------------------------------- */
/* SD RESOURCE LOADER                                 */
/* -------------------------------------------------- */

static int load_resource_sd(void) {
    if (!sd_ready)
        return -20;

    for (int i = 0; i < SD_PATHS; i++) {
        FILE *f = fopen(sd_paths[i], "rb");

        if (!f)
            continue;

        if (fseek(f, 0, SEEK_END) != 0) {
            fclose(f);
            continue;
        }

        long file_size = ftell(f);

        if (file_size < 80 || file_size > 0x7FFFFFF0L) {
            fclose(f);
            continue;
        }

        if (fseek(f, 0, SEEK_SET) != 0) {
            fclose(f);
            continue;
        }

        u32 sz = (u32)file_size;
        u32 alloc_sz = (sz + 31u) & ~31u;

        u8 *buffer = (u8 *)memalign(32, alloc_sz);

        if (!buffer) {
            fclose(f);
            return -13;
        }

        memset(buffer, 0, alloc_sz);

        size_t got = fread(buffer, 1, sz, f);
        fclose(f);

        if (got != sz) {
            free(buffer);
            continue;
        }

        const u8 *old_res = rfl_res;
        u32 old_len = rfl_len;

        rfl_res = buffer;
        rfl_len = sz;

        u32 section_len = 0;
        const u8 *face_data =
            rfl_file(A_FACE, 0, &section_len);

        if (!face_data ||
            section_len < 4 ||
            memcmp(face_data, "face", 4) != 0) {

            rfl_res = old_res;
            rfl_len = old_len;
            free(buffer);
            continue;
        }

        if (old_res)
            free((void *)old_res);

        snprintf(
            resource_path,
            sizeof(resource_path),
            "%s",
            sd_paths[i]
        );

        sd_resource_loaded = 1;

        return 0;
    }

    return -21;
}

/* -------------------------------------------------- */
/* NAND DIRECTORY SCAN                                */
/* -------------------------------------------------- */

static void scan_nand_dirs(void) {
    for (int i = 0; i < NAND_DIRS; i++) {
        memset(nand_names[i], 0, sizeof(nand_names[i]));

        nand_counts[i] = DIR_MAX;

        nand_results[i] = ISFS_ReadDir(
            nand_paths[i],
            nand_names[i],
            &nand_counts[i]
        );
    }
}

/* -------------------------------------------------- */
/* VIDEO                                              */
/* -------------------------------------------------- */

static void video_init(void) {
    VIDEO_Init();

    rmode = VIDEO_GetPreferredMode(NULL);

    xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    if (!xfb[0] || !xfb[1])
        return;

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb[fb]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    fifo = memalign(32,256*1024);

    if (!fifo)
        return;

    memset(fifo,0,256*1024);
    GX_Init(fifo,256*1024);

    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_SetNumTevStages(1);

    GX_SetTevOrder(
        GX_TEVSTAGE0,
        GX_TEXCOORDNULL,
        GX_TEXMAP_NULL,
        GX_COLOR0A0
    );

    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GX_SetCullMode(GX_CULL_NONE);

    GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
    GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);

    f32 ys = GX_GetYScaleFactor(
        rmode->efbHeight,
        rmode->xfbHeight
    );

    u32 xh = GX_SetDispCopyYScale(ys);

    GX_SetDispCopySrc(
        0,0,
        rmode->fbWidth,
        rmode->efbHeight
    );

    GX_SetDispCopyDst(rmode->fbWidth,xh);

    GX_SetCopyFilter(
        rmode->aa,
        rmode->sample_pattern,
        GX_TRUE,
        rmode->vfilter
    );

    GX_SetFieldMode(
        rmode->field_rendering,
        (rmode->viHeight == 2*rmode->xfbHeight)
            ? GX_ENABLE : GX_DISABLE
    );

    GX_SetPixelFmt(
        rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24,
        GX_ZC_LINEAR
    );

    GX_SetDispCopyGamma(GX_GM_1_0);

    GX_SetVtxAttrFmt(
        GX_VTXFMT1,
        GX_VA_POS,
        GX_POS_XYZ,
        GX_F32,
        0
    );

    GX_SetVtxAttrFmt(
        GX_VTXFMT1,
        GX_VA_NRM,
        GX_NRM_XYZ,
        GX_F32,
        0
    );
}

/* -------------------------------------------------- */
/* LIGHTING                                           */
/* -------------------------------------------------- */

static void luz_set(const Luz *l) {
    GXLightObj lo;

    GX_InitLightPos(&lo,l->x,l->y,l->z);
    GX_InitLightColor(&lo,l->col);
    GX_LoadLightObj(&lo,GX_LIGHT0);
}

/* -------------------------------------------------- */
/* GEOMETRY                                           */
/* -------------------------------------------------- */

static void ell(
    f32 cx,f32 cy,f32 cz,
    f32 rx,f32 ry,f32 rz
) {
    enum { ST=12, SL=20 };

    for (int i=0; i<ST; i++) {
        GX_Begin(
            GX_TRIANGLESTRIP,
            GX_VTXFMT1,
            (SL+1)*2
        );

        for (int j=0; j<=SL; j++) {
            f32 b=2*M_PI*j/SL;

            for (int k=0; k<2; k++) {
                f32 a=M_PI*(i+k)/ST;

                f32 ux=sinf(a)*cosf(b);
                f32 uy=cosf(a);
                f32 uz=sinf(a)*sinf(b);

                f32 nx=ux/rx;
                f32 ny=uy/ry;
                f32 nz=uz/rz;

                f32 n=sqrtf(nx*nx+ny*ny+nz*nz);

                GX_Position3f32(
                    cx+ux*rx,
                    cy+uy*ry,
                    cz+uz*rz
                );

                GX_Normal3f32(
                    nx/n,
                    ny/n,
                    nz/n
                );
            }
        }

        GX_End();
    }
}

static void ell_part(
    Mtx base,
    f32 x,f32 y,f32 z,
    f32 sx,f32 sy,f32 sz,
    f32 rz,
    GXColor color,
    const Luz *l
) {
    Mtx t,r,sc,tmp,local,world,nrm;

    guMtxTrans(t,x,y,z);
    guMtxRotDeg(r,'z',rz);
    guMtxScale(sc,sx,sy,sz);

    guMtxConcat(t,r,tmp);
    guMtxConcat(tmp,sc,local);
    guMtxConcat(base,local,world);
    guMtxInvXpose(world,nrm);

    GX_SetChanMatColor(GX_COLOR0A0,color);
    GX_LoadPosMtxImm(world,GX_PNMTX0);
    GX_LoadNrmMtxImm(nrm,GX_PNMTX0);
    GX_SetCurrentMtx(GX_PNMTX0);

    ell(0,0,0,1,1,1);
}

/* -------------------------------------------------- */
/* BODY                                               */
/* -------------------------------------------------- */

static void body(
    const RFLInfo *m,
    Mtx vr,
    const Luz *l,
    f32 fh,
    f32 fw
) {
    GXColor shirt = RFL_FAV[m->favC % 12];
    GXColor pants = {42,48,62,255};
    GXColor skin = RFL_FACE[m->faceC < 6 ? m->faceC : 0];
    GXColor shoes = {28,27,32,255};
    GXColor eye = {25,22,24,255};
    GXColor mouth = {135,45,48,255};

    GX_SetCullMode(GX_CULL_NONE);
    GX_ClearVtxDesc();

    GX_SetVtxAttrFmt(
        GX_VTXFMT1,
        GX_VA_POS,
        GX_POS_XYZ,
        GX_F32,
        0
    );

    GX_SetVtxAttrFmt(
        GX_VTXFMT1,
        GX_VA_NRM,
        GX_NRM_XYZ,
        GX_F32,
        0
    );

    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_NRM,GX_DIRECT);

    GX_SetNumTexGens(0);
    GX_SetNumChans(1);
    GX_SetNumTevStages(1);

    GX_SetTevOrder(
        GX_TEVSTAGE0,
        GX_TEXCOORDNULL,
        GX_TEXMAP_NULL,
        GX_COLOR0A0
    );

    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);

    GX_SetChanCtrl(
        GX_COLOR0A0,
        l->lit ? GX_ENABLE : GX_DISABLE,
        GX_SRC_REG,
        GX_SRC_REG,
        GX_LIGHT0,
        GX_DF_CLAMP,
        GX_AF_NONE
    );

    GX_SetChanAmbColor(GX_COLOR0A0,l->amb);

    ell_part(vr,0,-0.83f*fh,0,
        0.57f*fw,0.78f*fh,0.34f*fw,0,shirt,l);

    ell_part(vr,0,-0.13f,0,
        0.19f*fw,0.27f*fh,0.19f*fw,0,skin,l);

    if (!model_ok) {
        ell_part(vr,0,0.91f,0,
            0.48f*fw,0.62f*fh,0.39f*fw,0,skin,l);

        ell_part(vr,-0.49f*fw,0.91f,0,
            0.12f*fw,0.19f*fh,0.12f*fw,0,skin,l);

        ell_part(vr,0.49f*fw,0.91f,0,
            0.12f*fw,0.19f*fh,0.12f*fw,0,skin,l);

        ell_part(vr,-0.18f*fw,1.00f,0.345f*fw,
            0.055f*fw,0.075f*fh,0.035f*fw,0,eye,l);

        ell_part(vr,0.18f*fw,1.00f,0.345f*fw,
            0.055f*fw,0.075f*fh,0.035f*fw,0,eye,l);

        ell_part(vr,0,0.78f,0.365f*fw,
            0.13f*fw,0.035f*fh,0.035f*fw,0,mouth,l);
    }

    ell_part(vr,-0.57f*fw,-0.53f*fh,0,
        0.22f*fw,0.28f*fh,0.23f*fw,-24,shirt,l);

    ell_part(vr,0.57f*fw,-0.53f*fh,0,
        0.22f*fw,0.28f*fh,0.23f*fw,24,shirt,l);

    ell_part(vr,-0.76f*fw,-0.93f*fh,0,
        0.17f*fw,0.43f*fh,0.17f*fw,-9,shirt,l);

    ell_part(vr,0.76f*fw,-0.93f*fh,0,
        0.17f*fw,0.43f*fh,0.17f*fw,9,shirt,l);

    ell_part(vr,-0.83f*fw,-1.34f*fh,0.015f,
        0.16f*fw,0.18f*fh,0.16f*fw,0,skin,l);

    ell_part(vr,0.83f*fw,-1.34f*fh,0.015f,
        0.16f*fw,0.18f*fh,0.16f*fw,0,skin,l);

    ell_part(vr,0,-1.49f*fh,0,
        0.49f*fw,0.25f*fh,0.31f*fw,0,pants,l);

    ell_part(vr,-0.27f*fw,-1.82f*fh,0,
        0.22f*fw,0.48f*fh,0.22f*fw,-3,pants,l);

    ell_part(vr,0.27f*fw,-1.82f*fh,0,
        0.22f*fw,0.48f*fh,0.22f*fw,3,pants,l);

    ell_part(vr,-0.27f*fw,-2.17f*fh,0.035f,
        0.24f*fw,0.15f*fh,0.34f*fw,0,shoes,l);

    ell_part(vr,0.27f*fw,-2.17f*fh,0.035f,
        0.24f*fw,0.15f*fh,0.34f*fw,0,shoes,l);
}

/* -------------------------------------------------- */
/* MII RENDER                                         */
/* -------------------------------------------------- */

static void draw_mii(
    const RFLInfo *m,
    const RFLModel *mod,
    f32 ang,
    const Luz *l,
    int show_body
) {
    Mtx v,r,vr,t,s,a,b,h;

    guVector cam={0,0.1f,9.5f};
    guVector up={0,1,0};
    guVector look={0,-0.25f,0};

    guLookAt(v,&cam,&up,&look);

    guMtxRotDeg(r,'y',ang);
    guMtxConcat(v,r,vr);

    luz_set(l);

    f32 fh=0.85f+m->height/127.0f*0.35f;
    f32 fw=0.75f+m->build/127.0f*0.55f;
    f32 hy=0.05f*fh+0.85f;

    /* Dibuja el cuerpo procedimental */
    if (show_body) {
        body(m,vr,l,fh,fw);
    }

    /* Dibuja la cabeza, pelo, cara y accesorios del Mii */
    if (model_ok && mod->hgt > 0.001f) {
        f32 sc = 1.9f / mod->hgt;

        guMtxTrans(t,0,hy,0);
        guMtxScale(s,sc,sc,sc);
        guMtxConcat(t,s,a);

        guMtxTrans(
            t,
            -mod->ctr[0],
            -mod->ctr[1],
            -mod->ctr[2]
        );

        guMtxConcat(a,t,b);
        guMtxConcat(vr,b,h);

        Mtx hn;
        guMtxInvXpose(h,hn);

        RFL_Draw(mod,h,hn,l->lit,l->amb);
    }
}

/* -------------------------------------------------- */
/* MODEL                                              */
/* -------------------------------------------------- */

static void clear_efb(int bg) {
    GX_SetCopyClear(BG[bg],0x00ffffff);
    GX_CopyDisp(xfb[fb],GX_TRUE);
    GX_DrawDone();
}

static void rebuild(const RFLInfo *m, int bg) {
    RFL_Free(&model);
    memset(&model, 0, sizeof(model));

    model_ok = 0;

    make_error = RFL_Make(
        &model,
        m,
        RFL_R256M,
        (1 << RFL_NORMAL) |
        (1 << RFL_SMILE) |
        (1 << RFL_ANGER) |
        (1 << RFL_SORROW) |
        (1 << RFL_SURPRISE) |
        (1 << RFL_BLINK) |
        (1 << RFL_OPENMOUTH)
    );

    model_ok = (make_error == 0 && model.face.dls != 0);

    if (!model_ok)
        RFL_Free(&model);

    clear_efb(bg);
}

/* -------------------------------------------------- */
/* DIAGNOSTIC SCREEN                                  */
/* -------------------------------------------------- */

static void show_diagnostics(
    int loaded_count,
    const RFLInfo *m
) {
    int page=0;
    const int total_pages=7;

    while (1) {
        WPAD_ScanPads();

        u32 down=WPAD_ButtonsDown(0);

        if (down&WPAD_BUTTON_RIGHT)
            page=(page+1)%total_pages;

        if (down&WPAD_BUTTON_LEFT)
            page=(page+total_pages-1)%total_pages;

        if (down&WPAD_BUTTON_A)
            break;

        console_init(
            xfb[0],
            20,
            20,
            rmode->fbWidth,
            rmode->xfbHeight,
            rmode->fbWidth*VI_DISPLAY_PIX_SZ
        );

        printf("\n");
        printf("========================================\n");
        printf("             MiiVisor - DEBUG           \n");
        printf("========================================\n");

        printf("       Pagina %d de %d\n\n",
            page+1,total_pages);

        if (page==0) {
            printf("--- ESTADO GENERAL ---\n\n");

            printf("RFL_Init NAND:  %d\n",nand_rfl_error);
            printf("RFL_Init final: %d\n",rfl_error);
            printf("RFL_Load:       %d Miis\n",loaded_count);
            printf("RFL_Make:       %d\n",make_error);
            printf("Model OK:       %d\n\n",model_ok);

            printf("SD inicializada: %s\n",
                sd_ready ? "SI" : "NO");

            printf("SD resultado: %d\n",
                sd_result);

            printf("Recurso desde SD: %s\n",
                sd_resource_loaded ? "SI" : "NO");

            printf("Ruta recurso:\n%s\n\n",
                resource_path);

            printf("Resource length:\n%u bytes\n\n",
                (unsigned)rfl_len);

            printf("Recurso disponible: %s\n",
                rfl_res ? "SI" : "NO");
        }

        else if (page>=1 && page<=4) {
            int d=page-1;

            printf("--- EXPLORADOR NAND ---\n\n");
            printf("Ruta:\n%s\n\n",nand_paths[d]);

            printf("ReadDir: %d\n",nand_results[d]);

            if (nand_results[d]==0) {
                printf("Entradas: %u\n\n",
                    (unsigned)nand_counts[d]);

                if (nand_counts[d]==0) {
                    printf("(Directorio vacio)\n");
                }

                for (u32 i=0; i<nand_counts[d]; i++) {
                    printf("  %s\n",
                        nand_names[d]+i*DIR_NAME_LEN);
                }

                if (nand_counts[d]>=DIR_MAX) {
                    printf("\nLimite de entradas alcanzado.\n");
                }
            } else {
                printf("\nNo se pudo leer este directorio.\n");
            }
        }

        else if (page==5) {
            printf("--- DATOS DEL MII ---\n\n");

            if (m) {
                printf("Nombre: %s\n\n",m->name);

                printf("Genero: %u\n",m->gender);
                printf("Color favorito: %u\n",m->favC);
                printf("Altura: %u\n",m->height);
                printf("Complexion: %u\n\n",m->build);

                printf("Rostro: %u\n",m->faceT);
                printf("Color rostro: %u\n",m->faceC);
                printf("Textura rostro: %u\n",m->faceTex);

                printf("Cabello: %u\n",m->hairT);
                printf("Color cabello: %u\n",m->hairC);
                printf("Cabello invertido: %u\n\n",m->hairFlip);

                printf("Cejas: %u\n",m->browT);
                printf("Ojos: %u\n",m->eyeT);
                printf("Nariz: %u\n",m->noseT);
                printf("Boca: %u\n",m->mouthT);
                printf("Lentes: %u\n",m->glassT);
            }
        }

        else if (page==6) {
            printf("--- MODELO 3D ---\n\n");

            printf("Face DL: %u\n",
                (unsigned)model.face.dls);

            printf("Face vertices: %u\n",
                (unsigned)model.face.nv);

            printf("Model height: %f\n\n",
                model.hgt);

            printf("Cap DL: %u\n",
                (unsigned)model.cap.dls);

            printf("Hair DL: %u\n",
                (unsigned)model.hair.dls);

            printf("Fore DL: %u\n",
                (unsigned)model.fore.dls);

            printf("Beard DL: %u\n\n",
                (unsigned)model.beard.dls);

            printf("Nose DL: %u\n",
                (unsigned)model.nose.dls);

            printf("Nline DL: %u\n",
                (unsigned)model.nline.dls);

            printf("Mask DL: %u\n",
                (unsigned)model.mask.dls);

            printf("Glass DL: %u\n",
                (unsigned)model.glass.dls);
        }

        printf("\n========================================\n");
        printf("< IZQUIERDA       DERECHA >\n");
        printf("          A: CONTINUAR\n");
        printf("========================================\n");

        VIDEO_Flush();
        VIDEO_WaitVSync();
    }
}

/* -------------------------------------------------- */
/* MAIN                                               */
/* -------------------------------------------------- */

int main(void) {
    video_init();

    if (!rmode || !xfb[0] || !xfb[1] || !fifo)
        return 1;

    WPAD_Init();

    static RFLInfo lista[RFL_MAX];
    memset(lista,0,sizeof(lista));

    /*
     * 1. Inicializar SD.
     */
    sd_ready = fatInitDefault() ? 1 : 0;
    sd_result = sd_ready ? 0 : -1;

    /*
     * 2. Intentar cargar recursos desde NAND.
     */
    nand_rfl_error = RFL_Init(rmode);
    rfl_error = nand_rfl_error;

    /*
     * 3. Si NAND no tiene los recursos, buscarlos en SD.
     */
    if (nand_rfl_error != 0) {
        int sd_load_result = load_resource_sd();

        if (sd_load_result == 0) {
            rfl_error = 0;
        } else {
            sd_result = sd_load_result;
        }
    }

    /*
     * 4. Explorar NAND y cargar Miis.
     */
    scan_nand_dirs();

    int loaded_count=RFL_Load(lista,RFL_MAX);
    int n=loaded_count;

    if (!n) {
        memset(&lista[0],0,sizeof(RFLInfo));
        lista[0].height=64;
        lista[0].build=64;
        n=1;
    }

    int sel=0;
    int luz=0;
    int bg=0;
    int giro=1;
    int cuerpo=1;
    int ex=RFL_NORMAL;

    /*
     * 5. Construir el modelo si hay Mii y recursos.
     */
    if (loaded_count > 0 && rfl_res != NULL) {
        rebuild(&lista[0],bg);
    } else {
        make_error = -999;
        model_ok = 0;
        clear_efb(bg);
    }

    /* Mostrar pantalla de diagnóstico (presionar A para continuar) */
    show_diagnostics(loaded_count,&lista[0]);

    /* ================================================== */
    /* REINICIALIZAR ESTADO DE GX TRAS SALIR DE CONSOLA   */
    /* ================================================== */
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);

    f32 ang=0;
    Mtx44 proj;

    guPerspective(
        proj,
        45,
        (f32)rmode->fbWidth/(f32)rmode->efbHeight,
        0.1f,
        100.0f
    );

    while (1) {
        WPAD_ScanPads();

        u32 d=WPAD_ButtonsDown(0);
        u32 held=WPAD_ButtonsHeld(0);

        if (d&WPAD_BUTTON_HOME)
            break;

        int old=sel;

        if (d&WPAD_BUTTON_RIGHT)
            sel=(sel+1)%n;

        if (d&WPAD_BUTTON_LEFT)
            sel=(sel+n-1)%n;

        if (sel!=old && loaded_count>0) {
            if (rfl_res != NULL) {
                rebuild(&lista[sel],bg);
            } else {
                model_ok=0;
                make_error=-999;
            }

            ex=RFL_NORMAL;
        }

        if ((d & WPAD_BUTTON_2) && model_ok) {
            for (int k = 1; k < RFL_NEXP; k++) {
                int next = (ex + k) % RFL_NEXP;

                if (RFL_SetExp(&model, next) == 0) {
                    ex = next;
                    break;
                }
            }
        }

        if (d&WPAD_BUTTON_1)
            cuerpo^=1;

        if (d&WPAD_BUTTON_UP)
            luz=(luz+1)%NLUZ;

        if (d&WPAD_BUTTON_DOWN)
            bg=(bg+1)%NBG;

        if (d&WPAD_BUTTON_A)
            giro^=1;

        if (held&WPAD_BUTTON_PLUS)
            ang+=2;

        if (held&WPAD_BUTTON_MINUS)
            ang-=2;

        if (giro)
            ang+=1;

        GX_SetViewport(
            0,0,
            rmode->fbWidth,
            rmode->efbHeight,
            0,1
        );

        GX_LoadProjectionMtx(proj,GX_PERSPECTIVE);
        GX_SetCopyClear(BG[bg],0x00ffffff);

        draw_mii(
            &lista[sel],
            &model,
            ang,
            &LUZ[luz],
            cuerpo
        );

        GX_DrawDone();

        fb^=1;

        GX_CopyDisp(xfb[fb],GX_TRUE);

        VIDEO_SetNextFramebuffer(xfb[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }

    RFL_Free(&model);

    /*
     * Liberar recursos cargados desde SD.
     */
    if (rfl_res) {
        free((void *)rfl_res);
        rfl_res = NULL;
        rfl_len = 0;
    }

    ISFS_Deinitialize();

    return 0;
}
