<p align="center"><img src="docs/ocarina-engine/logo-escuro.png" alt="Ocarina Engine" width="560"></p>

# Ocarina Engine

Uma versão do [Hookshot](https://github.com/NotRexed/Hookshot) focada em desempenho: mais FPS e menos travadas no
Zelda: Ocarina of Time (Switch 2) no PC, em placas comuns, não só nas topo de linha.

O Ocarina Engine não traz o jogo. Você monta o jogo com o HookshotBuilder a partir da sua própria cópia, e o Ocarina Engine
troca só o renderizador (`hookshot_vk.dll`).

## v0.4.1: correção da v0.4.0

- **AMD RX 6000 (RX 6400 a 6950 XT):** caminho seguro dos efeitos de malha, mesmo com Mesh shaders em Nativo (a RX 6600 perdia a placa, e às vezes travava o PC, logo depois de carregar o save).

## Novo na v0.4: replay mais leve, sem a névoa de um quadro e relatório de problema

- **Compilação guiada pelo próprio jogo (PGO):** o renderizador foi otimizado pelo caminho que o jogo realmente usa. No notebook com RTX 4050, praça do Burgo: o replay (a parte que transforma os comandos do jogo em Vulkan) ficou ~30% mais rápido e o FPS subiu 5 a 7%.
- **Sem a névoa de um quadro:** a cena às vezes ficava enevoada por um quadro. A causa era o recurso de páginas tardias, que vem desligado agora (`RX_VK_LATE_LIVE=1` liga). O log ganhou um detector de piscada (`vk: scene flicker`).
- **Guarda de RAM (PCs de 16 GB):** quando a memória do PC aperta, a preparação de shaders usa menos threads e a memória de vídeo parada é devolvida, para o Windows não fechar o jogo por falta de memória. `RX_VK_RAM_GUARD=0` desliga.
- **Relatório de problema:** menu Insert/F1 > Gameplay > **Gerar relatório**, e **F9** para marcar um bug na hora em que ele acontece. Depois de uma sessão que terminou mal, o relatório é feito sozinho. Ele junta o log, as opções e um resumo do PC (placa, driver, Windows, memória), **sem dados pessoais**, num .zip na pasta do jogo. Mande o .zip junto com o seu relato.
- **Envio automático dos relatórios:** ligado de fábrica; o jogo avisa numa linha antes do primeiro envio. Vai só o .zip do relatório (sem dados pessoais) para um receptor do Ocarina Engine na Cloudflare. Desliga no menu Insert/F1 > Gameplay > Enviar relatórios automaticamente, ou com `OCARINA_REPORT_UPLOAD=0`.
- **Memória de vídeo pelo que o jogo precisa:** as texturas usam no máximo 1,5 GB em qualquer placa, em vez de uma fatia da placa. `RX_VK_TEX_CEIL_MB=<MB>` muda, 0 tira o teto.
- **Menu em português:** Insert/F1 > Jogo > Idioma / Language (Automático segue o idioma do jogo).
- **Abas pelo controle:** LB/RB (L1/R1 no PlayStation) trocam de aba; o direcional para os lados também, numa linha que não tem valor para mudar.
- **"Salvar as alterações?"** ao fechar o menu: Sim grava; Não volta tudo ao que era quando o menu abriu.
- **Cores vivas com nível:** Desligado, Leve, Médio e Forte (de fábrica). `RX_VK_VIVID=0..3` muda.
- **Visual próprio:** tela de preparo de shaders e menu Insert com a cara do Ocarina Engine, e ícone de ocarina na janela e na barra de tarefas.
- **Correção:** com pouca RAM livre a tela de preparo podia ficar parada em 100%.

Na primeira abertura depois da troca, o jogo prepara os shaders de novo (alguns minutos, uma vez só).

## Na v0.3: menos travadas ainda, e atualização pelo menu

**v0.3.1:** a trava de VRAM da v0.3.0 deixava o jogo mais lento em algumas placas e agora vem desligada (`RX_VK_VRAM_GOV=1` liga). A VRAM cai pelo **detalhe de textura**: em placas de até 6,5 GB, as texturas ASTC grandes sobem sem o nível de detalhe maior (`RX_VK_TEX_DETAIL=0` desliga). Na GTX 1660 Super, no Burgo: ~740 MB a menos de VRAM, abaixo do limite da placa, e 38% menos quadros acima de 50 ms. Saíram também as correções trazidas do Hookshot oficial e do Mako, que custavam FPS.

Medido na GTX 1660 Super (Burgo de Hyrule, mesmo caminho repetido, 3 pares alternados de 200 s): a v0.3 contra a mesma DLL com as correções desligadas (= v0.2):
**quadros acima de 50 ms 174 contra 292 (-40%), acima de 100 ms 15 contra 19 (-22%), 28,9 contra 27,3 fps.** As travadas de 110 ms por sincronismo de memória sumiram.

- **Importação de memória em segundo plano:** quando o jogo começa a usar um pedaço novo de memória, o driver demorava 15 a 60 ms para "prender" essas páginas no meio do desenho. Agora isso roda numa thread de fundo (em placas NVIDIA).
- **Páginas tardias sem espera:** as páginas que a placa ainda estava escrevendo de volta são copiadas pela própria placa, em vez de o jogo parar e esperar.
- **Invalidação por textura:** quando uma textura sai da memória, só os desenhos que a usavam são refeitos, não todos.
- **Descriptor reuse e Sampler cache** ligados de fábrica (menu Debug).
- **Trava de VRAM:** a memória da placa fica abaixo do limite que o driver permite (meta de 88%): devolve ao driver o espaço vazio preso nos blocos, enxuga o orçamento de texturas só no excesso e, por último, os espelhos. No notebook com RTX 4050 6 GB, praça do Burgo: 4,9-5,0 GB em vez de 5,3-5,4 GB, **~28 fps em vez de ~20** e cerca de 1/5 dos quadros acima de 50 ms. `RX_VK_VRAM_GOV=0` desliga; `RX_VK_VRAM_TARGET_PCT=<50-97>` muda a meta.
- **Correções trazidas do Hookshot oficial e do Mako:** congelamento em placas AMD ao fechar menu ou mapa (programas que o driver AMD derruba ficam fora dele), perda da placa ao desenhar em fatias 3D comprimidas, leitura da CPU que agora espera a escrita da GPU, desenhos que escrevem memória mantidos enquanto o pipeline compila, chefes e inimigos novos que apareciam todos azuis, texturas adiadas só uma vez e texturas grandes zeradas limpas na GPU.
- **Botão Atualizar** (Insert/F1 > Gameplay): o jogo avisa quando sai uma versão nova do Ocarina Engine e troca o `hookshot_vk.dll` sozinho, com um reinício. `OCARINA_NO_UPDATE_CHECK=1` (variável do sistema) desliga.
- **Marca:** título da janela, abertura e menu dizem Ocarina Engine (`RX_VK_BRAND=0` volta ao original).

Para desligar e comparar (no `hookshot_env.txt`): `RX_VK_ASYNC_IMPORT=0`, `RX_VK_LATE_LIVE=0`, `RX_VK_TEX_GONE_EXACT=0`, `RX_VK_DESC_SLOTGEN=0`, `RX_VK_SAMPLER_CACHE=0`.

## O que mudou na v0.2 em relação ao Hookshot

Comparado com o Hookshot 2.1.0, mesmo jogo, mesmo caminho repetido na praça do Burgo de Hyrule, 100% de resolução, sem geração de quadros:

| PC | Área | Hookshot 2.1.0 | Ocarina Engine v0.2 |
|---|---|---|---|
| GTX 1660 Super 6 GB + Xeon E5-2666 v3 | Burgo de Hyrule | 23,1 fps, 106 quadros >50 ms a cada 30 s | **27,5 fps (+19%), 43 quadros >50 ms (-59%)** |
| GTX 1660 Super 6 GB + Xeon E5-2666 v3 | Kakariko | 22,9 fps, 130 quadros >50 ms | **38,5 fps (+68%), 8 quadros >50 ms** |
| RTX 4050 Laptop 6 GB + i7-13620H | Burgo de Hyrule | ~24 fps, ~149 quadros >50 ms, 9-19 >100 ms | **~31 fps, 13-21 quadros >50 ms, 3-5 >100 ms** |
| RTX 4050 Laptop 6 GB + i7-13620H | Kakariko | – | **~60 fps, nenhum quadro acima de 31 ms** |

Em sessões de 13 minutos o FPS não cai com o tempo.

O que foi feito:
- **Menos travadas ao virar a câmera:** a área de envio de dados para a placa é reaproveitada e fica na RAM; imagens temporárias são reaproveitadas mesmo com a memória da placa cheia.
- **Memória da placa que não estoura:** todas as texturas ASTC ficam comprimidas na placa (BC3), a memória de texturas é compactada, e as texturas não são jogadas fora e refeitas a cada quadro quando a memória aperta.
- **Metade do trabalho da placa:** só os pedaços de 4 KB que a GPU mudou voltam para a memória do jogo.
- **Memória ajustada à sua placa** (4, 6, 8 GB ou mais), sem números fixos.
- **Cores vivas** (Vivid colors): ligado de fábrica, desliga no menu Insert/F1 > Gráficos.

Cada mudança pode ser desligada para comparar (no `hookshot_env.txt`): `RX_VK_STAGING_REUSE=0`, `RX_VK_TARGET_POOL_KEEP=0`,
`RX_VK_WB_PRINTS=0`, `RX_VK_TEX_STABLE=0`, `RX_VK_TARGET_POOL_STABLE=0`, `RX_VK_COMPACT=0`, `RX_VK_ASTC_BC_MB=2`, `RX_VK_VIVID=0`.

## Como instalar

O Ocarina Engine troca só o renderizador (`hookshot_vk.dll`) do jogo montado pelo Hookshot. Não traz o jogo: você precisa da sua própria cópia (o arquivo `.nup`).

### Já uso o Hookshot (só trocar os arquivos)
1. Confira que o seu HookshotBuilder é o **v2.1.0** (é a versão com que esta release funciona).
2. Feche o jogo.
3. Na pasta do jogo, ache o `hookshot_vk.dll` (nos builders novos ele fica na subpasta `bin`). Guarde uma cópia dele, se quiser voltar.
4. Baixe o `hookshot_vk.dll` da [última release](https://github.com/Samuel25z/ocarina-engine/releases/latest) e coloque por cima.
5. Baixe o `hookshot_env.txt` e coloque **na mesma pasta do `hookshot_vk.dll`** (nos builders novos, dentro da `bin`: o `hookshot.exe` da raiz é só o lançador; o jogo de verdade é o `bin\hookshot.exe`, e é ele que lê o arquivo).
6. Abra o jogo. Na primeira vez ele prepara os shaders de novo (alguns minutos). Seus saves e suas opções continuam os mesmos.

### Começando do zero (nunca usei o Hookshot)
1. Baixe o **HookshotBuilder v2.1.0** na [página de releases do Hookshot](https://github.com/NotRexed/Hookshot/releases/tag/v2.1.0).
2. Abra o builder, aponte para o seu `.nup` do jogo e monte. Leva um tempo e pede bastante espaço livre no disco.
3. Quando terminar, siga os passos de "Já uso o Hookshot" acima.

### Versões novas
- A partir da v0.3, o próprio jogo avisa quando sai versão nova: menu **Insert/F1 > Gameplay > Atualização**. Ele baixa e troca a DLL; é só reiniciar o jogo.
- **Atenção:** se você atualizar o Hookshot pelo HookshotBuilder, ele põe a DLL original de volta. Aí é só trocar de novo.
- Se o jogo não abrir depois da troca, a versão do builder não casa com a da DLL: use o builder indicado na release.

### English (short)
**Already on Hookshot:** HookshotBuilder **v2.1.0** required. Close the game, replace `hookshot_vk.dll` (game folder or its `bin` subfolder) with the one from the [latest release](https://github.com/Samuel25z/ocarina-engine/releases/latest), put `hookshot_env.txt` in the same folder as `hookshot_vk.dll` (the `bin` subfolder on newer builders: the root `hookshot.exe` is only the launcher), start the game (it prepares shaders once). Saves and settings are kept.
**From scratch:** build the game from your own `.nup` with [HookshotBuilder v2.1.0](https://github.com/NotRexed/Hookshot/releases/tag/v2.1.0), then do the above.
**Updates:** from v0.3 on, the game offers new versions in **Insert/F1 > Gameplay**. Updating Hookshot through its builder puts the original DLL back; just replace it again.

## Quem faz

Ocarina Engine é um projeto brasileiro, feito por [@Samuel25z](https://github.com/Samuel25z).

## Créditos e licença

Ocarina Engine é um fork do [Hookshot](https://github.com/NotRexed/Hookshot) (NotRexed e colaboradores) e segue a mesma
licença, **GPL v3** (arquivo `LICENSE`). Todo o código das mudanças está neste repositório, e quem redistribuir uma versão modificada deve publicar o código dela sob a mesma licença.
O Hookshot original continua com os seus autores; as mudanças do Ocarina Engine estão nos commits marcados "Ocarina Engine" e "port:".
Não é afiliado nem endossado pela Nintendo. The Legend of Zelda e Ocarina of Time são marcas da Nintendo. Nenhum arquivo do jogo é distribuído aqui: você precisa da sua própria cópia, obtida legalmente.
