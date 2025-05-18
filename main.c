/******************************************************************
 * FrevoBall — sprites, fundo Recife, texto legível, IA Gemini opc.
 *   • COM Gemini:
 *       gcc main.c -o FrevoBall.exe -lraylib -lcurl -lopengl32 -lgdi32 -lwinmm
 *   • SEM Gemini:
 *       gcc main.c -o FrevoBall.exe -DNO_GEMINI -lraylib -lopengl32 -lgdi32 -lwinmm
 ******************************************************************/

/* ── Evita conflitos de nomes WinAPI × Raylib ────────────────── */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define CloseWindow Win32CloseWindow
#define ShowCursor  Win32ShowCursor
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef LoadImage
#undef DrawText
#undef DrawTextEx
/* ─────────────────────────────────────────────────────────────── */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "raylib.h"

#ifndef NO_GEMINI
    #include <curl/curl.h>
    #define CHAVE_GEMINI "AIzaSyAXsmkzGzWSCw9DKUJ8SEPYo1-t3aTDV2o"
#endif

/* ───── CONFIGURAÇÕES GERAIS ───── */
#define LARG_JAN     960
#define ALT_JAN      540
#define CHAO_Y       460

#define LARG_JOG     64
#define ALT_JOG      64
#define VEL_JOG      300.0f
#define VEL_PULO     600.0f

#define GRAVIDADE    1200.0f
#define RAIO_BOLA    20
#define RESTITUIÇÃO  0.7f

#define GOL_LARG     14
#define GOL_TOPO     (CHAO_Y - 160)

#define GOLS_VITÓRIA 5
#define TEMPO_PART   60  /* segundos */

/* ───── TEXTURAS ───── */
static Texture2D texFundo, texJog, texIA, texBola;
static bool fundoOK=false, jogOK=false, iaOK=false, bolaOK=false;

/* ───── ESTRUTURAS ───── */
typedef enum { TELAMENU, TELAJOGO, TELARANK, TELASAIR } Tela;
typedef struct { Rectangle caixa; Vector2 vel; int pontos; } Jogador;
typedef struct { Vector2 pos, vel; float rot; } Bola;
typedef struct NoRank {
    char nome[24]; int gf, ga; time_t ts; struct NoRank *prox;
} NoRank;

/* ───── VARIÁVEIS GLOBAIS ───── */
static Jogador jogador, ia;
static Bola    bola;
static int     tempoRest;
static float   acum;          /* acumula DeltaTime para cronômetro */
static bool    fimPartida;
static NoRank *ranking = NULL;
static float   velBaseIA = 260.0f;

/* ───── FUNÇÕES AUXILIARES ───── */
static inline float limitar(float v,float mn,float mx){ return v<mn?mn:(v>mx?mx:v); }

static bool ColideCirculoRet(Rectangle rec, Vector2 c, float r){
    float cx = limitar(c.x, rec.x, rec.x + rec.width);
    float cy = limitar(c.y, rec.y, rec.y + rec.height);
    float dx = c.x - cx, dy = c.y - cy;
    return dx*dx + dy*dy < r*r;
}

static void TextoSombra(const char*txt,int x,int y,int fs,Color cor){
    DrawText(txt,x+1,y+1,fs,BLACK);
    DrawText(txt,x  ,y  ,fs,cor);
}

/* ───── FUNÇÕES DE RANKING ───── */
static NoRank* NovoNo(const char*n,int gf,int ga){
    NoRank*r=malloc(sizeof(*r)); strncpy(r->nome,n,23); r->nome[23]='\0';
    r->gf=gf; r->ga=ga; r->ts=time(NULL); r->prox=NULL; return r;}

static void InserirPartida(const char*n,int gf,int ga){
    NoRank*r=NovoNo(n,gf,ga); r->prox=ranking; ranking=r;
}

static void OrdenarRanking(void){
    NoRank*ord=NULL;
    while(ranking){
        NoRank*c=ranking; ranking=c->prox;
        if(!ord||(c->gf-c->ga)>(ord->gf-ord->ga)){ c->prox=ord; ord=c; }
        else{ NoRank*p=ord;
              while(p->prox&&(p->prox->gf-p->prox->ga)>=(c->gf-c->ga)) p=p->prox;
              c->prox=p->prox; p->prox=c; }
    }
    ranking=ord;
}

static void SalvarRanking(const char*arq){
    FILE*f=fopen(arq,"wb"); if(!f)return;
    for(NoRank*n=ranking;n;n=n->prox) fwrite(n,sizeof(*n),1,f);
    fclose(f);
}
static void CarregarRanking(const char*arq){
    FILE*f=fopen(arq,"rb"); if(!f)return;
    NoRank tmp,*t=NULL;
    while(fread(&tmp,sizeof tmp,1,f)==1){
        NoRank*n=malloc(sizeof tmp); *n=tmp; n->prox=NULL;
        if(!ranking) ranking=t=n; else{ t->prox=n; t=n; }}
    fclose(f); OrdenarRanking();
}
static void LimparRanking(void){
    while(ranking){ NoRank*t=ranking->prox; free(ranking); ranking=t; }
}

/* ───── IA GEMINI (opcional) ───── */
#ifndef NO_GEMINI
typedef struct{char*buf;size_t sz;}Mem;
static size_t cbWrite(void*ct,size_t s,size_t n,void*u){
    size_t r=s*n; Mem*m=u;
    char*p=realloc(m->buf,m->sz+r+1); if(!p)return 0;
    m->buf=p; memcpy(p+m->sz,ct,r); m->sz+=r; p[m->sz]=0; return r;}
static void AjustarIA_Gemini(int pj,int pi){
    char url[512];
    snprintf(url,sizeof url,
      "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash-latest:generateContent?key=%s",
      CHAVE_GEMINI);
    char prompt[256];
    snprintf(prompt,sizeof prompt,
      "Placar %d x %d. Apenas um número decimal (velocidade IA).",pj,pi);
    char payload[512];
    snprintf(payload,sizeof payload,
      "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}]}",prompt);

    Mem m={malloc(1),0};
    CURL*cur=curl_easy_init(); struct curl_slist*h=NULL;
    h=curl_slist_append(h,"Content-Type: application/json");
    curl_easy_setopt(cur,CURLOPT_URL,url);
    curl_easy_setopt(cur,CURLOPT_POSTFIELDS,payload);
    curl_easy_setopt(cur,CURLOPT_HTTPHEADER,h);
    curl_easy_setopt(cur,CURLOPT_WRITEFUNCTION,cbWrite);
    curl_easy_setopt(cur,CURLOPT_WRITEDATA,&m);
    curl_easy_setopt(cur,CURLOPT_USERAGENT,"frevoball");
    curl_easy_perform(cur); curl_easy_cleanup(cur); curl_slist_free_all(h);

    char*p=m.buf; double v=velBaseIA;
    if(p){ while(*p&&(*p<'0'||*p>'9')&&*p!='-')p++; if(*p) v=strtod(p,NULL);}
    if(v>100&&v<1000) velBaseIA=(float)v;
    free(m.buf);
}
#else
    #define AjustarIA_Gemini(a,b) ((void)0)
#endif

/* ───── CONTROLES DA PARTIDA ───── */
static void ReiniciarPartida(void)
{
    jogador.caixa = (Rectangle){ 80, CHAO_Y - ALT_JOG, LARG_JOG, ALT_JOG };
    jogador.vel   = (Vector2){ 0, 0 };
    jogador.pontos = 0;

    ia.caixa = (Rectangle){ LARG_JAN - 80 - LARG_JOG, CHAO_Y - ALT_JOG,
                            LARG_JOG, ALT_JOG };
    ia.vel   = (Vector2){ 0, 0 };
    ia.pontos = 0;

    bola.pos = (Vector2){ LARG_JAN / 2.0f, ALT_JAN / 4.0f };
    bola.vel = (Vector2){ GetRandomValue(-200, 200), -50 };
    bola.rot = 0;

    tempoRest = TEMPO_PART;
    acum      = 0.0f;
    fimPartida = false;
}

static void MarcarGol(int jogadorMarca) /* 1 = jogador, 0 = IA */
{
    if (jogadorMarca) jogador.pontos++;
    else               ia.pontos++;

    /* reposiciona bola */
    bola.pos = (Vector2){ LARG_JAN / 2.0f, 0 };
    bola.vel = (Vector2){ GetRandomValue(-200, 200), 0 };
    bola.rot = 0;
}

/* movimenta + gravidade */
static void AtualizarJogador(Jogador *j, float dt)
{
    j->caixa.x += j->vel.x * dt;
    j->caixa.y += j->vel.y * dt;
    j->vel.y   += GRAVIDADE * dt;

    if (j->caixa.y > CHAO_Y - ALT_JOG) {
        j->caixa.y = CHAO_Y - ALT_JOG;
        j->vel.y = 0;
    }
    j->caixa.x = limitar(j->caixa.x, 0, LARG_JAN - LARG_JOG);
}

static void AtualizarJogo(float dt, Tela *tela)
{
    if (fimPartida) {
        if (IsKeyPressed(KEY_ENTER)) *tela = TELARANK;
        return;
    }

    /* ----- cronômetro ----- */
    acum += dt;
    if (acum >= 1.0f) { acum -= 1.0f; tempoRest--; }
    if (tempoRest <= 0 || jogador.pontos >= GOLS_VITÓRIA || ia.pontos >= GOLS_VITÓRIA) {
        fimPartida = true;
        InserirPartida("Player", jogador.pontos, ia.pontos);
        OrdenarRanking();
        SalvarRanking("ranking.dat");
        AjustarIA_Gemini(jogador.pontos, ia.pontos);
    }

    /* ----- controle jogador ----- */
    jogador.vel.x = 0;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  jogador.vel.x = -VEL_JOG;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) jogador.vel.x =  VEL_JOG;
    if ((IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) && jogador.vel.y == 0)
        jogador.vel.y = -VEL_PULO;
    AtualizarJogador(&jogador, dt);

    /* ----- IA básica ----- */
    float alvoX = bola.pos.x;
    float dir = (alvoX > ia.caixa.x + LARG_JOG/2) ? 1 : -1;
    if (fabsf(alvoX - (ia.caixa.x + LARG_JOG/2)) < 10) dir = 0;
    ia.vel.x = dir * velBaseIA;
    if (bola.pos.y < ia.caixa.y && ia.vel.y == 0) ia.vel.y = -VEL_PULO * 0.9f;
    AtualizarJogador(&ia, dt);

    /* ----- física da bola ----- */
    bola.pos.x += bola.vel.x * dt;
    bola.pos.y += bola.vel.y * dt;
    bola.vel.y += GRAVIDADE * dt;
    bola.rot   += 180.0f * dt;

    /* quique chão/paredes */
    if (bola.pos.y > CHAO_Y - RAIO_BOLA) { bola.pos.y = CHAO_Y - RAIO_BOLA; bola.vel.y *= -RESTITUIÇÃO; }
    if (bola.pos.x < RAIO_BOLA) { bola.pos.x = RAIO_BOLA; bola.vel.x *= -RESTITUIÇÃO; }
    if (bola.pos.x > LARG_JAN - RAIO_BOLA) { bola.pos.x = LARG_JAN - RAIO_BOLA; bola.vel.x *= -RESTITUIÇÃO; }

    /* ----- colisão simples bola × jogadores ----- */
    if (ColideCirculoRet(jogador.caixa, bola.pos, RAIO_BOLA)) {
        bola.vel.x = (bola.pos.x - (jogador.caixa.x + LARG_JOG/2)) * 8;
        bola.vel.y = -300;
    }
    if (ColideCirculoRet(ia.caixa, bola.pos, RAIO_BOLA)) {
        bola.vel.x = (bola.pos.x - (ia.caixa.x + LARG_JOG/2)) * 8;
        bola.vel.y = -300;
    }

    /* ----- gols ----- */
    Rectangle golDir = { LARG_JAN - GOL_LARG, GOL_TOPO, GOL_LARG, CHAO_Y - GOL_TOPO };
    Rectangle golEsq = { 0, GOL_TOPO, GOL_LARG, CHAO_Y - GOL_TOPO };
    if (CheckCollisionCircleRec(bola.pos, RAIO_BOLA, golDir)) MarcarGol(1);
    if (CheckCollisionCircleRec(bola.pos, RAIO_BOLA, golEsq)) MarcarGol(0);

    /* ESC volta ao menu */
    if (IsKeyPressed(KEY_ESCAPE)) *tela = TELAMENU;
}

/* ───── ROTINAS DE DESENHO ───── */
static void DesenharFundo(void)
{
    if (!fundoOK) return;
    Rectangle dst = { 0, 0, LARG_JAN, ALT_JAN };
    Rectangle src = { 0, 0, texFundo.width, texFundo.height };
    DrawTexturePro(texFundo, src, dst, (Vector2){0,0}, 0, WHITE);
}

static void DesenharSprites(void)
{
    if (jogOK) DrawTexture(texJog, jogador.caixa.x, jogador.caixa.y, WHITE);
    else       DrawRectangleRec(jogador.caixa, BLUE);

    if (iaOK)  DrawTexture(texIA, ia.caixa.x, ia.caixa.y, WHITE);
    else       DrawRectangleRec(ia.caixa, RED);

    if (bolaOK) {
        Rectangle src  = { 0, 0, texBola.width, texBola.height };
        Rectangle dst  = { bola.pos.x - RAIO_BOLA, bola.pos.y - RAIO_BOLA,
                           RAIO_BOLA*2, RAIO_BOLA*2 };
        DrawTexturePro(texBola, src, dst,
                       (Vector2){RAIO_BOLA, RAIO_BOLA}, bola.rot, WHITE);
    } else {
        DrawCircleV(bola.pos, RAIO_BOLA, RED);
    }
}

static int indiceMenu = 0;

static void DesenharMenu(void)
{
    DesenharFundo();
    DrawRectangle(0, 80, LARG_JAN, 120, ColorAlpha(BLACK, 0.35f));
    TextoSombra("FrevoBall", LARG_JAN/2 - MeasureText("FrevoBall", 64)/2, 100, 64, BLUE);

    const char *itens[] = { "Jogar", "Ranking", "Sair" };
    for (int i = 0; i < 3; ++i) {
        Color cor = (i == indiceMenu) ? MAROON : BLACK;
        TextoSombra(itens[i], LARG_JAN/2 - MeasureText(itens[i], 32)/2, 240 + i * 50, 32, cor);
    }
    TextoSombra("W/S ou ↑/↓ • ENTER seleciona",
                LARG_JAN/2 - MeasureText("W/S ou ↑/↓ • ENTER seleciona", 18)/2,
                ALT_JAN - 40, 18, GRAY);
}

static void DesenharJogo(void)
{
    DesenharFundo();
    DrawRectangle(0, CHAO_Y, LARG_JAN, ALT_JAN - CHAO_Y, GREEN);
    DrawRectangle(0, GOL_TOPO, GOL_LARG, CHAO_Y - GOL_TOPO, WHITE);
    DrawRectangle(LARG_JAN - GOL_LARG, GOL_TOPO, GOL_LARG, CHAO_Y - GOL_TOPO, WHITE);

    DesenharSprites();

    DrawRectangle(LARG_JAN/2 - 120, 24, 240, 64, ColorAlpha(BLACK, 0.35f));
    TextoSombra(TextFormat("%d", jogador.pontos), LARG_JAN/2 - 60, 30, 40, SKYBLUE);
    TextoSombra("x", LARG_JAN/2 - 6, 30, 40, WHITE);
    TextoSombra(TextFormat("%d", ia.pontos), LARG_JAN/2 + 30, 30, 40, ORANGE);

    DrawRectangle(LARG_JAN/2 - 90, 72, 180, 30, ColorAlpha(BLACK, 0.35f));
    TextoSombra(TextFormat("Tempo: %02d", tempoRest),
                LARG_JAN/2 - MeasureText("Tempo: 00", 20)/2, 78, 20, WHITE);

    if (fimPartida) {
        const char *msg = (jogador.pontos > ia.pontos) ? "VITORIA!" : "DERROTA!";
        TextoSombra(msg, LARG_JAN/2 - MeasureText(msg, 48)/2, 160, 48, WHITE);
        TextoSombra("ENTER para Ranking",
                    LARG_JAN/2 - MeasureText("ENTER para Ranking", 22)/2, 230, 22, LIGHTGRAY);
    }
}

static void DesenharRanking(void)
{
    DesenharFundo();
    DrawRectangle(0, 40, LARG_JAN, ALT_JAN - 80, ColorAlpha(BLACK, 0.4f));
    TextoSombra("RANKING", LARG_JAN/2 - MeasureText("RANKING", 48)/2, 60, 48, YELLOW);

    int y = 140, pos = 1;
    for (NoRank *n = ranking; n && y < ALT_JAN - 80; n = n->prox, pos++, y += 32) {
        char d[32]; strftime(d, sizeof d, "%d/%m %H:%M", localtime(&n->ts));
        TextoSombra(TextFormat("%02d. %-8s  %d x %d  %s",
                               pos, n->nome, n->gf, n->ga, d), 140, y, 24, WHITE);
    }
    TextoSombra("ESC volta", LARG_JAN/2 - MeasureText("ESC volta", 20)/2,
                ALT_JAN - 50, 20, GRAY);
}

/* ─────── FUNÇÃO PRINCIPAL ─────── */
int main(void)
{
    InitWindow(LARG_JAN, ALT_JAN, "FrevoBall");
    SetTargetFPS(60);

    texFundo = LoadTexture("assets/recife_bg.jpeg");   fundoOK = texFundo.id != 0;
    texJog   = LoadTexture("assets/player1.png");      jogOK   = texJog.id   != 0;
    texIA    = LoadTexture("assets/player2.png");      iaOK    = texIA.id    != 0;
    texBola  = LoadTexture("assets/bola.png");         bolaOK  = texBola.id != 0;

    CarregarRanking("ranking.dat");
    ReiniciarPartida();

    Tela tela = TELAMENU;

    while (!WindowShouldClose() && tela != TELASAIR)
    {
        float dt = GetFrameTime();

        /* --- atualização --- */
        if (tela == TELAMENU) {
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) indiceMenu = (indiceMenu + 2) % 3;
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) indiceMenu = (indiceMenu + 1) % 3;
            if (IsKeyPressed(KEY_ENTER)) {
                if (indiceMenu == 0) { ReiniciarPartida(); tela = TELAJOGO; }
                else if (indiceMenu == 1) tela = TELARANK;
                else tela = TELASAIR;
            }
        }
        else if (tela == TELAJOGO) {
            AtualizarJogo(dt, &tela);
        }
        else if (tela == TELARANK) {
            if (IsKeyPressed(KEY_ESCAPE)) tela = TELAMENU;
        }

        /* --- desenho --- */
        BeginDrawing();
            if      (tela == TELAMENU) DesenharMenu();
            else if (tela == TELAJOGO) DesenharJogo();
            else if (tela == TELARANK) DesenharRanking();
        EndDrawing();
    }

    SalvarRanking("ranking.dat");
    LimparRanking();

    if (fundoOK) UnloadTexture(texFundo);
    if (jogOK)   UnloadTexture(texJog);
    if (iaOK)    UnloadTexture(texIA);
    if (bolaOK)  UnloadTexture(texBola);

    CloseWindow();
    return 0;
}
