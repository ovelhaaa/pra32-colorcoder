# PlatformIO Migration Baseline Checklist (Lock Before `platformio.ini`)

Date locked: **2026-04-27**

Purpose: capture the current Arduino IDE baseline so initial PlatformIO bring-up does not silently change synth/UI/audio behavior.

## 1) Toolchain/library baseline (documented known-good)

- [x] Arduino-Pico core: **5.5.1** (documented known-good in README).
- [x] MIDI library (`FortySevenEffects/arduino_midi_library`): **5.0.2** (documented known-good in README).
- [x] USB stack expectation: **Adafruit TinyUSB** (Arduino IDE Tools menu assumption; also reflected in source comments).

## 2) Source-of-truth feature toggles from `Digital-Synth-PRA32-U2.ino`

### MIDI transport toggles

- [x] `PRA32_U2_USE_USB_MIDI` = **enabled**.
- [x] `PRA32_U2_USE_UART_MIDI` = **disabled** (commented out).
- [x] `PRA32_U2_DISABLE_USB_MIDI_TRANSMITTION` = **enabled**.
- [x] `PRA32_U2_UART_MIDI_SPEED` = **31250**.
- [x] `PRA32_U2_UART_MIDI_SERIAL` = `Serial2`.
- [x] `PRA32_U2_UART_MIDI_TX_PIN` = **4**.
- [x] `PRA32_U2_UART_MIDI_RX_PIN` = **5**.

### Audio backend toggles

- [x] `PRA32_U2_USE_PWM_AUDIO_INSTEAD_OF_I2S` = **disabled** (I2S path is active).
- [x] I2S active pins/options:
  - `PRA32_U2_I2S_DATA_PIN` = **9**
  - `PRA32_U2_I2S_BCLK_PIN` = **10** (LRCLK = BCLK + 1)
  - `PRA32_U2_I2S_SWAP_BCLK_AND_LRCLK_PINS` = **false**
  - `PRA32_U2_I2S_SWAP_LEFT_AND_RIGHT` = **false**
  - `PRA32_U2_I2S_BUFFERS` = **4**
  - `PRA32_U2_I2S_BUFFER_WORDS` = **64**
  - `PRA32_U2_I2S_DAC_MUTE_OFF_PIN` = **22**
- [x] PWM fallback pins kept defined (inactive unless toggle enabled):
  - `PRA32_U2_PWM_AUDIO_L_PIN` = **28**
  - `PRA32_U2_PWM_AUDIO_R_PIN` = **27**

### Panel/input/display toggles

- [x] `PRA32_U2_USE_CONTROL_PANEL` = **enabled**.
- [x] `PRA32_U2_USE_CONTROL_PANEL_KEY_INPUT` = **enabled**.
- [x] `PRA32_U2_USE_CONTROL_PANEL_ENCODER_INPUT` = **enabled**.
- [x] `PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT` = **enabled** (legacy fallback path remains compiled).
- [x] `PRA32_U2_USE_CONTROL_PANEL_OLED_DISPLAY` = **disabled** (commented out).
- [x] `PRA32_U2_USE_CONTROL_PANEL_ST7789_DISPLAY` = **enabled**.

### Encoder constants (single-encoder UI path)

- [x] `PRA32_U2_ENCODER_PIN_A` = **14**
- [x] `PRA32_U2_ENCODER_PIN_B` = **15**
- [x] `PRA32_U2_ENCODER_SWITCH_PIN` = **21**
- [x] `PRA32_U2_ENCODER_ACTIVE_LEVEL` = **LOW**
- [x] `PRA32_U2_ENCODER_PIN_MODE` = **INPUT_PULLUP**
- [x] `PRA32_U2_ENCODER_DEBOUNCE_TICKS` = **15**
- [x] `PRA32_U2_ENCODER_LONG_PRESS_TICKS` = **375**

### ST7789 constants (new color panel)

- [x] `PRA32_U2_ST7789_WIDTH` = **284**
- [x] `PRA32_U2_ST7789_HEIGHT` = **76**
- [x] `PRA32_U2_ST7789_ROTATION` = **1**
- [x] `PRA32_U2_ST7789_PIN_SCK` = **2**
- [x] `PRA32_U2_ST7789_PIN_MOSI` = **3**
- [x] `PRA32_U2_ST7789_PIN_CS` = **17**
- [x] `PRA32_U2_ST7789_PIN_DC` = **8**
- [x] `PRA32_U2_ST7789_PIN_RST` = **12**

## 3) Pre-`platformio.ini` review gate

- [x] Baseline captured before adding or modifying `platformio.ini`.
- [x] Review intent: any PlatformIO defaults must be checked against this list to avoid behavior drift.

## 4) Immediate follow-up guardrails for first PlatformIO PR

- Keep USB MIDI enabled and TinyUSB-backed behavior equivalent.
- Keep I2S as default active audio backend (PWM remains opt-in fallback only).
- Keep control panel + encoder + ST7789 toggles unchanged.
- Keep currently-defined pin mappings unchanged unless explicitly requested and reviewed.
