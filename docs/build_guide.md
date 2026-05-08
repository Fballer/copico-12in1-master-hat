# Copico 10-in-1 Master Hat: Hardware Assembly Guide
**Date: May 6, 2026**

This guide provides the complete Bill of Materials (BOM) and step-by-step wiring instructions for building the **10-in-1 Master Hat** directly onto the **Centipede 32z** board. 

> [!IMPORTANT]
> This hardware is designed to work in tandem with the [Firmware Architecture Plan](file:///Users/macbook/.gemini/antigravity/brain/01be4bfb-ce62-4b38-8036-e57a2e46b2b8/firmware_architecture_plan.md).
> Ensure all power is disconnected from the Centipede and CoCo before soldering.

## Bill of Materials (BOM)

### Core Components
*   **[ ] 1x Centipede 32z Board**
*   **[ ] 2x 20-pin Female Headers (2x10, 2.54mm pitch)** (For the J3 and J4 slots)
*   **[ ] 1x MicroSD Card Module** ([Reference: CoCoSDC Blog](http://cocosdc.blogspot.com/))
*   **[ ] 1x PCM5102A I2S DAC Module** ([Reference: Orch-90 Docs](https://colorcomputerarchive.com/repo/Documents/Manuals/Hardware/Orchestra-90%20(Tandy).pdf))
*   **[ ] 1x TTL to RS-232 Module** (with MAX3232 chip)
*   **[ ] 1x DS3231 I2C Real-Time Clock (RTC) Module**
*   **[ ] 1x ESP32-C3 Super Mini Module** ([Reference: FujiNet Project](https://fujinet.online/))

### Connectors & Switches
*   **[ ] 1x HD15 Female VGA Connector** 
*   **[ ] 2x RCA Jacks** (Stereo out)
*   **[ ] 1x SPDT Toggle Switch** (Multi-Cart Mode Selection)
*   **[ ] 1x SPST Momentary Button** (CoCoSDC Disk Swap)
*   **[ ] 1x 5mm LED** (CoCoSDC Activity Status)

### Passive Components (Resistors & Capacitors)
*   **[ ] 2x 100Ω Resistors** (VGA Sync)
*   **[ ] 3x 470Ω Resistors** (VGA Red, Green, Blue)
*   **[ ] 2x 1kΩ Resistors** (Audio Mix)
*   **[ ] 1x 330Ω Resistor** (Status LED)
*   **[ ] 1x 10kΩ Trimpot** (Volume Control)
*   **[ ] 1x 10µF Capacitor** (Audio Coupling)
*   **[ ] 1x 1000µF Capacitor** (WiFi Power Stabilizer - **Solder to +5V Rail**)
*   **[ ] 1x 0.1µF Capacitor (104)** (Ceramic Noise Filter)

---

## The J3 and J4 Header Map

*   **J3 (Top Row)**: GPIO 8-19, 3.3V, 5V, GND.
*   **J4 (Middle Row)**: GPIO 20-34, CART, SND, ESP32 SPI, Interrupts.

---

## Step-by-Step Wiring Instructions

### Phase 1: Header Installation & Power Prep
1.  **Headers**: Solder 2x10 female headers into **J3** and **J4**.
2.  **DAC Clock**: Solder `SCK` on the PCM5102A module directly to its own `GND` pad.
3.  **Power Filter**: Solder the **1000µF Capacitor** across the **5V** and **GND** rails (J3-20 and J3-10).

### Phase 2: MicroSD Storage (CoCoSDC)
*   **Module**: 3.3V 6-pin MicroSD Breakout (J1).
*   **Wiring**:
    *   VCC ➔ 3.3V (J3-2) | GND ➔ GND
    *   MISO (DO) ➔ J4 Pin 12 (**G27**)
    *   SCK (CLK) ➔ J4 Pin 14 (**G29**)
    *   MOSI (DI) ➔ J4 Pin 16 (**G31**)
    *   CS ➔ J4 Pin 18 (**G34**)
*   **Indicators**: LED ➔ J4 Pin 5 (**G22**) | SDC Button ➔ J3 Pin 17 (**G19**)

### Phase 3: I2S DAC (Orchestra-90 / Speech)
*   **VIN** ➔ **J3 Pin 20 (+5V)** | GND ➔ GND
*   **BCK** ➔ J3 Pin 5 (**G10**)
*   **DIN** ➔ J3 Pin 7 (**G11**)
*   **LCK** ➔ J3 Pin 9 (**G12**)

### Phase 4: WordPak VGA (3-Bit RGB Color)
*   **GREEN** ➔ J3 Pin 1 (**G8**) @ 470Ω
*   **BLUE** ➔ J3 Pin 3 (**G9**) @ 470Ω
*   **RED** ➔ J3 Pin 12 (**G14**) @ 470Ω
*   **VSync** ➔ J3 Pin 14 (**G16**) @ 100Ω
*   **HSync** ➔ J3 Pin 16 (**G18**) @ 100Ω

### Phase 5: RS-232, RTC & WiFi
*   **RS-232**: TTL_TX ➔ J3 Pin 11 (**G13**) | TTL_RX ➔ J3 Pin 13 (**G15**)
*   **RTC (I2C)**: SDA ➔ J4 Pin 15 (**G30**) | SCL ➔ J4 Pin 17 (**G32**)
*   **WiFi (ESP32 SPI)**:
    *   MISO ➔ J4 Pin 7 (**G23**)
    *   MOSI ➔ J4 Pin 9 (**G24**)
    *   CS ➔ J4 Pin 11 (**G26**)
    *   SCK ➔ J4 Pin 13 (**G28**)

### Phase 6: Controls & Reset
1.  **BOOT_Switch**: J3 Pin 15 (**G17**) ➔ Toggle Switch ➔ GND.
2.  **Future Reset**: A pad is available on the PCB connected to **J3 Pin 18 (RESET)** for a future hard-wired link to the ESP32 EN pin. Initial reset is via software handshake.

### Phase 7: MIDI Expansion Header
Since full 5-pin DIN MIDI jacks and optoisolators are too large for the Hat, we provide a breakout header. You can plug a standard "Arduino MIDI Shield/Module" into this header later.
1.  **Hardware**: Place a 1x4 Male Header (2.54mm) on the PCB.
2.  **Wiring**:
    *   Pin 1 ➔ **3.3V** (Sourced directly from the main VCC rail between J3 and J4)
    *   Pin 2 ➔ GND
    *   Pin 3 ➔ **J4 Pin 3 (G21)** (MIDI TX)
    *   Pin 4 ➔ **J4 Pin 1 (G20)** (MIDI RX)

---
> [!TIP]
> **Check your work!** Continuity test 3.3V, 5V, and GND for shorts before power-on.
