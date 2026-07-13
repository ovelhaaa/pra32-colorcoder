# Guia de Pinout e Montagem — Waveshare RP2350 Zero + ST7789 + Encoder + PCM5102

Este guia documenta uma montagem de referência para o projeto PRA32-U2/P usando:

- **Placa:** Waveshare RP2350 Zero
- **Display:** ST7789 SPI (layout 284x76)
- **Entrada de UI:** Encoder rotativo incremental com push button
- **Áudio:** DAC I2S baseado em **PCM5102/PCM5102A** (ex.: GY-PCM5102)

> Objetivo: manter o comportamento de áudio e da UI conforme o caminho já migrado no código, alterando apenas mapeamento físico e instruções de integração.

---

## 1) Pinout de referência (firmware atual)

## 1.1 Display ST7789 (SPI)

| Sinal ST7789 | Macro no firmware | GPIO RP2350 Zero |
|---|---|---|
| SCK / CLK | `PRA32_U2_ST7789_PIN_SCK` | **GP2** |
| MOSI / SDA | `PRA32_U2_ST7789_PIN_MOSI` | **GP3** |
| CS | `PRA32_U2_ST7789_PIN_CS` | **GP17** |
| DC / A0 | `PRA32_U2_ST7789_PIN_DC` | **GP8** |
| RST / RES | `PRA32_U2_ST7789_PIN_RST` | **GP12** |
| BL / LED (backlight) | (não controlado por macro dedicada) | 3V3 (fixo) ou GPIO livre (opcional) |
| VCC | — | 3V3 |
| GND | — | GND |

Parâmetros gráficos ativos:

- `PRA32_U2_ST7789_WIDTH = 284`
- `PRA32_U2_ST7789_HEIGHT = 76`
- `PRA32_U2_ST7789_ROTATION = 1` (paisagem)

## 1.2 Encoder (A/B + Click)

| Sinal Encoder | Macro no firmware | GPIO RP2350 Zero |
|---|---|---|
| Canal A (CLK) | `PRA32_U2_ENCODER_PIN_A` | **GP14** |
| Canal B (DT) | `PRA32_U2_ENCODER_PIN_B` | **GP15** |
| Botão (SW) | `PRA32_U2_ENCODER_SWITCH_PIN` | **GP21** |
| Comum | — | GND |

Configuração lógica no firmware:

- Modo de pino: `INPUT_PULLUP`
- Nível ativo: `LOW`
- Click curto e longo habilitados pelo módulo `pra32-u2-ui-input-encoder`

## 1.3 DAC I2S (PCM5102/PCM5102A)

| Sinal PCM5102 | Macro no firmware | GPIO RP2350 Zero |
|---|---|---|
| DIN | `PRA32_U2_I2S_DATA_PIN` | **GP9** |
| BCK / BCLK | `PRA32_U2_I2S_BCLK_PIN` | **GP10** |
| LCK / LRCK / WS | (derivado) | **GP11** *(BCLK+1)* |
| XSMT (mute off, opcional) | `PRA32_U2_I2S_DAC_MUTE_OFF_PIN` | **GP22** |
| SCK / MCLK | (não usado por padrão) | NC |
| VIN/VCC | — | 3V3 ou 5V (conforme módulo) |
| GND | — | GND |
| OUTL/OUTR/GND | — | Para estágio de saída (jack/mixer/amp) |

Parâmetros de áudio I2S ativos:

- `PRA32_U2_I2S_SWAP_BCLK_AND_LRCLK_PINS = false`
- `PRA32_U2_I2S_SWAP_LEFT_AND_RIGHT = false`
- `PRA32_U2_I2S_BUFFERS = 4`
- `PRA32_U2_I2S_BUFFER_WORDS = 64`

---

## 2) Instruções detalhadas de montagem

## 2.1 Preparação elétrica

1. Desligue toda a alimentação USB/externa.
2. Defina um único ponto de terra comum entre RP2350, display, encoder e DAC.
3. Se usar protoboard, mantenha cabos de I2S curtos (idealmente <= 20 cm) para reduzir ruído/jitter.

## 2.2 Montagem do display ST7789

1. Conecte VCC do display ao 3V3 da RP2350 Zero.
2. Conecte GND ao barramento de terra comum.
3. Faça as ligações SPI e controle:
   - SCK -> GP2
   - MOSI -> GP3
   - CS -> GP17
   - DC -> GP8
   - RST -> GP12
4. Backlight:
   - configuração simples: BL direto em 3V3;
   - opcional: BL em GPIO com resistor para controle de brilho (exige customização adicional no firmware).

## 2.3 Montagem do encoder

1. Conecte pino comum do encoder (C/GND) ao GND.
2. Conecte Canal A ao GP14.
3. Conecte Canal B ao GP15.
4. Conecte SW (push) ao GP21.
5. Como o firmware usa `INPUT_PULLUP`, **não** é necessário pull-up externo na maioria dos módulos.
6. Se houver ruído mecânico excessivo, melhore cabeamento/aterramento antes de alterar debounce no software.

## 2.4 Montagem do DAC PCM5102

1. Conecte GND do DAC ao GND comum.
2. Conecte DIN do DAC ao GP9.
3. Conecte BCK do DAC ao GP10.
4. Conecte LCK/LRCK/WS do DAC ao GP11.
5. Conecte XSMT ao GP22 (opcional, porém recomendado para reduzir click em operações de gravação).
6. Conecte saída analógica (L/R/GND) ao circuito de áudio final.
7. Mantenha trilhas/cabos de áudio analógico afastados das linhas digitais rápidas (BCK/DIN).

---

## 3) Configuração de build e placa

Para PlatformIO, o ambiente foi configurado para:

- `board = waveshare_rp2350_zero`
- framework Arduino (core Arduino-Pico já especificado no projeto)

Se estiver usando Arduino IDE:

1. Instale/atualize o core **Raspberry Pi Pico/RP2040/RP2350** (Earle Philhower).
2. Selecione a placa correspondente à RP2350 Zero (ou equivalente da família RP2350).
3. Garanta que o build use o caminho I2S padrão (PWM desabilitado).

---

## 4) Checklist de validação pós-montagem

1. Boot da placa sem aquecimento anormal.
2. Display inicializa e desenha UI corretamente (orientação horizontal).
3. Encoder navega páginas/itens e click curto/longo responde.
4. DAC reproduz áudio sem canal invertido.
5. Sem dropouts evidentes durante edição de parâmetros e operação normal.
6. Sem ações destrutivas acidentais (fluxos de confirmação continuam ativos na UI).

---

## 5) Observações práticas

- Alguns módulos PCM5102 aceitam 5V em VIN e regulam internamente; outros são estritamente 3V3 na lógica. Verifique sempre o datasheet/serigrafia do seu módulo.
- O mapeamento acima prioriza estabilidade com o firmware atual e evita alterações na semântica de parâmetros/sintetizador.
- Caso remapeie pinos, altere somente macros de configuração e valide novamente todo o checklist.
