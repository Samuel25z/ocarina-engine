// (ocarina) The Insert menu in Brazilian Portuguese: the menu's own texts stay in English in vk_menu.cpp (the rows' names are
// also how the code finds them and how hookshot_options.ini names their keys), and are shown through translate(), a table
// from the English text to the Portuguese one. A text the table has whole is that one; a text built from parts (a hint made
// of two sentences, "Quality 67% (2560x1440)", a size and "(native)") is translated part by part: each part a text of the
// table, or a number, a size, a sign, a word in capitals (DLSS, NVIDIA); anything else (a GPU's or a microphone's name, a
// text already in Portuguese) is shown as it is. vk_menu.cpp menu_pt says when (the Language row); tests/test_menu_pt.py
// checks that every row, value, hint and prompt has its Portuguese and measures them.
#pragma once
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

namespace mkwvk::menu_lang {

struct Pair {
    const char* en;
    const char* pt;
};

// clang-format off
inline constexpr Pair kPairs[] = {
    // ---- the tabs, the pages' titles ----
    {"Graphics", "Gráficos"}, {"Audio", "Áudio"}, {"Gameplay", "Jogo"}, {"Controls", "Controles"}, {"Debug", "Depuração"},
    {"Inventory", "Inventário"}, {"Teleport", "Teletransporte"},

    // ---- the prompts ----
    {"Choose", "Escolher"}, {"Change", "Mudar"}, {"Tabs", "Trocar aba"}, {"Close", "Fechar"}, {"Confirm", "Confirmar"},
    {"Open", "Abrir"}, {"Back", "Voltar"}, {"Column", "Coluna"}, {"Bind", "Definir"}, {"Clear", "Limpar"},
    {"Cancel", "Cancelar"}, {"Move", "Mover"}, {"Amount", "Quantidade"}, {"Section", "Seção"}, {"Done", "Pronto"},
    {"Travel", "Viajar"}, {"Later", "Depois"}, {"Toggle", "Alternar"}, {"Apply", "Aplicar"}, {"Set", "Definir"},
    {"Open list", "Abrir lista"}, {"Edit amount", "Mudar quantidade"}, {"Relock", "Bloquear"}, {"Unlock", "Liberar"},

    // ---- values ----
    {"Off", "Desligado"}, {"On", "Ligado"}, {"Auto", "Automático"}, {"Automatic", "Automático"}, {"Native", "Nativo"}, {"Compute", "Compute"},
    {"Custom", "Personalizado"}, {"Default", "Padrão"}, {"None", "Nenhum"},
    {"Minimum (4GB)", "Mínimo (4GB)"}, {"Low (6GB)", "Baixo (6GB)"}, {"Balanced (8GB)", "Equilibrado (8GB)"},
    {"High (12GB)", "Alto (12GB)"}, {"Ultra (16GB+)", "Ultra (16GB+)"},
    {"Handheld", "Portátil"}, {"Docked", "Na TV"},
    {"Insert or F1", "Insert ou F1"}, {"F1 only", "Só F1"}, {"Insert, F1 or Esc", "Insert, F1 ou Esc"},
    {"Expanded", "Expandido"}, {"Native (the game's)", "Nativo (o do jogo)"},
    {"60 (the console's)", "60 (o do console)"},
    {"On (gyro found)", "Ligado (com giroscópio)"}, {"Off (gyro found)", "Desligado (com giroscópio)"},
    {"On (no gyro on this pad)", "Ligado (sem giroscópio)"}, {"Off (no gyro on this pad)", "Desligado (sem giroscópio)"},
    {"On (no controller)", "Ligado (sem controle)"}, {"Off (no controller)", "Desligado (sem controle)"},
    {"As the stick", "Como o analógico"},
    {"Not needed (window = output)", "Não precisa (janela = saída)"},
    {"On + Boost", "Ligado + Boost"},
    {"Game's own", "O do jogo"}, {"AMD FSR 4 Native AA", "AMD FSR 4 AA nativo"}, {"NVIDIA default", "Padrão da NVIDIA"},
    {"NVIDIA default (RX_VK_DLSS_PRESET)", "Padrão NVIDIA (RX_VK_DLSS_PRESET)"},
    {"Auto (dynamic)", "Automática (dinâmica)"},
    {"Quality", "Qualidade"}, {"Balanced", "Equilibrado"}, {"Performance", "Desempenho"},
    {"Ultra Performance", "Ultra Desempenho"},
    {"Game's", "Do jogo"}, {"Real time", "Tempo real"}, {"Per frame", "Por quadro"}, {"Normal", "Normal"},
    {"Instant", "Instantâneo"}, {"Original", "Original"}, {"Monitor's shape", "Formato do monitor"},
    {"Device", "Dispositivo"}, {"Microphone", "Microfone"},
    {"Small", "Pequeno"}, {"Medium", "Médio"}, {"Large", "Grande"}, {"Default (24%)", "Padrão (24%)"},
    {"Stereo", "Estéreo"}, {"Low", "Baixa"}, {"Standard", "Padrão"}, {"High", "Alta"},
    {"Full width", "Tela inteira"}, {"Centred, blurred sides", "No meio, lados borrados"},
    {"Centred, black bars", "No meio, faixas pretas"},
    {"Nintendo (Def)", "Nintendo (padrão)"}, {"Swap Face", "A e B trocados"},
    {"Light", "Leve"}, {"Strong", "Forte"},
    {"nits", "nits"}, {"Kid", "Criança"}, {"Adult", "Adulto"},
    {"Press again to confirm", "Aperte de novo para confirmar"},

    // ---- Graphics ----
    {"Quality preset", "Qualidade (pré-ajuste)"},
    {"Sets the scaling rows and memory limits for a GPU with that much memory. The Upscaler and Frame generation stay yours.",
     "Ajusta a resolução, a escala e o uso de memória para uma placa de vídeo com essa memória. O Upscaler e a Geração de "
     "quadros continuam como você deixou."},
    {"Each row as you set it. Changing most rows yourself comes back to Custom.",
     "Cada opção como você deixou. Mudar a maioria das opções volta para Personalizado."},
    {"Resolution 50% (with DLSS its nearest mode), Lanczos, Sharpness 30%, the Handheld Profile.",
     "Resolução 50% (com DLSS, o modo mais perto), Lanczos, Nitidez 30%, Perfil Portátil."},
    {"Resolution 75% (with DLSS its nearest mode), Lanczos, Sharpness 30%, the Handheld Profile.",
     "Resolução 75% (com DLSS, o modo mais perto), Lanczos, Nitidez 30%, Perfil Portátil."},
    {"Resolution 100% (with DLSS its nearest mode), Catmull-Rom, Sharpness 30%.",
     "Resolução 100% (com DLSS, o modo mais perto), Catmull-Rom, Nitidez 30%."},
    {"Resolution 100% (with DLSS its nearest mode), Lanczos, Sharpness 40%.",
     "Resolução 100% (com DLSS, o modo mais perto), Lanczos, Nitidez 40%."},
    {"Resolution 100% (with DLSS its nearest mode), Lanczos, Sharpness 50%.",
     "Resolução 100% (com DLSS, o modo mais perto), Lanczos, Nitidez 50%."},
    {"Fullscreen", "Tela cheia"},
    {"Fills the whole screen with the game, without the window's border. The same as F11.",
     "O jogo ocupa a tela toda, sem a borda da janela. O mesmo que F11."},
    {"GPU device", "Placa de vídeo"},
    {"Which graphics card the game runs on. Applies after a restart.",
     "Em qual placa de vídeo o jogo roda. Vale depois de reiniciar."},
    {"The best dedicated GPU.", "A melhor placa de vídeo dedicada."},
    {"Frame generation", "Geração de quadros"},
    {"Makes frames between the game's own, for smoother motion, at a little input delay. Fast motion or camera cuts can "
     "show artifacts.",
     "Cria quadros entre os do jogo, para o movimento ficar mais suave, com um pouco de atraso nos comandos. Movimentos "
     "rápidos ou cortes de câmera podem mostrar falhas na imagem."},
    {"One made frame between each two of the game's, by AMD FSR 3.1 from the scene's depth and motion.",
     "Um quadro criado entre cada dois do jogo, pelo AMD FSR 3.1, a partir da profundidade e do movimento da cena."},
    {"Only the game's own frames: no added delay.", "Só os quadros do próprio jogo: sem atraso a mais."},
    {"A frame for each of the display's refreshes: none made when the game keeps up. Frame pacing does not apply.",
     "Um quadro para cada atualização da tela: nenhum é criado quando o jogo dá conta. O Ritmo dos quadros não vale aqui."},
    {"One made frame between each two of the game's.", "Um quadro criado entre cada dois do jogo."},
    {"Two made frames between each two of the game's.", "Dois quadros criados entre cada dois do jogo."},
    {"Three made frames between each two of the game's.", "Três quadros criados entre cada dois do jogo."},
    {"Four made frames between each two of the game's.", "Quatro quadros criados entre cada dois do jogo."},
    {"Five made frames between each two of the game's.", "Cinco quadros criados entre cada dois do jogo."},
    {"Six made frames between each two of the game's.", "Seis quadros criados entre cada dois do jogo."},
    {"Seven made frames between each two of the game's.", "Sete quadros criados entre cada dois do jogo."},
    {"Frame pacing", "Ritmo dos quadros"},
    {"When each frame is shown.", "Quando cada quadro aparece na tela."},
    {"As soon as it is ready.", "Assim que fica pronto."},
    {"At an even interval: less stutter, at up to one frame of delay.",
     "Em intervalos iguais: menos engasgos, com até um quadro de atraso."},
    {"HDR", "HDR"},
    {"HDR10 out on an HDR display (Windows: Use HDR on in the display settings). Expanded: the game's SDR picture, its white at "
     "the paper white and its highlights up to the peak. Native: the game's own HDR, told the display's peak and paper white.",
     "Imagem HDR10 numa tela HDR (no Windows, ligue Usar HDR nas configurações de vídeo). Expandido: a imagem normal do jogo, "
     "com o branco no Branco do HDR e as partes claras até o brilho máximo. Nativo: o próprio HDR do jogo, ajustado ao "
     "brilho da sua tela."},
    {"HDR paper white", "Branco do HDR"},
    {"How bright the picture's white is with HDR on. Around 200 for a normal room; higher in a bright one.",
     "O brilho do branco da imagem com o HDR ligado. Uns 200 num quarto normal; mais num lugar claro."},
    {"HDR peak brightness", "Brilho máximo do HDR"},
    {"The brightest highlights with HDR on: set it to your display's peak brightness.",
     "As partes mais claras com o HDR ligado: coloque o brilho máximo da sua tela."},
    {"Game frame rate cap", "Limite de FPS do jogo"},
    {"Limits frames rendered by the game. Generated frames are additional. Takes a restart. "
     "Past 60 is experimental: the game keeps real time, but some cutscenes and bosses can misbehave (Gohma, the intro's "
     "narration). 60 is the safe choice.",
     "Limita os quadros que o jogo desenha; os quadros criados vêm a mais. Vale depois de reiniciar. Acima de 60 é "
     "experimental: o jogo segue o tempo certo, mas algumas cenas e chefes podem dar problema (Gohma, a narração do "
     "começo). 60 é a escolha segura."},
    {"NVIDIA Reflex", "NVIDIA Reflex"},
    {"NVIDIA GPUs: each frame starts as late as it can, for less input delay. Not used with frame generation.",
     "Placas NVIDIA: cada quadro começa o mais tarde possível, para menos atraso nos comandos. Não vale com a Geração de "
     "quadros."},
    {"The game as without it.", "O jogo como sem ele."},
    {"The driver paces the frames for less latency.", "O driver controla o ritmo dos quadros para menos atraso."},
    {"As On, and the GPU's clocks are kept up as well.", "Como Ligado, e a placa de vídeo fica sempre acelerada."},
    {"AMD Anti-Lag 2", "AMD Anti-Lag 2"},
    {"AMD GPUs: each frame starts as late as it can, for less input delay. Not used with frame generation.",
     "Placas AMD: cada quadro começa o mais tarde possível, para menos atraso nos comandos. Não vale com a Geração de "
     "quadros."},
    {"The driver delays each frame's start for less latency.", "O driver atrasa o começo de cada quadro para menos atraso."},
    {"Output resolution", "Resolução de saída"},
    {"The game's whole frame, HUD included, before it is scaled to the window. Docked only. Applies after a restart.",
     "A imagem inteira do jogo, com o HUD, antes de ir para a janela. Só no modo TV. Vale depois de reiniciar."},
    {"The game's own size.", "O tamanho do próprio jogo."},
    {"The sharpest on a 2560x1440 screen: nothing to scale to the window.",
     "A mais nítida numa tela 2560x1440: nada para esticar na janela."},
    {"The sharpest on a 3840x2160 screen. The game's render targets get a bigger GPU pool, so a full-size 3D fits.",
     "A mais nítida numa tela 3840x2160. O jogo ganha mais memória de vídeo para o 3D em tamanho cheio."},
    {"The sharpest on a 1920x1080 screen: the HUD and text pixel for pixel, nothing to scale to the window.",
     "A mais nítida numa tela 1920x1080: o HUD e os textos pixel por pixel, nada para esticar na janela."},
    {"The sharpest on a 5120x1440 ultrawide screen: the whole frame pixel for pixel. The game's render targets get a bigger "
     "GPU pool, so a full-size 3D fits.",
     "A mais nítida numa tela ultrawide 5120x1440: a imagem toda pixel por pixel. O jogo ganha mais memória de vídeo para "
     "o 3D em tamanho cheio."},
    {"The sharpest on a 5120x2880 (5K) screen: the whole frame pixel for pixel. The game's render targets get a bigger GPU "
     "pool, so a full-size 3D fits.",
     "A mais nítida numa tela 5120x2880 (5K): a imagem toda pixel por pixel. O jogo ganha mais memória de vídeo para o 3D "
     "em tamanho cheio."},
    {"Your own size, from output_res= in hookshot_options.ini.",
     "O seu próprio tamanho, do output_res= no hookshot_options.ini."},
    {"Upscaler", "Upscaler"},
    {"What scales the 3D up to the Output resolution.", "O que aumenta o 3D até a Resolução de saída."},
    {"The game's own network.", "A rede do próprio jogo."},
    {"NVIDIA's, from a smaller 3D. Softer than the game's own: set Sharpness to around 80%. Things that move by "
     "themselves can trail for now.",
     "O da NVIDIA, a partir de um 3D menor. Mais suave que o do jogo: deixe a Nitidez perto de 80%. Coisas que se mexem "
     "sozinhas podem deixar rastro, por enquanto."},
    {"NVIDIA's, the 3D at full size with its edges smoothed; the slowest. Softer than the game's own: set Sharpness to "
     "around 80%. Things that move by themselves can trail for now.",
     "O da NVIDIA, com o 3D em tamanho cheio e as bordas suavizadas; o mais pesado. Mais suave que o do jogo: deixe a "
     "Nitidez perto de 80%. Coisas que se mexem sozinhas podem deixar rastro, por enquanto."},
    {"AMD's, from a smaller 3D, on any GPU: built from AMD's own shaders. Things that move by themselves can trail for "
     "now.",
     "O da AMD, a partir de um 3D menor, em qualquer placa de vídeo. Coisas que se mexem sozinhas podem deixar rastro, por "
     "enquanto."},
    {"AMD's machine-learned upscaler, from a smaller 3D, with AMD's files beside the game. Things that move by themselves "
     "can trail for now.",
     "O upscaler com IA da AMD, a partir de um 3D menor, com os arquivos da AMD junto do jogo. Coisas que se mexem "
     "sozinhas podem deixar rastro, por enquanto."},
    {"AMD's machine-learned upscaler, the 3D at full size with its edges smoothed; the slowest.",
     "O upscaler com IA da AMD, com o 3D em tamanho cheio e as bordas suavizadas; o mais pesado."},
    {"DLSS and DLAA: not in this build (built without NVIDIA's DLSS SDK, see the readme).",
     "DLSS e DLAA: não vêm nesta versão (feita sem o SDK do DLSS da NVIDIA, veja o leia-me)."},
    {"DLSS and DLAA: off for this run (RX_VK_DLSS=0).", "DLSS e DLAA: desligados nesta vez (RX_VK_DLSS=0)."},
    {"DLSS and DLAA: need an NVIDIA RTX GPU.", "DLSS e DLAA: precisam de uma placa NVIDIA RTX."},
    {"DLSS and DLAA: need a newer NVIDIA driver.", "DLSS e DLAA: precisam de um driver da NVIDIA mais novo."},
    {"DLSS and DLAA: DLSS's runtime was not found (the game folder's ngx folder, see the readme).",
     "DLSS e DLAA: os arquivos do DLSS não foram achados (a pasta ngx na pasta do jogo, veja o leia-me)."},
    {"DLSS and DLAA: NVIDIA's NGX on Linux loads DLSS's runtime only from a folder named in ASCII (see the readme).",
     "DLSS e DLAA: no Linux, o NGX da NVIDIA só carrega o DLSS de uma pasta com nome sem acentos (veja o leia-me)."},
    {"AMD FSR 4: not in this build (see the readme).", "AMD FSR 4: não vem nesta versão (veja o leia-me)."},
    {"AMD FSR 4: off for this run (RX_VK_FSR4=0).", "AMD FSR 4: desligado nesta vez (RX_VK_FSR4=0)."},
    {"AMD FSR 4: AMD's files were not found beside the game (see the readme).",
     "AMD FSR 4: os arquivos da AMD não foram achados junto do jogo (veja o leia-me)."},
    {"AMD FSR 4: AMD's files have no FSR 4 for this GPU.", "AMD FSR 4: os arquivos da AMD não têm FSR 4 para esta placa."},
    {"DLSS model", "Modelo do DLSS"},
    {"Which of NVIDIA's DLSS networks draws the picture.", "Qual rede do DLSS da NVIDIA desenha a imagem."},
    {"NVIDIA's transformer model: the one tested most here.", "O modelo transformer da NVIDIA: o mais testado aqui."},
    {"NVIDIA's second-generation transformer, its pick for Performance: sharper, at a little more GPU time.",
     "O transformer de segunda geração, o que a NVIDIA indica para Desempenho: mais nítido, um pouco mais pesado."},
    {"The second-generation transformer as NVIDIA tuned it for Ultra Performance, the smallest Render resolutions.",
     "O transformer de segunda geração ajustado para Ultra Desempenho, as menores resoluções do 3D."},
    {"Lets DLSS's runtime pick for each mode: what it picks can change with a runtime update.",
     "Deixa o DLSS escolher em cada modo: a escolha pode mudar quando o DLSS for atualizado."},
    {"Used by NVIDIA DLSS and DLAA: choose one in the Upscaler row.",
     "Usado pelo NVIDIA DLSS e pelo DLAA: escolha um deles no Upscaler."},
    {"Set for this run by RX_VK_DLSS_PRESET.", "Definido nesta vez pelo RX_VK_DLSS_PRESET."},
    {"Render resolution", "Resolução do 3D"},
    {"The share of the output the 3D is drawn at; the Upscaler fills in the rest.",
     "Em que parte do tamanho da saída o 3D é desenhado; o Upscaler completa o resto."},
    {"DLAA: the 3D at the picture's full size, anti-aliased by DLSS. The sharpest, and the heaviest.",
     "DLAA: o 3D no tamanho cheio da imagem, com as bordas suavizadas pelo DLSS. O mais nítido e o mais pesado."},
    {"FSR 4's native AA: the 3D at the picture's full size. The sharpest, and the heaviest.",
     "AA nativo do FSR 4: o 3D no tamanho cheio da imagem. O mais nítido e o mais pesado."},
    {"The game lowers it when the GPU is busy, and DLSS follows. Set Sharpness to around 80%.",
     "O jogo abaixa quando a placa está pesada, e o DLSS acompanha. Deixe a Nitidez perto de 80%."},
    {"Between Native and Quality: sharper than Quality, a little slower.",
     "Entre Nativa e Qualidade: mais nítido que Qualidade, um pouco mais pesado."},
    {"DLSS's Quality mode: its sharpest from a smaller 3D. Set Sharpness to around 80%.",
     "O modo Qualidade do DLSS: o mais nítido a partir de um 3D menor. Deixe a Nitidez perto de 80%."},
    {"Between Quality and Performance. Set Sharpness to around 80%.",
     "Entre Qualidade e Desempenho. Deixe a Nitidez perto de 80%."},
    {"Faster, and softer. Set Sharpness to around 80%.", "Mais leve e mais suave. Deixe a Nitidez perto de 80%."},
    {"Between Performance and Ultra Performance.", "Entre Desempenho e Ultra Desempenho."},
    {"DLSS's fastest mode, and its softest. Set Sharpness to around 80%.",
     "O modo mais leve do DLSS, e o mais suave. Deixe a Nitidez perto de 80%."},
    {"The game lowers it when the GPU is busy, and FSR follows.", "O jogo abaixa quando a placa está pesada, e o FSR acompanha."},
    {"FSR's Quality mode, 1.5x: its sharpest.", "O modo Qualidade do FSR, 1.5x: o mais nítido."},
    {"FSR's Balanced mode, 1.7x.", "O modo Equilibrado do FSR, 1.7x."},
    {"FSR's Performance mode, 2x: faster, and softer.", "O modo Desempenho do FSR, 2x: mais leve e mais suave."},
    {"FSR's Ultra Performance mode, 3x: its fastest, and its softest.",
     "O modo Ultra Desempenho do FSR, 3x: o mais leve e o mais suave."},
    {"The game lowers it when the GPU is busy.", "O jogo abaixa quando a placa de vídeo está pesada."},
    {"The 3D at the output's full size: the sharpest, and the slowest.", "O 3D no tamanho cheio da saída: o mais nítido e o mais pesado."},
    {"A steady size: lower is faster, and softer.", "Um tamanho fixo: mais baixo é mais leve e mais suave."},
    {"Window scaling filter", "Filtro da janela"},
    {"How the finished picture is scaled to the window, when its size differs from the Output resolution.",
     "Como a imagem pronta é esticada para a janela, quando o tamanho dela é diferente da Resolução de saída."},
    {"The plain copy.", "A cópia simples."}, {"Sharper.", "Mais nítido."},
    {"Sharper still; can ring on edges.", "Mais nítido ainda; pode deixar contornos nas bordas."},
    {"The window is the Output resolution's size: nothing to scale. The choice is kept for another size.",
     "A janela tem o tamanho da Resolução de saída: nada para esticar. A escolha fica guardada para outro tamanho."},
    {"Sharpness", "Nitidez"},
    {"Sharpens the picture as it goes to the window, with any filter. Too much shows halos around edges.",
     "Deixa a imagem mais nítida ao ir para a janela, com qualquer filtro. Demais aparece um brilho em volta das bordas."},
    {"DLSS and DLAA look softer than the game's own upscaler: around 80% suits them.",
     "DLSS e DLAA ficam mais suaves que o upscaler do jogo: perto de 80% fica bom."},
    {"Off: the picture as it is.", "Desligado: a imagem como ela é."},
    {"Vivid colors", "Cores vivas"},
    {"More colour and contrast as the picture goes to the window, like emulators' Vivid filter. Skin and colours already "
     "strong are left as they are. Not with HDR.",
     "Mais cor e contraste na imagem, como o filtro Vivid dos emuladores. A pele e as cores que já são fortes ficam como "
     "estão. Não vale com HDR."},
    {"The picture as the game draws it.", "A imagem como o jogo desenha."},
    {"A touch more colour: the old On.", "Um pouco mais de cor: o antigo Ligado."},
    {"Clearly more colour and contrast.", "Bem mais cor e contraste."},
    {"Much more colour and contrast.", "Muito mais cor e contraste."},
    {"Anisotropic filtering", "Filtro anisotrópico"},
    {"Keeps textures sharp on surfaces seen at a slant, such as the ground ahead: higher is sharper in the distance, for a "
     "little GPU time. Game's is the console's own (1x or 2x).",
     "Deixa as texturas nítidas em superfícies vistas de lado, como o chão à frente: mais alto é mais nítido ao longe, "
     "por um pouco de placa de vídeo. Do jogo é o do console (1x ou 2x)."},
    {"Profile", "Perfil"},
    {"Runs the game as the console docked or handheld. Applies after a restart.",
     "Roda o jogo como o console na TV ou no modo portátil. Vale depois de reiniciar."},
    {"As on a TV: the game at the Output resolution's size.", "Como na TV: o jogo no tamanho da Resolução de saída."},
    {"As in the hands: the game at the console's handheld size (1536x864), lighter on the GPU.",
     "Como na mão: o jogo no tamanho portátil do console (1536x864), mais leve para a placa de vídeo."},
    {"Ultrawide", "Ultrawide"},
    {"The camera's shape on a screen wider or narrower than 16:9 (4:3, 16:10). Applies after a restart.",
     "O formato da câmera numa tela mais larga ou mais estreita que 16:9 (4:3, 16:10). Vale depois de reiniciar."},
    {"The game's own shape.", "O formato do próprio jogo."},
    {"The camera takes the screen's shape, wider or narrower. The HUD keeps its 16:9 shape in the middle.",
     "A câmera fica no formato da tela, mais larga ou mais estreita. O HUD continua em 16:9 no meio."},
    {"Ultrawide menus", "Menus no ultrawide"},
    {"With Ultrawide: the pause, item and map menus, and what shows beside them. Loading screens and pre-rendered movies, "
     "which draw no world either, get the same sides. Changes at once.",
     "Com Ultrawide: os menus de pausa, itens e mapa, e o que aparece do lado deles. Telas de carregamento e vídeos também "
     "ganham os mesmos lados. Muda na hora."},
    {"The menus stretched over the whole screen, as before.", "Os menus esticados na tela toda, como antes."},
    {"The menus in the middle 16:9; beside them, what the game draws there, out of focus.",
     "Os menus no meio, em 16:9; dos lados, o jogo desfocado."},
    {"The menus in the middle 16:9; black beside them.", "Os menus no meio, em 16:9; dos lados, preto."},
    {"Robust buffers", "Buffers protegidos"},
    {"Bounds-checked buffer reads: a read past a buffer's end is dropped instead of stopping the GPU. Applies after a "
     "restart.",
     "Leitura de buffers com checagem: uma leitura fora do lugar é ignorada em vez de travar a placa de vídeo. Vale "
     "depois de reiniciar."},
    {"On, on every card.", "Ligado, em toda placa."}, {"Can be faster on some NVIDIA cards.", "Pode ser mais rápido em algumas placas NVIDIA."},
    {"Always on.", "Sempre ligado."},
    {"Mesh shaders", "Mesh shaders"},
    {"How the game's mesh programs (much of the scenery) run: on the GPU's own mesh shaders, or as compute shaders, which "
     "every GPU has. Applies after a restart.",
     "Como rodam os programas de malha do jogo (boa parte do cenário): nos mesh shaders da placa ou como compute shaders, "
     "que toda placa tem. Vale depois de reiniciar."},
    {"The GPU's own mesh shaders where it has them, the compute path where it has not.",
     "Os mesh shaders da placa quando ela tem; o caminho compute quando não tem."},
    {"The GPU's own mesh shaders for every draw. A GPU without them still uses the compute path.",
     "Os mesh shaders da placa em tudo. Uma placa sem eles continua no caminho compute."},
    {"The compute path on every GPU: try it if your card crashes or leaves objects out. Usually a little slower.",
     "O caminho compute em qualquer placa: teste se o jogo trava ou some algum objeto. Costuma ser um pouco mais lento."},
    {"Takes effect after a restart", "Vale depois de reiniciar o jogo"},

    // ---- Audio ----
    {"Volume", "Volume"},
    {"How loud the game plays. The game's own sound settings and Windows' volume still apply.",
     "O volume do jogo. As opções de som do próprio jogo e o volume do Windows continuam valendo."},
    {"Mute in background", "Mudo em segundo plano"},
    {"Silences the game while another window has the focus. The sound comes back when you switch to the game.",
     "Tira o som do jogo enquanto outra janela está na frente. O som volta quando você volta para o jogo."},
    {"Audio output", "Saída de áudio"},
    {"The speakers the game mixes its sound for. Applies after a restart.",
     "Para quais caixas de som o jogo mistura o áudio. Vale depois de reiniciar."},
    {"5.1 when the default output device has 5.1 or more speakers, else stereo.",
     "5.1 quando a saída de som padrão tem 5.1 ou mais caixas; senão, estéreo."},
    {"Stereo, whatever the device has.", "Estéreo, seja qual for o aparelho."},
    {"5.1, whatever the device has. Mixed down on stereo speakers.",
     "5.1, seja qual for o aparelho. Em caixas estéreo, o som é misturado para dois canais."},

    // ---- Gameplay ----
    {"The language of this menu.", "O idioma deste menu."},
    {"Portuguese when the game or Windows is in Portuguese, else English.",
     "Português quando o jogo ou o Windows está em português; senão, inglês."},
    {"FPS counter", "Contador de FPS"},
    {"Frames per second in the top right corner.", "Quadros por segundo no canto de cima, à direita."},
    {"With frame generation on, the game's own rate too.", "Com a Geração de quadros ligada, mostra também o FPS do próprio jogo."},
    {"Menu size", "Tamanho do menu"},
    {"How large this menu, the FPS counter and the start-up screen are drawn.",
     "O tamanho deste menu, do contador de FPS e da tela de abertura."},
    {"Smaller, for a desk.", "Menor, para jogar na mesa."}, {"The usual size.", "O tamanho normal."},
    {"Larger, for a TV across the room.", "Maior, para uma TV longe."},
    {"Game speed", "Velocidade do jogo"},
    {"How the game keeps time when it runs below 60 fps.", "Como o jogo conta o tempo quando roda abaixo de 60 FPS."},
    {"A step each frame: the game slows down whenever it runs below 60 fps.",
     "Um passo por quadro: o jogo fica mais lento sempre que roda abaixo de 60 FPS."},
    {"Normal speed down to 15 fps, by taking longer steps.", "Velocidade normal até 15 FPS, dando passos maiores."},
    {"Text speed", "Velocidade do texto"},
    {"How fast the letters of dialogue and other messages appear.", "A rapidez com que as letras dos diálogos e mensagens aparecem."},
    {"The game's own pace, with its pauses.", "O ritmo do próprio jogo, com as pausas dele."},
    {"Twice as fast.", "Duas vezes mais rápido."}, {"Four times as fast.", "Quatro vezes mais rápido."},
    {"Each page at once, as pressing A does.", "Cada página de uma vez, como apertar A."},
    {"Text size", "Tamanho do texto"},
    {"How large the text of dialogue, Navi and signs is drawn.", "O tamanho do texto dos diálogos, da Navi e das placas."},
    {"The game's original size.", "O tamanho original do jogo."}, {"A little larger.", "Um pouco maior."},
    {"Larger.", "Maior."}, {"Much larger.", "Bem maior."}, {"Largest.", "O maior."},
    {"What the ocarina hears when the game's microphone option is on (playing an instrument, or humming).",
     "O que a ocarina escuta quando a opção de microfone do jogo está ligada (tocar um instrumento ou cantarolar)."},
    {"The game hears silence.", "O jogo não escuta nada."},
    {"The system's default recording device. On Windows, desktop apps must be allowed to use the microphone (Settings, "
     "Privacy & security).",
     "O microfone padrão do sistema. No Windows, os aplicativos da área de trabalho precisam ter permissão para usar o "
     "microfone (Configurações, Privacidade e segurança)."},
    {"Not plugged in: Default stands in until it is back.", "Desconectado: o Padrão fica no lugar até ele voltar."},
    {"This microphone, by its name. A change applies at once.", "Este microfone, pelo nome. A mudança vale na hora."},
    {"Discord status", "Status no Discord"},
    {"Shows \"Playing Hookshot\" and the time played in your Discord status, while Discord runs on this PC.",
     "Mostra \"Jogando Hookshot\" e o tempo de jogo no seu status do Discord, enquanto o Discord estiver aberto neste PC."},
    {"Restart", "Reiniciar"},
    {"Starts the game again. Save your progress first. Press Enter or A twice to restart. Esc or B cancels.",
     "Abre o jogo de novo. Salve o seu progresso antes. Aperte Enter ou A duas vezes para reiniciar. Esc ou B cancela."},
    {"Quit", "Sair"},
    {"Save your progress first. Press Enter or A twice to quit. Esc or B cancels.",
     "Salve o seu progresso antes. Aperte Enter ou A duas vezes para sair. Esc ou B cancela."},
    // (ocarina: the update's and the reports' rows)
    {"Version", "Versão"}, {"Update", "Atualização"}, {"Report", "Relatório"}, {"Report folder", "Pasta do relatório"},
    {"Automatic reports", "Relatórios automáticos"}, {"Send reports automatically", "Envio automático"},
    {"This game's Ocarina Engine version and the newest one on GitHub. The game asks once at each start, without slowing "
     "anything down, and sends none of your data.",
     "A versão do Ocarina Engine deste jogo e a mais nova publicada no GitHub. O jogo pergunta uma vez a cada início, sem atrasar "
     "nada, e não manda nenhum dado seu."},
    {"Check for updates asks GitHub now. With a newer version, Update now downloads hookshot_vk.dll, checks that it fits this "
     "game and puts it in place; it takes effect from the next start.",
     "Procurar atualização pergunta ao GitHub agora. Com uma versão nova, Atualizar baixa a hookshot_vk.dll, confere se ela serve "
     "para este jogo e a coloca no lugar; ela vale a partir do próximo início."},
    {"Make report puts in a .zip file, in the game folder, what we need to understand a problem: the game's logs, a picture of "
     "the screen, each frame's time over the last 3 minutes and around the stutters, the bugs marked with F9, your settings, "
     "hookshot_env.txt, the graphics card, the processor, the memory and Windows. With Send reports automatically on, it also "
     "goes to Ocarina Engine by itself; off, you attach the file yourself. Your user name and personal folders are left out.",
     "Gerar relatório junta num arquivo .zip, na pasta do jogo, o que precisamos para entender um problema: os logs do jogo, uma "
     "foto da tela, o tempo de cada quadro dos últimos 3 minutos e das travadas, os bugs marcados com F9, suas opções, o "
     "hookshot_env.txt, a placa de vídeo, o processador, a memória e o Windows. Com o Envio automático ligado, ele "
     "também vai sozinho para o Ocarina Engine; desligado, é você quem anexa o arquivo. Seu nome de usuário e suas pastas pessoais "
     "ficam de fora."},
    {"Opens the game folder in Explorer with the last report selected. Saw a bug on screen? Press F9 right then: the picture and "
     "the moment go in the next report (RX_VK_BUG_KEY changes the key; 0 turns it off).",
     "Abre a pasta do jogo no Explorador com o último relatório selecionado. Viu um bug na tela? Aperte F9 na hora: a foto e o "
     "momento vão no próximo relatório (RX_VK_BUG_KEY troca a tecla; 0 desliga)."},
    {"On: when the game closes with an error (it froze, the graphics card stopped or it was forced closed), the next start saves a "
     ".zip report in the game folder and says so on screen; at most 5 are kept. With sending on, stutters and low FPS also make "
     "small performance reports, with no picture. Off: no automatic reports; Make report still works.",
     "Ligado: quando o jogo fecha com erro (travou, a placa de vídeo parou ou foi fechado à força), o próximo início salva um "
     "relatório .zip na pasta do jogo e avisa na tela; ficam no máximo 5. Com o envio ligado, travadas e FPS baixo também geram "
     "relatórios de desempenho leves, sem foto. Desligado: nenhum relatório automático; Gerar relatório continua funcionando."},
    {"On: each report goes over the internet to Ocarina Engine, so the problem reaches whoever fixes it without you attaching "
     "anything: the automatic one after an error, the Make report one, and small performance reports when the game stutters or "
     "the FPS drops (at most 7 a session). Only the report's files go, without your user name, the computer's name or your "
     "personal folders. With no internet, it goes at the next start. Off: nothing is sent; the reports stay in the game folder only.",
     "Ligado: cada relatório vai pela internet para o Ocarina Engine, para o problema chegar a quem corrige sem você precisar anexar "
     "nada: o automático depois de um erro, o do Gerar relatório e os de desempenho, leves, quando o jogo trava ou o FPS cai (no "
     "máximo 7 por sessão). Vão só os arquivos do relatório, sem seu nome de usuário, o nome do computador ou suas pastas pessoais. "
     "Sem internet, ele vai no próximo início. Desligado: nada é enviado; os relatórios ficam só na pasta do jogo."},

    // ---- Controls ----
    {"Controller in background", "Controle em 2º plano"},
    {"Whether the controller plays the game while another window has the focus.",
     "Se o controle funciona no jogo enquanto outra janela está na frente."},
    {"Only while the game's window has the focus.", "Só quando a janela do jogo está na frente."},
    {"Also behind other windows, for streaming or other apps. The keyboard and mouse still need the game's window.",
     "Também atrás de outras janelas, para live ou outros programas. Teclado e mouse ainda precisam da janela do jogo."},
    {"Mouse camera", "Câmera no mouse"},
    {"How the captured mouse turns the camera.", "Como o mouse gira a câmera."},
    {"The mouse stands in for the right stick, with its curve and top speed.",
     "O mouse faz o papel do analógico direito, com a curva e a velocidade máxima dele."},
    {"The mouse turns the camera itself, one to one, as in a PC game.", "O mouse gira a própria câmera, um para um, como num jogo de PC."},
    {"Disable camera centering", "Sem centralizar a câmera"},
    {"Stops the camera gradually turning toward the direction you move, useful with native mouse look. Off keeps the "
     "game's standard behavior. Manual camera controls and recentering still work.",
     "A câmera para de virar sozinha para onde você anda, bom com a câmera Nativa no mouse. Desligado deixa como o jogo "
     "faz. Mover a câmera e centralizar na mão continuam funcionando."},
    {"Mouse sensitivity", "Sensibilidade do mouse"},
    {"How far the camera turns for a mouse movement, with either Mouse camera: at 1.00 native look turns 0.1 degree a "
     "count. The - and = keys change it in game too.",
     "Quanto a câmera gira para cada movimento do mouse, nos dois modos da Câmera no mouse: em 1.00 a câmera Nativa gira "
     "0,1 grau por passo. As teclas - e = também mudam durante o jogo."},
    {"Invert mouse vertical", "Inverter mouse vertical"},
    {"Swaps up and down for the mouse.", "Troca cima e baixo no mouse."},
    {"Moving the mouse forward tilts the camera down.", "Mover o mouse para frente inclina a câmera para baixo."},
    {"Invert camera X", "Inverter câmera X"},
    {"Inverts horizontal look (left/right) on the right analog stick.", "Inverte a câmera na horizontal (esquerda/direita) no analógico direito."},
    {"Invert camera Y", "Inverter câmera Y"},
    {"Inverts vertical look (up/down) on the right analog stick.", "Inverte a câmera na vertical (cima/baixo) no analógico direito."},
    {"Stick dead zone", "Zona morta do analógico"},
    {"How far a stick moves before the game sees it, on each axis. Lower for a precise stick, higher for one that drifts. "
     "The game's own dead zone still follows.",
     "Quanto o analógico precisa se mexer para o jogo perceber, em cada direção. Mais baixo para um analógico preciso, "
     "mais alto para um que mexe sozinho. A zona morta do próprio jogo vem depois."},
    {"Rumble strength", "Força da vibração"},
    {"How strongly the controller rumbles, in percent of the game's rumble. Off turns it off.",
     "A força da vibração do controle, em porcentagem da vibração do jogo. Desligado tira a vibração."},
    {"Motion controls", "Controle por movimento"},
    {"Aims and looks with the controller's gyroscope where the game uses motion, as on the Switch. Works with Switch Pro "
     "controllers and pads in Switch mode, DualSense and DualShock 4. Shown beside it: whether the connected pad has one.",
     "Mira e olha em volta com o giroscópio do controle onde o jogo usa movimento, como no Switch. Funciona com Switch Pro "
     "Controller, controles no modo Switch, DualSense e DualShock 4. Ao lado aparece se o controle ligado tem giroscópio."},
    {"The sticks alone aim.", "Só os analógicos miram."},
    {"Switch Pro controllers and pads in Switch mode, DualSense and DualShock 4. The value says whether the connected pad "
     "has a gyroscope.",
     "Switch Pro Controller, controles no modo Switch, DualSense e DualShock 4. O valor diz se o controle ligado tem "
     "giroscópio."},
    {"Motion sensitivity X", "Sensibilidade do movimento X"},
    {"How far the aim moves left and right for a given turn of the controller. 100% is the console's own.",
     "Quanto a mira anda para os lados quando você gira o controle. 100% é o do console."},
    {"Motion sensitivity Y", "Sensibilidade do movimento Y"},
    {"How far the aim moves up and down for a given tilt of the controller. 100% is the console's own.",
     "Quanto a mira anda para cima e para baixo quando você inclina o controle. 100% é o do console."},
    {"Invert motion X", "Inverter movimento X"},
    {"Turns the motion aim's left and right around.", "Troca esquerda e direita na mira por movimento."},
    {"Invert motion Y", "Inverter movimento Y"},
    {"Turns the motion aim's up and down around.", "Troca cima e baixo na mira por movimento."},
    {"Motion deadzone", "Zona morta do movimento"},
    {"How slow a turn counts as none, the console's three settings: Standard is its own. High steadies a pad that jitters "
     "when held still; Low keeps the smallest movements.",
     "Abaixo de qual giro o movimento é ignorado, com os três ajustes do console: Padrão é o dele. Alta firma um controle "
     "que treme parado; Baixa pega até os menores movimentos."},
    {"Motion", "Movimento"},
    {"Motion controls: aiming with the controller's gyroscope, its sensitivity and its inverts.",
     "Controle por movimento: mirar com o giroscópio do controle, a sensibilidade e as inversões."},
    {"Layout preset", "Layout dos botões"},
    {"Which face button is which. Binding one yourself on the Controls page makes it Custom.",
     "Qual botão é qual. Mudar um você mesmo na página de botões deixa como Personalizado."},
    {"The Switch's order: A on the right, B at the bottom, X at the top, Y on the left.",
     "A ordem do Switch: A à direita, B embaixo, X em cima, Y à esquerda."},
    {"A at the bottom (action and talk), B on the right, X on the left, Y at the top.",
     "A embaixo (ação e conversar), B à direita, X à esquerda, Y em cima."},
    {"A at the bottom, B on the right; X and Y as on the Switch.", "A embaixo, B à direita; X e Y como no Switch."},
    {"Bindings of your own, from the Controls page.", "Botões do seu jeito, da página de botões."},
    {"Button prompts", "Ícones dos botões"},
    {"The button icons the game shows, in its texts and menus: those of this controller, for the buttons your controls "
     "put each action on.",
     "Os ícones de botão que o jogo mostra nos textos e menus: os deste controle, nos botões onde você colocou cada ação."},
    {"Those of the first controller, from its name (SDL's). The Switch's when it has none, as with XInput alone.",
     "Os do primeiro controle, pelo nome dele. Os do Switch quando ele não tem nome, como só com XInput."},
    {"The layout preset, the button prompts, and which gamepad button and which keys press each of the Switch's "
     "buttons.",
     "O layout, os ícones dos botões e qual botão do controle e quais teclas apertam cada botão do Switch."},
    {"Menu shortcut", "Atalho do menu"},
    {"Which keys open and close this menu. F1 always does; F1 only leaves Insert free for an overlay. With Esc, Esc opens it "
     "too, as a PC game's pause menu (open, Esc still goes back a page or closes it).",
     "Quais teclas abrem e fecham este menu. F1 sempre abre; Só F1 deixa o Insert livre para outro programa. Com Esc, o "
     "Esc também abre, como o menu de pausa de um jogo de PC (aberto, o Esc volta uma página ou fecha)."},

    // ---- the Controls page ----
    {"Reset to defaults", "Voltar ao padrão"},
    {"Press the gamepad button to use. Esc cancels.", "Aperte o botão do controle que quer usar. Esc cancela."},
    {"Press the key to use. Esc cancels.", "Aperte a tecla que quer usar. Esc cancela."},
    {"Which gamepad button and which keys press each of the Switch's buttons. Enter binds the one selected; Delete clears "
     "it. The game's button icons follow after a restart.",
     "Qual botão do controle e quais teclas apertam cada botão do Switch. Enter define o escolhido; Delete apaga. Os "
     "ícones de botão do jogo mudam depois de reiniciar."},
    {"Gamepad: the D-pad moves, A binds, B goes back.", "No controle: o direcional anda, A define, B volta."},
    {"Button", "Botão"}, {"Gamepad", "Controle"}, {"Keyboard", "Teclado"},
    {"Press a button...", "Aperte um botão..."}, {"Press a key...", "Aperte uma tecla..."},
    {"Plus", "Mais (+)"}, {"Minus", "Menos (-)"},
    {"Left stick press", "Clicar analóg. esq."}, {"Right stick press", "Clicar analóg. dir."},
    {"D-pad up", "Direcional cima"}, {"D-pad down", "Direcional baixo"}, {"D-pad left", "Direcional esquerda"},
    {"D-pad right", "Direcional direita"},
    {"Move forward", "Andar: frente"}, {"Move back", "Andar: trás"}, {"Move left", "Andar: esquerda"},
    {"Move right", "Andar: direita"}, {"Camera up", "Câmera: cima"}, {"Camera down", "Câmera: baixo"},
    {"Camera left", "Câmera: esquerda"}, {"Camera right", "Câmera: direita"},
    {"Bottom button", "Botão de baixo"}, {"Right button", "Botão da direita"}, {"Left button", "Botão da esquerda"},
    {"Top button", "Botão de cima"},
    {"Cross", "X"}, {"Circle", "Círculo"}, {"Square", "Quadrado"}, {"Triangle", "Triângulo"},
    {"Space", "Espaço"}, {"Up", "Seta para cima"}, {"Down", "Seta para baixo"}, {"Left", "Seta esquerda"},
    {"Right", "Seta direita"}, {"Left click", "Clique esquerdo"}, {"Right click", "Clique direito"},
    {"Middle click", "Clique do meio"}, {"Key", "Tecla"},

    // ---- the restart question ----
    {"Restart Needed", "Precisa reiniciar"}, {"Restart Now", "Reiniciar agora"},
    {"Progress since your last save is lost.", "O que você jogou desde o último save se perde."},

    // ---- Debug (the tools; the Developer rows stay in English: technical) ----
    {"Debug tools", "Ferramentas de teste"},
    {"Testing tools: no-clip flight, a free camera, teleport, Link's age, the inventory, the clock, heart protection and "
     "movement speed. Off puts flight, the free camera and heart protection away and Link back to his own speed.",
     "Ferramentas de teste: voo livre, câmera livre, teletransporte, idade do Link, inventário, relógio, proteção de "
     "corações e velocidade. Desligado tira o voo, a câmera livre e a proteção, e o Link volta à velocidade normal."},
    {"Heart protection", "Proteger corações"},
    {"Protects Link's hearts. Hits can still knock him back.", "Protege os corações do Link. Os golpes ainda podem empurrá-lo."},
    {"No-clip flight", "Voo livre"},
    {"Enable double-tap Jump for takeoff and landing. Hold Jump to climb or L to descend.",
     "Aperte Pular duas vezes para decolar e pousar. Segure Pular para subir ou L para descer."},
    {"Mark position", "Marcar posição"},
    {"Remember Link's position for this loaded scene.", "Guarda a posição do Link neste lugar."},
    {"Return to mark", "Voltar à marca"},
    {"Teleport Link to the marked position in the current scene.", "Leva o Link de volta à posição marcada neste lugar."},
    {"Set time", "Mudar a hora"},
    {"Choose an hour with Left or Right. Enter sets the world clock.", "Escolha a hora com esquerda ou direita. Enter muda o relógio do jogo."},
    {"Inventory item", "Item"},
    {"Choose a consumable with Left or Right.", "Escolha um item com esquerda ou direita."},
    {"Change quantity", "Mudar quantidade"},
    {"Choose how many to add or remove. Enter applies the change.", "Escolha quanto pôr ou tirar. Enter aplica."},
    {"Free camera", "Câmera livre"},
    {"Move and look with your usual controls. Page Up / Page Down or R / L changes height.",
     "Ande e olhe com os comandos de sempre. Page Up / Page Down ou R / L mudam a altura."},
    {"Opens the map list. Link travels through the game's own map change, in his current age.",
     "Abre a lista de mapas. O Link viaja pela troca de mapa do próprio jogo, na idade atual."},
    {"Switch age", "Trocar idade"},
    {"Swaps Link between child and adult and reloads this map in the other age (the Market when this map has no match).",
     "Troca o Link entre criança e adulto e recarrega o mapa na outra idade (o Mercado quando este mapa não existe na outra)."},
    {"Enter opens the inventory editor: unlock or relock equipment, items, upgrades and quest items, and change amounts.",
     "Enter abre o editor de inventário: libere ou bloqueie equipamentos, itens, melhorias e itens de missão, e mude "
     "quantidades."},
    {"Movement speed", "Velocidade do Link"},
    {"How fast Link walks, runs and swims, as a multiple of his own speed. Choose with Left or Right.",
     "A velocidade com que o Link anda, corre e nada, em vezes a velocidade normal. Escolha com esquerda ou direita."},
    {"Developer", "Desenvolvedor"},
    {"Player", "Jogador"}, {"Camera & world", "Câmera e mundo"},   // (the group Travel: vk_menu.cpp, "Travel" is Viajar)
    {"Mark", "Marcar"}, {"Return", "Voltar"}, {"Choose map", "Escolher mapa"}, {"Edit items", "Editar itens"},
    {"All", "Todos"}, {"Equipment", "Equipamento"}, {"Items", "Itens"}, {"Masks", "Máscaras"}, {"Upgrades", "Melhorias"},
    {"Quest", "Missão"},
    {"Load a save to edit the inventory.", "Carregue um save para editar o inventário."},
    {"Load a save to edit this item.", "Carregue um save para editar este item."},
    {"Choose Edit amount, then use Left and Right to change the quantity.",
     "Escolha Mudar quantidade e use esquerda e direita para mudar."},
    {"Unlock or relock this item.", "Libere ou bloqueie este item."},
    {"Unavailable", "Indisponível"}, {"Unlocked", "Liberado"}, {"Locked", "Bloqueado"},
    {"Select a destination, then Travel (or Enter) to go to its map entrance. Link keeps his current age.",
     "Escolha um destino e aperte Viajar (ou Enter) para ir à entrada do mapa. O Link continua na idade atual."},
    {"adult only", "só adulto"}, {"Overworld", "Mundo"}, {"Dungeons", "Masmorras"},
};

// (a text that ends with one of these: its start kept as it is (a microphone's name, a size), then this)
inline constexpr Pair kSuffixes[] = {
    {" (not connected)", " (desconectado)"}, {" (native)", " (nativa)"}, {" (game's)", " (do jogo)"},
};
// (a text that starts with one of these: then its rest translated as it can be)
inline constexpr Pair kPrefixes[] = {
    {"Native (DLAA", "Nativa (DLAA"}, {"Native (FSR 4", "Nativa (FSR 4"},
};
// clang-format on

inline bool word_char(char c) {
    const unsigned char u = (unsigned char)c;
    return std::isalnum(u) || c == '\'' || c == '-' || u >= 0x80;
}
/// A word shown as it is in either language: numbers and sizes (2560x1440, 2x, 1.25x, 67), words in capitals (DLSS, FSR,
/// NVIDIA, RTX, K)
inline bool neutral_word(std::string_view w) {
    for (size_t i = 0; i < w.size(); i++) {
        const unsigned char c = (unsigned char)w[i];
        if (std::islower(c) && !(c == 'x' && i > 0 && std::isdigit((unsigned char)w[i - 1]))) return false;
        if (c >= 0x80 || c == '\'') return false;
    }
    return true;
}
inline const char* exact(std::string_view en) {
    for (const Pair& p : kPairs)
        if (en == p.en) return p.pt;
    return nullptr;
}
/// en part by part (see the top of this file): true and the Portuguese in *out when every part is known
inline bool pieces(std::string_view en, std::string* out) {
    std::string r;
    size_t at = 0;
    bool any = false;
    while (at < en.size()) {
        size_t best = 0;
        const char* best_pt = nullptr;
        for (const Pair& p : kPairs) {
            const size_t n = std::strlen(p.en);
            if (n <= best || n > en.size() - at || en.compare(at, n, p.en) != 0) continue;
            if (at + n < en.size() && word_char(p.en[n - 1]) && word_char(en[at + n])) continue;   // (a whole word only)
            best = n, best_pt = p.pt;
        }
        if (best_pt) {
            r += best_pt, at += best, any = true;
            continue;
        }
        if (!word_char(en[at])) {
            r += en[at++];
            continue;
        }
        size_t end = at;
        while (end < en.size() && word_char(en[end])) end++;
        if (!neutral_word(en.substr(at, end - at))) return false;
        r.append(en.substr(at, end - at)), at = end;
    }
    if (!any) return false;
    *out = std::move(r);
    return true;
}
/// The Portuguese of en: true and *out when the table has it (whole, by parts, or with a suffix or prefix of its own)
inline bool translate(std::string_view en, std::string* out) {
    if (en.empty()) return false;
    if (const char* pt = exact(en)) return *out = pt, true;
    for (const Pair& s : kSuffixes) {
        const size_t n = std::strlen(s.en);
        if (en.size() > n && en.compare(en.size() - n, n, s.en) == 0) {
            *out = std::string(en.substr(0, en.size() - n)) + s.pt;
            return true;
        }
    }
    for (const Pair& s : kPrefixes) {
        const size_t n = std::strlen(s.en);
        if (en.size() >= n && en.compare(0, n, s.en) == 0) {
            std::string tail;
            if (!pieces(en.substr(n), &tail)) tail = std::string(en.substr(n));
            *out = s.pt + tail;
            return true;
        }
    }
    return pieces(en, out);
}

}  // namespace mkwvk::menu_lang
