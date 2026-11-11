<p align="center"><img src="docs/ocarina-engine/logo-escuro.png" alt="Ocarina Engine" width="560"></p>

# Ocarina Engine

Uma versão do [Hookshot](https://github.com/NotRexed/Hookshot) focada em desempenho: mais FPS e menos travadas no
Zelda: Ocarina of Time (Switch 2) no PC, em placas comuns, não só nas topo de linha.

O Ocarina Engine não traz o jogo. Você monta o jogo com o HookshotBuilder a partir da sua própria cópia, e o Ocarina Engine
troca só o renderizador (`hookshot_vk.dll`).

## O que muda em relação ao Hookshot

Medido num notebook com RTX 4050 (6 GB), em 100% de resolução e sem geração de quadros:

| Área | Hookshot | Ocarina Engine |
|---|---|---|
| Burgo de Hyrule (a mais pesada) | ~22 fps, muitas travadas ao virar a câmera | ~29-31 fps, quadros acima de 50 ms caíram de 60-190 para 10-20 a cada 30 s |
| Kakariko | ~27-45 fps | ~45-57 fps, quase nenhum quadro acima de 50 ms |

As mudanças:
- **Menos travadas ao virar a câmera:** a área de envio de dados para a placa (64 MB) é reaproveitada e fica na RAM, em vez de ser
  criada de novo a cada envio (20-110 ms cada uma com a memória da placa cheia).
- **Imagens temporárias reaproveitadas:** o jogo não cria mais ~10 imagens novas por quadro quando a memória da placa enche.
- **Metade do trabalho da placa no write-back:** só os pedaços de 4 KB que a GPU mudou voltam para a memória do jogo (ideia do
  fork de juniortesla53, refeita), e isso continua valendo com a memória da placa cheia.
- **Memória ajustada à sua placa:** o espelho na VRAM e o limite de texturas são calculados pela memória de cada placa.

Cada mudança pode ser desligada para comparar: `RX_VK_STAGING_REUSE=0`, `RX_VK_TARGET_POOL_KEEP=0`, `RX_VK_WB_PRINTS=0`
(no arquivo `hookshot_env.txt`, ao lado do jogo).

## Como instalar

1. Monte o jogo com o **HookshotBuilder** (página de Releases do Hookshot), como de costume.
2. Baixe o `hookshot_vk.dll` da página de Releases deste repositório e coloque por cima do que está na pasta do jogo
   (guarde o original, se quiser voltar).
3. Copie o `hookshot_env.txt` do release para a pasta do jogo.
4. Abra o jogo. Na primeira vez ele prepara os shaders de novo (um minuto e pouco).

Cada release diz com qual versão do HookshotBuilder ele foi testado. Se o jogo não abrir, a versão do builder não casa:
use a indicada no release.

## Créditos e licença

Ocarina Engine é um fork do [Hookshot](https://github.com/NotRexed/Hookshot) (NotRexed e colaboradores) e segue a mesma
licença, **GPL v3** (arquivo `LICENSE`). Todo o código das mudanças está neste repositório.
Não é afiliado à Nintendo. The Legend of Zelda é marca da Nintendo. Nenhum arquivo do jogo é distribuído aqui.
