# 🎮 FrevoBall

**Um jogo 1x1 inspirado em Head Soccer com sprites, física 2D, ranking e IA dinâmica via Gemini.**  
Trabalho prático desenvolvido em C com Raylib.

---

## ✅ Requisitos para execução

Você deve estar no ambiente **MSYS2 MinGW UCRT64**.  
Se ainda não tem o MSYS2, instale aqui: [https://www.msys2.org](https://www.msys2.org)

### Pacotes obrigatórios:

Abra o terminal `MSYS2 UCRT64` e execute:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc             # Compilador C
pacman -S mingw-w64-ucrt-x86_64-raylib          # Biblioteca gráfica Raylib
pacman -S mingw-w64-ucrt-x86_64-curl            # libcurl (acesso à internet)
```

## Como compilar o jogo
Entre na pasta do projeto com o terminal MSYS2 UCRT64 e use um dos comandos abaixo:

Compilar com Gemini: gcc main.c -o FrevoBall.exe -lraylib -lcurl -lopengl32 -lgdi32 -lwinmm

## 🕹Controles do jogo
 # Tecla	Ação<br>
      A / ←	Andar para a esquerda<br>
      D / →	Andar para a direita<br>
      W / ↑	Pular<br>
      ENTER	Confirmar (menu / ranking)<br>
      ESC	Voltar ao menu / sair

## 🧼Como limpar o ranking
Para remover os dados salvos: del ranking.dat  # Windows

## 🎥 Vídeo de demonstração

Veja o jogo funcionando no vídeo abaixo:
🔗 [Assista aqui](https://drive.google.com/file/d/1c4_SmHCNGUYUVUIZcjgGTbnDz_QDsLZH/view?usp=drive_link)
