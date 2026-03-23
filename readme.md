# SNEngine

![SNEngine SNES to PC Engine Adapter](media/snengine_logo_small.png)

**SNES to PCEngine controller adapter** *by RetroCityRansom (<www.youtube.com/@retrocityransom>)*

## About

<https://youtu.be/9fGX9Tb3N7Q>

When I was looking for a simple and affordable way to connect an SNES/SFC controller to my consolized MVS system, I came across Robin Edwards’ GitHub repository featuring the excellent “[SNES to NeoGeo](https://github.com/robinhedwards/SNES-to-NeoGeo) ” project. It builds on the groundwork laid by [another GitHub project by Anthony Burkholder](https://github.com/burks10/Arduino-SNES-Controller) — to give proper credit here. Putting the adapter together went really well, and it was surprisingly easy, even though I’m not particularly experienced with soldering. One especially nice touch is that Robin also provides files for a 3D-printable case to house the Arduino and the SNES female controller port. Luckily, my brother owns a 3D printer and kindly made a few of these cases for me.

Then one day, while playing on my PC Engine, I caught myself thinking, “Man, the controls are totally mushy”. The original PCE controllers are fine - in a way, but I personally prefer using a PS4, NES or SNES controller. They offer much tighter controls, I think.  There is an adapter for NES pads available ("NES2PCE"), but at the moment it costs around 35\$, plus shipping and import taxes, which adds up pretty quickly. It was mostly not in stock when I was looking, anyway. Another option are PCE Bluetooth adapters ("PCE BT", ~40\$) - I own two of them. Well, the input latency becomes noticeable when both BT adapters + controllers are connected — or when you hook up two or more controllers to a single adapter in multitap mode. For me, those adapters are only a good option when playing solo. Maybe I was using the existing solutions just wrong, but however ... With the recent experience of working on my own “SNES to NeoGeo” project still fresh in my mind, I decided to create something similar for the PC Engine and fit the electronics into the same case design from the SNES to NeoGeo project. I was looking forward to get some more practice soldering stuff together. So this is a small SNES-to-PCEngine converter (or SNES-to-Turbografx converter) project just for the fun of it.

## Features

- Implementation template for PC Engine and TurboGrafx-16. Optionally use a female NES port instead of an SNES port to connect an NES controller (without autofire, since the SNES shoulder buttons are not available); a different case would be required though
- Two to eight selectable button layouts for 2-button mode (depending on version)
- Autofire with adjustable speed
- Settings can be stored in persistent memory
- Multitap compatible (tested with two SNEngine adapters and one original CoreGrafx controller simultaneously)
- Pressing the equivalents for button I and II on the SNES controller overrides already pressed autofire buttons. Very useful for games like R-Type in which you want to have autofire and a charged shot at the same time.
- No noticeable input lag or audio glitches (at least in my testing).
- Works with the 8BitDo SN30 2.4G for SNES and the 8BitDo Retro Receiver for SNES -> allows use of wireless SNES or Bluetooth controllers; likely compatible with similar 8BitDo products for NES

## Currently not featured

- 6 button mode; I don’t use this myself, but if someone proposes a reasonable SNES button mapping, I can add support for it.
- Just a heads-up — this might not apply to your case: the more recent “green shell” 8BitDo SN30 controllers may come with a different 2.4 GHz adapter. In my case, that adapter seems to mix up the SNES button mapping. However, the controller works fine when connected using the 2.4 GHz adapter from the older grey version. Alternatively, you could adjust the code to handle the swapped mapping.

## Button mapping details

- SNEngine supports autofire. The default value is set to 30Hz, but you can switch between 30Hz and 15Hz by simultaneously pressing SELECT + UP.
- New since version 202603231-1: You can press SELECT+RIGHT to save your chosen autofire frequency and button mapping in persistent memory. This will be your new default setting from then on.
- The selectable mappings SNES => PCE are
  - since version 20260322-2
    - Layout A: B/X=I, L=Turbo I, A/Y=II, R=Turbo II
    - Layout B: B/X=II, L=Turbo II, A/Y=I, R=Turbo I
  - new in version 202603231-1
    - Layout C: B/L=I, Y=Turbo I, A/R=II, X=Turbo II
    - Layout D: B/L=II, Y=Turbo II, A/R=I, X=Turbo I
    - Layout E: X/L=I, B=Turbo I, Y/R=II, A=Turbo II
    - Layout F: X/L=II, B=Turbo II, Y/R=I, A=Turbo I
    - Layout G: X/L=I, Y=Turbo I, A/R=II, B=Turbo II
    - Layout H: X/L=II, Y=Turbo II, A/R=I, B=Turbo I (great for Salamander)

## Basic Principle

I've tried really hard to make it all work only with the Arduino, following the "SNES to NeoGeo" principle. But the PC Engine / Turbografx 16 has completely different requirements and the Ardu alone was not enough. The signal processing has to be done in such a fast way, that even the Arduino interrupt was overburdened. So I've had to add a much faster multiplexer (same type as used in the original controller) to the mix to cope with the fast switching SEL and OE processing from the PCE.
The rough layout sketch is like: SNES controller -> [ SNES controller female port -> Arduino -> Multiplexer -> Mini Din 8 cable (for PC Engine) / Din 8 (for TG16) ] -> PC Engine (or Turbografx 16). Please check out the diagrams provided below for reference.
Since wireless adapters may introduce voltage inconsistency, it’s best to place capacitors near the SNES port.

## Disclaimer

Use at your own risk. I am not an electrician and strongly advise against trying this without consulting a qualified professional and verifying the accuracy of all information provided in this repository.
The adapter has not been thoroughly tested yet. You may need to add resistors, capacitors or other electrical parts to ensure safe and error free operation.
The pictures provided here are not an implementation manual, but rather a personal photo diary.

## Shopping list, software & tools

| **Type** | **Optional** | **Part**                                                                     | **Remark**                                                                                                                                                                                                                                                                         |
| -------- | ------------ | ---------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Part     |              | Arduino Pro Mini                                                             | Split the SNES controller signal stream into discrete signals per button. Costs around 3€.                                                                                                                                                                                         |
| Part     |              | 74HC157 multiplexer (or "mux")                                               | Device fast enough to switch outputs the PCE’s SEL signal – the Arduino interrupt seems a little bit too slow. They cost me 30 cents each when bought in a pack of ten.                                                                                                            |
| Part     |              | Female SNES controller port                                                  | Guess … Should not cost more than 50 cents rather 35 cents.                                                                                                                                                                                                                        |
| Part     |              | Mini Din-8 cable (at least with one male plug)                               | To connect the multiplexer to the PCE. I got mine for under €8. These are also available with two male connectors, so you can build two adapters with just one cable.                                                                                                              |
| Part     |              | Some wires & heat shrink tubing                                              | I've used 28 AWG wires, 30 AWG should also be fine                                                                                                                                                                                                                                 |
| Part     | (Yes)        | 47uF electrolytic capcitor                                                   | Highly recommended! Mind the correct polarity! Use together with 100nF ceramic cap to smooth out electrical spikes, stabilize voltage and hence avoid sound glitches when using 2.4gHz 8BitDo adapters instead of wired SNES pad. Connect between GND and VCC of SNES female port. |
| Part     | (Yes)        | 100nF ceramic capcitor (type 104)                                            | Highly recommended! Use together with 47uF ceramic cap to smooth out electrical spikes, stabilize voltage and hence avoid sound glitches when using 2.4gHz 8BitDo adapters instead of wired SNES pad. Connect between GND and VCC of SNES female port.                             |
| Part     | Yes          | Screw terminal block (8 pin) connectors 2.54mm                               | Very convenient if you do not want to take the risk soldering the wrong cables to your Arduino. Any size will work, but two eight-terminal connectors are optimal. Using a larger one prevents the MUX from fitting under the Arduino, requiring a bigger case.                    |
| Part     | Yes          | A small Perfboard                                                            | I've cut mine 4x8. To mount/protect the multiplexer so that its pins do not bend, attach the wires to the underside.  If your case is significantly larger than that of the SNES-to-NEOGEO adapter, you can try mounting all the electronic components.                            |
| Part     | Yes          | Superglue                                                                    | To firmly attach the SNES port to the case. Best only glue the lower part of the case and the SNES port, so you can still open the case without breaking it.                                                                                                                       |
| Part     | Yes          | Adhesive Tape                                                                | Wrap around the perfboard and the multiplexer. Also wrap around the multiplexer and the Arduino to secure them. This will make it easier to position both components inside the small enclosure.                                                                                   |
| Part     | Yes          | The 3d printed SNES-TO-NEOGEO shell                                          | TO achieve greatness!!!                                                                                                                                                                                                                                                            |
| Part     | Yes          | 3mmx16mm screws                                                              | To assemble the shell. You could simply wrap some tape around it instead.                                                                                                                                                                                                          |
| Part     | Yes          | Cable/strain relief                                                          | I've got mine from another electrical case.                                                                                                                                                                                                                                        |
| Software |              | [SNEngine code](files/code)                                                  |                                                                                                                                                                                                                                                                                    |
| Software | Yes          | [SNEngine cheat sheet](files/SNEngine_Cheat_Sheet_Example.ods)               | A Libre Office Workook (should also work in Excel and Open Office) to keep track of wire colors, etc..                                                                                                                                                                             |
| Software |              | Arduino IDE                                                                  |                                                                                                                                                                                                                                                                                    |
| Software | Yes          | [Robin's 3d printing files](https://github.com/robinhedwards/SNES-to-NeoGeo) | For the case. Highly recommended.                                                                                                                                                                                                                                                  |
| Tool     |              | Soldering equipment                                                          | Guess ...                                                                                                                                                                                                                                                                          |
| Tool     |              | USB-to-TTL serial adapter, e.g. IDUINO FT232                                 | To program the Arduino                                                                                                                                                                                                                                                             |
| Tool     | Yes          | 3d printer                                                                   | If you want the case ...                                                                                                                                                                                                                                                           |

## SNES2PCE / SNES2TG16 pinout

- Multiplexer bank A: Action buttons
- Multiplexer bank B: D-Pad buttons

| SNES Pin | SNES Function       | PC Engine Function | Arduino In Pin | Arduino Out Pin | MUX In Pin | MUX Out Pin | PC Engine / TG Cable Pin | PC Engine / TG function                       |
| -------- | ------------------- | ------------------ | -------------- | --------------- | ---------- | ----------- | ------------------------ | --------------------------------------------- |
| 4        | Serial: B / X       | Button I           | A2             | D4              | 2 (1A)     | 4 (1Y)      | 2                        | Up / Button I                                 |
| 4        | Serial: D-Pad Up    | Up                 | A2             | D8              | 3 (1B)     | 4 (1Y)      | 2                        | Up / Button I                                 |
| 4        | Serial: A / Y       | Button II          | A2             | D5              | 5 (2A)     | 7 (2Y)      | 3                        | Right / Button II                             |
| 4        | Serial: D-Pad Right | Right              | A2             | D9              | 6 (2B)     | 7 (2Y)      | 3                        | Right / Button II                             |
| 4        | Serial: Start       | Run                | A2             | D6              | 11 (3A)    | 9 (3Y)      | 5                        | Left / Run                                    |
| 4        | Serial: D-Pad Left  | Left               | A2             | D10             | 10 (3B)    | 9 (3Y)      | 5                        | Left / Run                                    |
| 4        | Serial: Select      | Select             | A2             | D7              | 14 (4A)    | 12 (4Y)     | 4                        | Down / Select                                 |
| 4        | Serial: D-Pad Down  | Down               | A2             | D11             | 13 (4B)    | 12 (4Y)     | 4                        | Down / Select                                 |
| 4        | Serial: L-Shoulder  | Autofire I         | A2             | - (D4)          | -          | -           | - (2)                    | -                                             |
| 4        | Serial: R-Shoulder  | Autofire II        | A2             | - (D5)          | -          | -           | - (3)                    | -                                             |
| -        | -                   | -                  | -              | -               | 1          | -           | 6                        | Select Line (Direction/Action)                |
| -        | -                   | -                  | -              | -               | 15         | -           | 7                        | Output Enable (multitap controller switching) |
| 1        | VCC 5V+             | -                  | VCC            | -               | 16         | -           | 1                        | VCC 5V+                                       |
| 7        | GND                 | -                  | GND            | -               | 8          | -           | 8                        | GND                                           |
| 2        | Clock               | -                  | -              | A0              | 8          | -           | -                        | -                                             |
| 2        | Latch               | -                  | -              | A1              | 8          | -           | -                        | -                                             |

**Click the diagrams below to enlarge.**

[![SNEngine SNES to PC Engine Adapter](media/snengine_conn_ardu-mux_small.png)](media/snengine_conn_ardu-mux.png) [![SNEngine SNES to PC Engine Adapter](media/snengine_conn_ardu-pce_small.png)](media/snengine_conn_ardu-pce.png) [![SNEngine SNES to PC Engine Adapter](media/snengine_conn_snes-ardu_small.png)](media/snengine_conn_snes-ardu.png)

## Some photos

Here are some pictures to show, how I've tried to build the SNEngine adapter and fit everything into the SNES-TO-NEOGEO box. It was quite a bit of a pain, especially since I don’t have much experience with soldering and that DIY kind of thing, but the result was totally worth it. If you want to make things a bit easier, just modify the 3D printing files to create a larger case.

![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_29_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_20_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_21_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_22_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_23_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_25_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_31_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_32_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_33_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_34_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_35_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_36_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_37_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_40_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_41_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_42_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_44_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_45_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_46_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_47_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_48_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_49_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_50_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_51_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_52_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_53_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_54_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_55_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_56_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_57_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_60_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_61_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_63_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_64_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_65_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_69_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_70_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_71_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_72_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_73_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_74_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_93_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_96_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_97_small.jpg)
![SNEngine SNES to PC Engine Adapter](media/photos/snengine_assembly_98_small.jpg)
