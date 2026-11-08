// rfl.h - Mii (Revolution Face Library) para devkitPPC/libogc. Header-only.
// Port compacto de la decompilacion de RVL FaceLib (koopthekoopa/RFL): mismos modelos, TEV, colores,
// escalas y render de la mascara (ojos/cejas/boca/barba/lunar) a textura por EFB.
// NOTA: FaceLib solo dibuja la CABEZA (cara, pelo, nariz, barba, lentes, gorro del pelo, mascara facial).
//       El cuerpo NO esta en RFL_Res.dat: cada juego trae sus propios modelos de cuerpo.
#ifndef RFL_H
#define RFL_H
#include <gccore.h>
#include <ogc/isfs.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Culling al dibujar la cabeza. El original usa GX_CULL_BACK (y lo invierte con el pelo volteado).
// NONE siempre se ve completo gracias al Z-buffer; si quieres el comportamiento original usa -DRFL_CULL=GX_CULL_BACK
#ifndef RFL_CULL
#define RFL_CULL GX_CULL_NONE
#endif

#define RFL_RES "/shared2/menu/FaceLib/RFL_Res.dat"
#define RFL_DB  "/shared2/menu/FaceLib/RFL_DB.dat"
#define RFL_MAX 100
enum { RFL_R64M = 96, RFL_R128M = 224, RFL_R256M = 480 };   // resolucion de mascara (bits 32|64|128|256)
enum { RFL_NORMAL, RFL_SMILE, RFL_ANGER, RFL_SORROW, RFL_SURPRISE, RFL_BLINK, RFL_OPENMOUTH, RFL_NEXP };
enum { A_BEARD, A_EYE, A_BROW, A_FACE, A_FACETEX, A_FORE, A_GLASS, A_GLASSTEX, A_HAIR, A_MASK, A_MOLE,
       A_MOUTH, A_MUST, A_NOSE, A_NLINE, A_NLINETEX, A_CAP, A_CAPTEX };   // orden de secciones de RFL_Res.dat

typedef struct {   // RFLiCharInfo en plano
    char name[11];
    u8 gender, favC, height, build;
    u8 faceT, faceC, faceTex, hairT, hairC, hairFlip;
    u8 browT, browRot, browC, browS, browY, browX;
    u8 eyeT, eyeRot, eyeC, eyeS, eyeY, eyeX;
    u8 noseT, noseS, noseY, mouthT, mouthC, mouthS, mouthY;
    u8 glassT, glassC, glassS, glassY;
    u8 mustT, beardT, beardC, beardS, beardY;
    u8 moleT, moleS, moleX, moleY;
} RFLInfo;

typedef struct { s16 *p, *n, *t; u8 *dl; u32 dls, nv; } RShp;
typedef struct { GXTexObj o; void *img; } RTex;
typedef struct {
    RShp face, cap, hair, fore, beard, nose, nline, mask, glass;
    RTex tFace, tCap, tNline, tGlass;
    GXTexObj mtex[RFL_NEXP]; void *mbuf[RFL_NEXP]; int mok[RFL_NEXP], exp;
    int flip, cFace, cHair, cBeard, cGlass, cFav;
    float ctr[3], hgt;   // centro y alto de la cara (para encuadrar)
} RFLModel;

static const GXColor RFL_FACE[6] = {{240,216,196,255},{255,188,128,255},{216,136,80,255},{255,176,144,255},{152,80,48,255},{82,46,28,255}};
static const GXColor RFL_HAIR[8] = {{30,26,24,255},{56,32,21,255},{85,38,23,255},{112,64,36,255},{114,114,120,255},{73,54,26,255},{122,89,40,255},{193,159,100,255}};
static const GXColor RFL_GLASS[6] = {{16,16,16,255},{96,56,16,255},{152,24,16,255},{32,48,96,255},{144,88,0,255},{96,88,80,255}};
static const GXColor RFL_FAV[12] = {{184,64,48,255},{240,120,40,255},{248,216,32,255},{128,200,40,255},{0,116,40,255},{32,72,152,255},
    {64,160,216,255},{232,96,120,255},{112,44,168,255},{72,56,24,255},{224,224,224,255},{24,24,20,255}};
static const GXColor RFL_EYEC1[6] = {{0,0,0,255},{124,128,128,255},{112,80,64,255},{112,110,64,255},{88,104,184,255},{72,128,104,255}};
static const GXColor RFL_MOUTH0[3] = {{190,78,38,255},{216,48,40,255},{207,68,71,255}};
static const GXColor RFL_MOUTH1[3] = {{113,42,4,255},{120,21,16,255},{126,37,40,255}};
static const GXColor RFL_MOLEC = {18,15,15,255};
static const u8 RFL_EYEROT[] = {29,28,28,28,29,28,28,28,29,28,28,28,28,29,29,28,28,28,29,29,28,29,28,29,29,28,29,28,28,29,28,28,
    28,29,29,29,28,28,29,29,29,28,28,29,29,29,29,29,28,28};
static const u8 RFL_BROWROT[] = {26,26,27,25,26,25,26,25,28,25,26,24,27,27,26,26,25,25,26,26,27,26,25,27};

static const u8 *rfl_res; static u32 rfl_len; static GXRModeObj *rfl_rm;

static inline u16 rb16(const u8 *p) { return (u16)((p[0] << 8) | p[1]); }
static inline u32 rb32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }
static inline void *ral(u32 n) {
    u32 size;
    if (n > 0xFFFFFFDFu) return 0;
    size = (n + 31u) & ~31u;
    if (!size) size = 32;
    return memalign(32, size);
}

// FIX: formato de vertices 3D. Las coordenadas de textura de las formas de RFL son S16 con 13 bits
// fraccionales (RFLLoadVertexSetting del original), NO 8. Con 8 la textura de la cara y la mascara
// (ojos/boca/cejas) se muestreaban en el lugar equivocado y no se veian.
static void rvat3d(void) {
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 8);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_S16, 14);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 13);
}

// ---------- recursos ----------
static const u8 *rfl_file(int arc, int idx, u32 *len) {
    u32 so, n, base, a, b;
    if (!len || !rfl_res || rfl_len < 80 || arc < 0 || arc >= 18 || idx < 0)
        return 0;
    so = rb32(rfl_res + 4 + 4 * (u32)arc);
    if (so > rfl_len || rfl_len - so < 8) return 0;
    n = rb16(rfl_res + so);
    if ((u32)idx >= n || n + 1u > (rfl_len - so - 8u) / 4u) return 0;
    base = so + 8u + 4u * n;
    if (base > rfl_len) return 0;
    a = rb32(rfl_res + so + 4u + 4u * (u32)idx);
    b = rb32(rfl_res + so + 8u + 4u * (u32)idx);
    if (b < a || a > rfl_len - base || b > rfl_len - base) return 0;
    *len = b - a;
    return rfl_res + base + a;
}

static int RFL_Init(GXRModeObj *rm) {
    static fstats st ATTRIBUTE_ALIGN(32);
    int fd;
    s32 err;
    rfl_rm = rm;
    if (rfl_res) { free((void *)rfl_res); rfl_res = 0; rfl_len = 0; }
    err = ISFS_Initialize();
    if (err < 0) return -10;
    fd = ISFS_Open(RFL_RES, ISFS_OPEN_READ);
    if (fd < 0) return fd;
    if (ISFS_GetFileStats(fd, &st) < 0) { ISFS_Close(fd); return -11; }
    u32 sz = st.file_length;
    if (sz < 80 || sz > 0x7FFFFFF0u) { ISFS_Close(fd); return -12; }
    u32 alloc_sz = (sz + 31u) & ~31u;
    u8 *b = (u8 *)memalign(32, alloc_sz);
    if (!b) { ISFS_Close(fd); return -13; }
    memset(b, 0, alloc_sz);
    u32 got = 0;
    while (got < sz) {
        u32 remain = sz - got;
        u32 request = remain > 0x8000u ? 0x8000u : ((remain + 31u) & ~31u);
        int r = ISFS_Read(fd, b + got, request);
        if (r <= 0 || (u32)r > alloc_sz - got) break;
        got += (u32)r;
    }
    ISFS_Close(fd);
    if (got < sz) { free(b); return -14; }
    rfl_res = b; rfl_len = sz;
    u32 section_len = 0;
    const u8 *face = rfl_file(A_FACE, 0, &section_len);
    if (!face || section_len < 4 || memcmp(face, "face", 4) != 0) {
        free(b); rfl_res = 0; rfl_len = 0; return -15;
    }
    return 0;
}

// ---------- base de datos ----------
#define BF(w, s, n) ((u8)(((w) >> (s)) & ((1u << (n)) - 1)))
static void RFL_Parse(RFLInfo *m, const u8 *d) {
    u32 w;
    memset(m, 0, sizeof(*m));
    for (int i = 0; i < 10; i++) {
        u16 c = rb16(d + 2 + 2 * i);
        m->name[i] = (c && c < 128) ? (char)c : (c ? '?' : 0);
    }
    m->name[10] = 0;
    w = rb16(d); m->gender = BF(w, 14, 1); m->favC = BF(w, 1, 4);
    m->height = d[0x16]; m->build  = d[0x17];
    // Mii de 74 bytes: 0x20 cara, 0x22 pelo, 0x24 cejas, 0x28 ojos, 0x2C nariz, 0x2E boca, 0x30 lentes, 0x32 barba, 0x34 lunar
    w = rb16(d + 0x20); m->faceT = BF(w, 13, 3); m->faceC = BF(w, 10, 3); m->faceTex = BF(w, 6, 4);
    w = rb16(d + 0x22); m->hairT = BF(w, 9, 7); m->hairC = BF(w, 6, 3); m->hairFlip = BF(w, 5, 1);
    w = rb32(d + 0x24); m->browT = BF(w, 27, 5); m->browRot = BF(w, 22, 5); m->browC = BF(w, 13, 3); m->browS = BF(w, 9, 4); m->browY = BF(w, 4, 5); m->browX = BF(w, 0, 4);
    w = rb32(d + 0x28); m->eyeT = BF(w, 26, 6); m->eyeRot = BF(w, 21, 5); m->eyeY = BF(w, 16, 5); m->eyeC = BF(w, 13, 3); m->eyeS = BF(w, 9, 4); m->eyeX = BF(w, 5, 4);
    w = rb16(d + 0x2C); m->noseT = BF(w, 12, 4); m->noseS = BF(w, 8, 4); m->noseY = BF(w, 3, 5);
    w = rb16(d + 0x2E); m->mouthT = BF(w, 11, 5); m->mouthC = BF(w, 9, 2); m->mouthS = BF(w, 5, 4); m->mouthY = BF(w, 0, 5);
    w = rb16(d + 0x30); m->glassT = BF(w, 12, 4); m->glassC = BF(w, 9, 3); m->glassS = BF(w, 5, 4); m->glassY = BF(w, 0, 5);
    w = rb16(d + 0x32); m->mustT = BF(w, 14, 2); m->beardT = BF(w, 12, 2); m->beardC = BF(w, 9, 3); m->beardS = BF(w, 5, 4); m->beardY = BF(w, 0, 5);
    w = rb16(d + 0x34); m->moleT = BF(w, 15, 1); m->moleS = BF(w, 11, 4); m->moleX = BF(w, 6, 5); m->moleY = BF(w, 1, 5);
    // limites de RFL_Res.dat: 8 caras, 72 pelos, 12 narices, 50 ojos, 24 cejas, 25 bocas, 10 lentes, 4 barbas/bigotes, 12 texturas
    if (m->faceT > 7) m->faceT = 0;
    if (m->hairT > 71) m->hairT = 0;
    if (m->noseT > 11) m->noseT = 0;
    if (m->eyeT > 47) m->eyeT = 0;
    if (m->browT > 23) m->browT = 0;
    if (m->mouthT > 23) m->mouthT = 0;
    if (m->glassT > 8) m->glassT = 0;
    if (m->faceTex > 11) m->faceTex = 0;
}

static int RFL_Load(RFLInfo *out, int max) {
    const u32 len = 0x1D00;
    static fstats st ATTRIBUTE_ALIGN(32);
    int n = 0, fd; u8 *buf;
    if (!out || max <= 0) return 0;
    if (max > RFL_MAX) max = RFL_MAX;
    if (ISFS_Initialize() < 0) return 0;
    buf = (u8 *)memalign(32, len);
    if (!buf) return 0;
    memset(buf, 0, len);
    fd = ISFS_Open(RFL_DB, ISFS_OPEN_READ);
    if (fd >= 0) {
        if (ISFS_GetFileStats(fd, &st) >= 0 && st.file_length >= len && ISFS_Read(fd, buf, len) == (s32)len && memcmp(buf, "RNOD", 4) == 0) {
            for (int i = 0; i < RFL_MAX && n < max; i++) {
                const u8 *d = buf + 4 + (u32)i * 74u;
                if (rb32(d) != 0) { RFL_Parse(&out[n], d); n++; }
            }
        }
        ISFS_Close(fd);
    }
    free(buf); return n;
}

// ---------- formas ----------
static void rshp_free(RShp *s) { free(s->p); free(s->n); free(s->t); free(s->dl); memset(s, 0, sizeof *s); }

static void rshp(RShp *o, int arc, int idx, int uv, int flip, int xf, float sc, const float *tr, float *hdr) {
    u32 len = 0, V, N, T = 0, cap;
    const u8 *f, *p, *end, *vp, *np, *tp = 0;
    int prims, scale, tx, ty, tz;

    if (!o) return;
    memset(o, 0, sizeof *o);
    f = rfl_file(arc, idx, &len);
    if (!f || len < 8) return;
    p = f + 4; end = f + len;

    if (arc == A_FACE) {
        if ((u32)(end - p) < 36) return;
        if (hdr) {
            for (int i = 0; i < 9; i++) {
                // union para garantizar conversion correcta de flotantes Big-Endian
                union { u32 u; float f; } uf;
                uf.u = rb32(p + 4u * i);
                hdr[i] = uf.f;
            }
        }
        p += 36;
    }

    if ((u32)(end - p) < 2) return;
    V = rb16(p); p += 2;
    if (!V || V > 255 || (u32)(end - p) < V * 6u + 2u) return;
    vp = p; p += V * 6u;

    N = rb16(p); p += 2;
    if (!N || N > 255 || (u32)(end - p) < N * 6u) return;
    np = p; p += N * 6u;

    if (uv) {
        if ((u32)(end - p) < 2) return;
        T = rb16(p); p += 2;
        if (T > 255 || (u32)(end - p) < T * 4u) return;
        if (T > 0) { tp = p; p += T * 4u; }
    }
    if (p >= end) return;
    prims = *p++;
    if (prims <= 0) return;

    o->p = (s16 *)ral(V * 6u); o->n = (s16 *)ral(N * 6u); o->t = (s16 *)ral(T * 4u);
    cap = (u32)(end - p) + (u32)prims * 8u + 128u; cap = (cap + 31u) & ~31u;
    o->dl = (u8 *)memalign(32, cap);
    if (!o->p || !o->n || !o->t || !o->dl) { rshp_free(o); return; }

    scale = xf ? (int)(256.0f * sc) : 256;
    tx = (xf && tr) ? (int)(256.0f * tr[0]) : 0;
    ty = (xf && tr) ? (int)(256.0f * tr[1]) : 0;
    tz = (xf && tr) ? (int)(256.0f * tr[2]) : 0;

    for (u32 i = 0; i < V; i++) {
        int x = (s16)rb16(vp + 6u * i); int y = (s16)rb16(vp + 6u * i + 2u); int z = (s16)rb16(vp + 6u * i + 4u);
        o->p[3u * i] = (s16)(tx + (((flip ? -x : x) * scale) >> 8));
        o->p[3u * i + 1u] = (s16)(ty + ((y * scale) >> 8));
        o->p[3u * i + 2u] = (s16)(tz + ((z * scale) >> 8));
    }
    for (u32 i = 0; i < N; i++) {
        int x = (s16)rb16(np + 6u * i);
        o->n[3u * i] = (s16)(flip ? -x : x);
        o->n[3u * i + 1u] = (s16)rb16(np + 6u * i + 2u);
        o->n[3u * i + 2u] = (s16)rb16(np + 6u * i + 4u);
    }
    if (uv && T > 0) for (u32 i = 0; i < T * 2u; i++) o->t[i] = (s16)rb16(tp + 2u * i);

    DCFlushRange(o->p, V * 6u); DCFlushRange(o->n, N * 6u);
    if (uv && T > 0) DCFlushRange(o->t, T * 4u);

    rvat3d();
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_NRM, GX_INDEX8);
    if (uv && T > 0) GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);

    // FIX: el original hace DCInvalidateRange del buffer antes de grabar la display list.
    // Sin esto, lineas de cache sucias del buffer (memoria reciclada por memalign) pueden pisar la lista
    // ya grabada por el GP -> triangulos corruptos/faltantes (cabeza incompleta, pelo que no sale).
    DCInvalidateRange(o->dl, cap);
    GX_BeginDispList(o->dl, cap);
    for (int i = 0; i < prims; i++) {
        int count = p[0]; u8 type = p[1]; p += 2;
        GX_Begin(type, GX_VTXFMT0, count);
        for (int j = 0; j < count; j++) {
            GX_Position1x8(*p++); GX_Normal1x8(*p++);
            if (uv && T > 0) GX_TexCoord1x8(*p++);
            else if (uv && T == 0) p++;
        }
        GX_End();
    }
    o->dls = GX_EndDispList();
    if (!o->dls) { rshp_free(o); return; }
    o->nv = V;
}

// ---------- texturas ----------
static void rtex_free(RTex *t) { free(t->img); memset(t, 0, sizeof *t); }

static int rtex(RTex *t, int arc, int idx, int part) {
    u32 len = 0, io, sz, w, h, alloc_sz;
    const u8 *f = rfl_file(arc, idx, &len);
    if (!t) return -1;
    memset(t, 0, sizeof *t);
    if (!f || len < 0x20) return -1;
    w = rb16(f + 2); h = rb16(f + 4); io = rb32(f + 0x1C);
    if (!w || !h || io >= len || f[0] > GX_TF_CMPR) return -1;
    sz = len - io;
    if (!sz || sz > 0x7FFFFFF0u) return -1;
    alloc_sz = (sz + 31u) & ~31u;
    t->img = memalign(32, alloc_sz);
    if (!t->img) return -1;
    memset(t->img, 0, alloc_sz); memcpy(t->img, f + io, sz); DCFlushRange(t->img, alloc_sz);
    GX_InitTexObj(&t->o, t->img, w, h, f[0], part ? GX_CLAMP : f[6], part ? GX_CLAMP : f[7], GX_FALSE);
    if (part) GX_InitTexObjLOD(&t->o, f[0x14], f[0x15], (s8)f[0x16], (s8)f[0x17], (s16)rb16(f + 0x1A), f[0x12], f[0x11], f[0x13]);
    else GX_InitTexObjLOD(&t->o, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    return 0;
}

// ---------- mascara ----------
typedef struct { RTex t; float x, y, w, h, a; int o; } RPart;

static void rtev(int s, int rs, int ts, int a, int b, int c, int d, int aa, int ab, int ac, int ad) {
    GX_SetTevSwapMode(s, rs, ts);
    GX_SetTevOrder(s, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTevColorIn(s, a, b, c, d); GX_SetTevAlphaIn(s, aa, ab, ac, ad);
    GX_SetTevColorOp(s, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GX_SetTevAlphaOp(s, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}
#define TEXA_ GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA
#define PREVA_ GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV

static void tev1(GXColor c) {
    GX_SetNumTevStages(1);
    rtev(0, GX_TEV_SWAP0, GX_TEV_SWAP0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0, TEXA_);
    GX_SetTevColor(GX_TEVREG0, c);
}

static void tev3(int s0, int s1, int s2, int s3, int s4, int s5, GXColor c0, GXColor c1) {
    GX_SetNumTevStages(3);
    rtev(0, s0, s1, GX_CC_ZERO, GX_CC_C0, GX_CC_TEXC, GX_CC_ZERO, TEXA_);
    rtev(1, s2, s3, GX_CC_ZERO, GX_CC_C1, GX_CC_TEXC, GX_CC_CPREV, PREVA_);
    rtev(2, s4, s5, GX_CC_ZERO, GX_CC_ONE, GX_CC_TEXC, GX_CC_CPREV, PREVA_);
    GX_SetTevColor(GX_TEVREG0, c0); GX_SetTevColor(GX_TEVREG1, c1);
}

static void rquad(RPart *q) {
    Mtx mv, r, t;
    GX_LoadTexObj(&q->t.o, GX_TEXMAP0);
    guMtxIdentity(mv); guMtxScaleApply(mv, mv, q->w, q->h, 1);
    guMtxRotRad(r, 'z', q->a * (float)M_PI / 180.0f); guMtxConcat(r, mv, t);
    guMtxScaleApply(t, t, 0.88961464f, 0.9276675f, 1); guMtxTransApply(t, mv, q->x, q->y, 0);
    GX_LoadPosMtxImm(mv, GX_PNMTX0); GX_SetCurrentMtx(GX_PNMTX0);
    float xo = q->o == 0 ? -0.5f : q->o == 2 ? -1.0f : 0.0f; s16 s0 = q->o == 1 ? 0 : 256, s1 = 256 - s0;
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2f32(1 + xo, -0.5f); GX_Color1u32(0x000000FF); GX_TexCoord2s16(s0, 0);
    GX_Position2f32(1 + xo, 0.5f);  GX_Color1u32(0x000000FF); GX_TexCoord2s16(s0, 256);
    GX_Position2f32(xo, 0.5f);      GX_Color1u32(0x000000FF); GX_TexCoord2s16(s1, 256);
    GX_Position2f32(xo, -0.5f);     GX_Color1u32(0x000000FF); GX_TexCoord2s16(s1, 0);
    GX_End();
}

static void rdraw_parts(const RFLInfo *i, RPart *f) {
    GXColor eye0 = i->eyeT == 9 ? (GXColor){255, 130, 0, 255} : i->eyeT == 20 ? (GXColor){0, 255, 255, 255} : (GXColor){0, 0, 0, 255};
    tev1(RFL_HAIR[i->beardC & 7]); rquad(&f[5]); rquad(&f[6]);
    tev3(GX_TEV_SWAP1, GX_TEV_SWAP1, GX_TEV_SWAP3, GX_TEV_SWAP2, GX_TEV_SWAP2, GX_TEV_SWAP3, RFL_MOUTH0[i->mouthC % 3], RFL_MOUTH1[i->mouthC % 3]);
    rquad(&f[4]);
    tev1(RFL_HAIR[i->browC & 7]); rquad(&f[2]); rquad(&f[3]);
    tev3(GX_TEV_SWAP1, GX_TEV_SWAP1, GX_TEV_SWAP3, GX_TEV_SWAP3, GX_TEV_SWAP2, GX_TEV_SWAP2, eye0, RFL_EYEC1[i->eyeC % 6]);
    rquad(&f[0]); rquad(&f[1]);
    tev1(RFL_MOLEC); rquad(&f[7]);
}

// FIX: captura identica a RFLiCapture del original: DOS pasadas con blend por alpha de destino y
// alpha-test GREATER 0. El render "en un paso" con SRCALPHA/INVSRCALPHA arruinaba el alpha de la mascara
// (el alpha se sobreescribia pieza por pieza y el color se multiplicaba dos veces) y la cara quedaba sin rasgos.
static void rcapture(u8 *buf, u32 size, const RFLInfo *i, RPart *f, int res) {
    Mtx44 pr;
    (void)res;
    GX_InvalidateTexAll();

    // RFLiSetup2DCameraAndParam: ortho 608x448, 1 unidad = 1 pixel de EFB (el scissor/copia recortan a res x res)
    guOrtho(pr, 0, 448, 0, 608, 0, 1);
    GX_LoadProjectionMtx(pr, GX_ORTHOGRAPHIC);
    GX_SetViewport(0, 0, 608, 448, 0, 1);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    GX_SetZCompLoc(GX_FALSE);
    GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_OR, GX_NEVER, 0);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetDither(GX_FALSE);

    GX_ClearVtxDesc();
    GX_InvVtxCache();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 8);

    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
    GX_SetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA);
    GX_SetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN, GX_CH_ALPHA);
    GX_SetTevSwapModeTable(GX_TEV_SWAP3, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA);
    for (int k = 0; k < 4; k++) GX_SetTevDirect(k);

    // pasada 1: color
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_INVDSTALPHA, GX_BL_DSTALPHA, GX_LO_SET);
    GX_SetColorUpdate(GX_TRUE);
    rdraw_parts(i, f);

    // copia intermedia con color bloqueado: solo limpia el alpha del EFB
    GX_SetColorUpdate(GX_FALSE);
    DCInvalidateRange(buf, size);
    GX_CopyTex(buf, GX_TRUE);
    GX_PixModeSync();

    // pasada 2: acumula
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_INVDSTALPHA, GX_BL_ONE, GX_LO_SET);
    rdraw_parts(i, f);

    GX_SetColorUpdate(GX_TRUE);
    GX_DrawDone();
    DCInvalidateRange(buf, size);
    GX_CopyTex(buf, GX_TRUE);
    GX_PixModeSync();
}

static int rclamp(int v, int d, int mx) { v += d; return v < 0 ? 0 : v > mx ? mx : v; }
static int rfl_rot(const u8 *table, int count, int index, int fallback) {
    if (index < 0 || index >= count) return fallback;
    return table[index];
}

static void rset_parts(const RFLInfo *i, RPart *f, float res) {
    float k = res / 64.0f;
    float ux = 0.88961464f; float uy = 1.1600001f * 0.9276675f; float sc;
    float ex = ux * i->eyeX; float ey = 18.451525f + uy * i->eyeY;
    int eye_rot = rfl_rot(RFL_EYEROT, sizeof(RFL_EYEROT), i->eyeT, 28);
    float ea = 11.25f * ((i->eyeRot + eye_rot) % 32);
    float bx = ux * i->browX; float by = 16.549807f + uy * i->browY;
    int brow_rot = rfl_rot(RFL_BROWROT, sizeof(RFL_BROWROT), i->browT, 26);
    float ba = 11.25f * ((i->browRot + brow_rot) % 32);
    sc = 1 + 0.4f * i->eyeS; float ew = 342 / 64.0f * sc; float eh = 288 / 64.0f * sc;
    sc = 1 + 0.4f * i->browS; float bw = 324 / 64.0f * sc; float bh = 288 / 64.0f * sc;
    sc = 1 + 0.4f * i->mouthS; float mw = 396 / 64.0f * sc; float mh = 288 / 64.0f * sc; float my = 29.25885f + uy * i->mouthY;
    sc = 1 + 0.4f * i->beardS; float tw = 288 / 64.0f * sc; float th = 576 / 64.0f * sc; float ty = 31.763554f + uy * i->beardY;
    sc = 1 + 0.4f * i->moleS; float lx = 17.766165f + 2 * ux * i->moleX; float ly = 17.95986f + uy * i->moleY;

    RPart q[8] = {0};
    q[0].x = k * (32 - ex); q[0].y = k * ey; q[0].w = k * ew; q[0].h = k * eh; q[0].a = ea; q[0].o = 2;
    q[1].x = k * (32 + ex); q[1].y = k * ey; q[1].w = k * ew; q[1].h = k * eh; q[1].a = 360 - ea; q[1].o = 1;
    q[2].x = k * (32 - bx); q[2].y = k * by; q[2].w = k * bw; q[2].h = k * bh; q[2].a = ba; q[2].o = 2;
    q[3].x = k * (32 + bx); q[3].y = k * by; q[3].w = k * bw; q[3].h = k * bh; q[3].a = 360 - ba; q[3].o = 1;
    q[4].x = 32 * k; q[4].y = k * my; q[4].w = k * mw; q[4].h = k * mh;
    q[5].x = 32 * k; q[5].y = k * ty; q[5].w = k * tw; q[5].h = k * th; q[5].o = 2;
    q[6].x = 32 * k; q[6].y = k * ty; q[6].w = k * tw; q[6].h = k * th; q[6].o = 1;
    q[7].x = k * lx; q[7].y = k * ly; q[7].w = k * sc; q[7].h = k * sc;

    for (int j = 0; j < 8; j++) { q[j].t = f[j].t; f[j] = q[j]; }
}

static void rmasks(RFLModel *m, const RFLInfo *b, int res, int flags) {
    int lv[4] = {256, 128, 64, 32}, mx = 0, nl = 0, first = -1; u32 tot = 0;
    for (int l = 0; l < 4; l++) if (res & lv[l]) { if (!mx) mx = lv[l]; nl++; tot += 2 * lv[l] * lv[l]; }
    if (!mx) return;
    int mip = nl > 1; float lod = (float)(nl - 1);
    for (int e = 0; e < RFL_NEXP; e++) {
        m->mok[e] = 0; m->mbuf[e] = 0;
        if (!(flags & (1 << e))) continue;
        m->mbuf[e] = memalign(32, tot); m->mok[e] = m->mbuf[e] != 0;
        if (m->mok[e] && first < 0) first = e;
    }
    if (first < 0) return;

    RFLInfo in[RFL_NEXP]; for (int e = 0; e < RFL_NEXP; e++) in[e] = *b;
    int base_eye = rfl_rot(RFL_EYEROT, sizeof(RFL_EYEROT), b->eyeT, 28);

    #define ADJ(e, t) in[e].eyeT = t; in[e].eyeRot = rclamp(b->eyeRot, base_eye - rfl_rot(RFL_EYEROT, sizeof(RFL_EYEROT), t, 28), 7)
    ADJ(RFL_SMILE, 48); ADJ(RFL_BLINK, 26); ADJ(RFL_SURPRISE, 49);
    #undef ADJ

    in[RFL_SURPRISE].browY = (b->browY - 2) & 31;
    in[RFL_ANGER].browRot = rclamp(b->browRot, 2, 11); in[RFL_ANGER].eyeRot = rclamp(b->eyeRot, 2, 7); in[RFL_ANGER].mouthT = 10;
    in[RFL_SORROW].browRot = rclamp(b->browRot, -2, 11); in[RFL_SORROW].eyeRot = rclamp(b->eyeRot, -2, 7); in[RFL_SORROW].mouthT = 12;
    in[RFL_OPENMOUTH].mouthT = 24;

    GX_SetFog(GX_FOG_NONE, 1, 1, 0, 0, (GXColor){0, 0, 0, 0});
    GX_SetColorUpdate(GX_TRUE); GX_SetAlphaUpdate(GX_TRUE); GX_SetDstAlpha(GX_FALSE, 0);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE); GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
    GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL); GX_SetCopyClamp(GX_CLAMP_TOP | GX_CLAMP_BOTTOM);
    GX_SetTexCopySrc(0, 0, mx, mx); GX_SetTexCopyDst(mx, mx, GX_TF_RGB5A3, GX_FALSE);
    GX_SetCopyClear((GXColor){0, 0, 0, 0}, 0xFFFFFF);
    DCInvalidateRange(m->mbuf[first], tot); GX_CopyTex(m->mbuf[first], GX_TRUE); GX_PixModeSync();

    for (int e = 0; e < RFL_NEXP; e++) {
        if (!m->mok[e]) continue;
        const RFLInfo *i = &in[e];
        RPart f[8]; memset(f, 0, sizeof f);
        rtex(&f[0].t, A_EYE, i->eyeT, 1); rtex(&f[2].t, A_BROW, i->browT, 1); rtex(&f[4].t, A_MOUTH, i->mouthT, 1);
        rtex(&f[5].t, A_MUST, i->mustT, 1); rtex(&f[7].t, A_MOLE, i->moleT, 1);
        f[1].t = f[0].t; f[3].t = f[2].t; f[6].t = f[5].t;
        u8 *p = (u8 *)m->mbuf[e];
        for (int l = 0; l < 4; l++) {
            if (!(res & lv[l])) continue;
            GX_SetTexCopySrc(0, 0, lv[l], lv[l]); GX_SetTexCopyDst(lv[l], lv[l], GX_TF_RGB5A3, GX_FALSE);
            GX_SetScissor(0, 0, lv[l], lv[l]);
            rset_parts(i, f, (float)lv[l]);
            rcapture(p, 2 * lv[l] * lv[l], i, f, lv[l]);
            p += 2 * lv[l] * lv[l];
        }
        rtex_free(&f[0].t); rtex_free(&f[2].t); rtex_free(&f[4].t); rtex_free(&f[5].t); rtex_free(&f[7].t);
        GX_InitTexObj(&m->mtex[e], m->mbuf[e], mx, mx, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, mip);
        if (mip) GX_InitTexObjLOD(&m->mtex[e], GX_LIN_MIP_LIN, GX_LINEAR, 0, lod, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
        else GX_InitTexObjLOD(&m->mtex[e], GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    }
    GX_DrawDone();
    GX_SetViewport(0, 0, rfl_rm->fbWidth, rfl_rm->efbHeight, 0, 1);
    GX_SetScissor(0, 0, rfl_rm->fbWidth, rfl_rm->efbHeight);
    GX_SetPixelFmt(rfl_rm->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetCopyFilter(rfl_rm->aa, rfl_rm->sample_pattern, GX_TRUE, rfl_rm->vfilter);
    GX_InvalidateTexAll();
}

// ---------- modelo ----------
static void RFL_Free(RFLModel *m) {
    RShp *s[] = {&m->face, &m->cap, &m->hair, &m->fore, &m->beard, &m->nose, &m->nline, &m->mask, &m->glass};
    for (int i = 0; i < 9; i++) rshp_free(s[i]);
    rtex_free(&m->tFace); rtex_free(&m->tCap); rtex_free(&m->tNline); rtex_free(&m->tGlass);
    for (int e = 0; e < RFL_NEXP; e++) free(m->mbuf[e]);
    memset(m, 0, sizeof *m);
}

// OJO: RFL_Make dibuja en el EFB (para generar la mascara). Llamalo ANTES de empezar a dibujar el frame
// (o justo despues de copiar el frame al XFB), nunca en medio de la escena.
static int RFL_Make(RFLModel *m, const RFLInfo *i, int res, int flags) {
    float h[9] = {0}, ht[3], bt[3], nt[3], gt[3];
    memset(m, 0, sizeof *m);

    rvat3d();

    rshp(&m->face, A_FACE, i->faceT, 1, 0, 0, 1, 0, h);
    if (!m->face.dls) return -1;

    memcpy(bt, h + 3, 12);
    memcpy(ht, h + 6, 12);
    m->flip = i->hairFlip;

    rshp(&m->cap, A_CAP, i->hairT, 1, m->flip, 1, 1, ht, 0);
    rshp(&m->hair, A_HAIR, i->hairT, 0, m->flip, 1, 1, ht, 0);
    rshp(&m->fore, A_FORE, i->hairT, 0, m->flip, 1, 1, ht, 0);
    rshp(&m->beard, A_BEARD, i->beardT, 0, 0, 1, 1, bt, 0);

    float ns = 0.4f + 0.175f * i->noseS;
    nt[0] = h[0]; nt[1] = h[1] - 1.5f * (i->noseY - 8); nt[2] = h[2];

    rshp(&m->nose, A_NOSE, i->noseT, 0, 0, 1, ns, nt, 0);
    rshp(&m->nline, A_NLINE, i->noseT, 1, 0, 1, ns, nt, 0);
    rshp(&m->mask, A_MASK, i->faceT, 1, 0, 0, 1, 0, 0);

    gt[0] = h[0]; gt[1] = 5.0f + h[1] - 1.5f * (i->glassY - 11); gt[2] = 2.0f + h[2];
    // FIX: la forma de los lentes siempre es el indice 0 (arg.index = 0 en el original); el tipo solo elige la textura.
    rshp(&m->glass, A_GLASS, 0, 1, 0, 1, 0.15f * i->glassS + 0.4f, gt, 0);

    if (rtex(&m->tFace, A_FACETEX, i->faceTex, 0) != 0) { RFL_Free(m); return -2; }
    if (m->cap.dls) rtex(&m->tCap, A_CAPTEX, i->hairT, 0);
    if (m->nline.dls) rtex(&m->tNline, A_NLINETEX, i->noseT, 0);
    rtex(&m->tGlass, A_GLASSTEX, i->glassT, 0);

    m->cFace = i->faceC < 6 ? i->faceC : 0; m->cHair = i->hairC & 7;
    m->cBeard = i->beardC & 7; m->cGlass = i->glassC < 6 ? i->glassC : 0; m->cFav = i->favC % 12;

    {
        float mn[3] = {1e9f, 1e9f, 1e9f};
        float mx[3] = {-1e9f, -1e9f, -1e9f};
        for (u32 k = 0; k < m->face.nv; k++) {
            for (int j = 0; j < 3; j++) {
                float x = m->face.p[3 * k + j] / 256.0f;
                if (x < mn[j]) mn[j] = x;
                if (x > mx[j]) mx[j] = x;
            }
        }
        for (int j = 0; j < 3; j++) m->ctr[j] = (mn[j] + mx[j]) * 0.5f;
        m->hgt = mx[1] - mn[1] > 1e-3f ? mx[1] - mn[1] : 1;
    }

    if (flags) {
        rmasks(m, i, res, flags);
        for (int e = 0; e < RFL_NEXP; e++) { if (m->mok[e]) { m->exp = e; break; } }
    }
    return 0;
}

static int RFL_SetExp(RFLModel *m, int e) {
    if (e < 0 || e >= RFL_NEXP || !m->mok[e]) return -1;
    m->exp = e; return 0;
}

// ---------- dibujo ----------
static void rarr(const RShp *s, int uv) {
    GX_SetArray(GX_VA_POS, s->p, 6);
    GX_SetArray(GX_VA_NRM, s->n, 6);
    if (uv) GX_SetArray(GX_VA_TEX0, s->t, 4);
    GX_CallDispList(s->dl, s->dls);
}

#define KC(c) GX_SetTevKColor(GX_KCOLOR0, c)

static void RFL_Draw(const RFLModel *m, Mtx pos, Mtx nrm, int lit, GXColor amb) {
    if (!m || !m->face.dls || !m->tFace.img || !m->face.p || !m->face.n) return;

    GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_OR, GX_NEVER, 0);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_COPY);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetZCompLoc(GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetDither(GX_FALSE);
    GX_SetDstAlpha(GX_FALSE, 0);

    GX_SetCullMode(RFL_CULL);

    if (lit) {
        GX_SetTevDirect(GX_TEVSTAGE1);
        GX_SetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0);
        GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_RASC, GX_CC_ZERO);
        GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
        GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    }

    GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
    GX_SetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_ALPHA, GX_CH_BLUE, GX_CH_GREEN);
    GX_SetNumTevStages(lit ? 2 : 1);
    GX_SetTevDirect(GX_TEVSTAGE0);
    GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
    GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);   // alpha constante = 1.0 para las piezas solidas

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_NRM, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);

    rvat3d();   // FIX: TEX0 con 13 bits fraccionales
    GX_SetNumTexGens(1);

    if (lit) {
        GX_SetNumChans(1);
        GX_SetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0, GX_DF_CLAMP, GX_AF_NONE);
        GX_SetChanCtrl(GX_ALPHA0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
        GX_SetChanAmbColor(GX_COLOR0, amb);
        GX_SetChanMatColor(GX_COLOR0, (GXColor){255, 255, 255, 255});
    } else {
        GX_SetNumChans(0);
    }

    // --- OPACO ---
    GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
    GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

    GX_LoadPosMtxImm(pos, GX_PNMTX0);
    GX_LoadNrmMtxImm(nrm, GX_PNMTX0);
    GX_SetCurrentMtx(GX_PNMTX0);

    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY);
    GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
    GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);

    if (m->beard.dls) { KC(RFL_HAIR[m->cBeard]); rarr(&m->beard, 0); }
    KC(RFL_FACE[m->cFace]);
    if (m->nose.dls) rarr(&m->nose, 0);
    if (m->fore.dls) rarr(&m->fore, 0);
    if (m->hair.dls) { KC(RFL_HAIR[m->cHair]); rarr(&m->hair, 0); }

    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);

    if (m->cap.dls && m->tCap.img) {
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_KONST, GX_CC_TEXC, GX_CC_KONST);
        GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
        KC(RFL_FAV[m->cFav]);
        GX_LoadTexObj((GXTexObj *)&m->tCap.o, GX_TEXMAP0); rarr(&m->cap, 1);
        GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    }

    GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_KONST, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
    KC(RFL_FACE[m->cFace]);
    GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP1);

    GX_LoadTexObj((GXTexObj *)&m->tFace.o, GX_TEXMAP0);
    rarr(&m->face, 1);

    // --- TRANSLUCIDO: mascara facial (ojos, cejas, boca), linea de nariz y lentes ---
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
    GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);

    if (m->exp >= 0 && m->exp < RFL_NEXP && m->mok[m->exp] && m->mask.dls) {
        GX_LoadTexObj((GXTexObj *)&m->mtex[m->exp], GX_TEXMAP0);
        rarr(&m->mask, 1);
    }
    if (m->nline.dls && m->tNline.img) {
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
        GX_LoadTexObj((GXTexObj *)&m->tNline.o, GX_TEXMAP0);
        rarr(&m->nline, 1);
    }
    if (m->glass.dls && m->tGlass.img) {
        KC(RFL_GLASS[m->cGlass]);
        GX_SetCullMode(GX_CULL_NONE);   // como el original: lentes a doble cara
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
        GX_LoadTexObj((GXTexObj *)&m->tGlass.o, GX_TEXMAP0);
        rarr(&m->glass, 1);
    }
}
#endif
