<p align="center"><img src="docs/ocarina-engine/logo-escuro.png" alt="Ocarina Engine" width="560"></p>

# Ocarina Engine

Uma versão do [Hookshot](https://github.com/NotRexed/Hookshot) focada em desempenho: mais FPS e menos travadas no
Zelda: Ocarina of Time (Switch 2) no PC, em placas comuns, não só nas topo de linha.

O Ocarina Engine não traz o jogo. Você monta o jogo com o HookshotBuilder a partir da sua própria cópia, e o Ocarina Engine
troca só o renderizador (`hookshot_vk.dll`).

## Novo na v0.3: menos travadas ainda, e atualização pelo menu

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

1. Monte o jogo com o **HookshotBuilder** (página de Releases do Hookshot), como de costume.
2. Baixe o `hookshot_vk.dll` da página de Releases deste repositório e coloque por cima do `hookshot_vk.dll` que já existe na pasta do jogo (nos builders mais novos ele fica dentro da subpasta `bin`)
   (guarde o original, se quiser voltar).
3. Copie o `hookshot_env.txt` do release para a pasta do jogo.
4. Abra o jogo. Na primeira vez ele prepara os shaders de novo (um minuto e pouco).

Esta versão (v0.3.0) é para o **HookshotBuilder v2.1.0**. A partir dela, as próximas versões chegam pelo botão Atualizar no menu do jogo (Insert/F1 > Gameplay). Cada release diz com qual versão do builder ele funciona. Se o jogo não abrir, a versão do builder não casa:
use a indicada no release.

## Quem faz

Ocarina Engine é um projeto brasileiro, feito por [@Samuel25z](https://github.com/Samuel25z).

## Créditos e licença

Ocarina Engine é um fork do [Hookshot](https://github.com/NotRexed/Hookshot) (NotRexed e colaboradores) e segue a mesma
licença, **GPL v3** (arquivo `LICENSE`). Todo o código das mudanças está neste repositório, e quem redistribuir uma versão modificada deve publicar o código dela sob a mesma licença.
O Hookshot original continua com os seus autores; as mudanças do Ocarina Engine estão nos commits marcados "Ocarina Engine" e "port:".
Não é afiliado nem endossado pela Nintendo. The Legend of Zelda e Ocarina of Time são marcas da Nintendo. Nenhum arquivo do jogo é distribuído aqui: você precisa da sua própria cópia, obtida legalmente.
