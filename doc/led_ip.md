# LED controller IP

The LED controller IP provides an AXI interface for a controller than can drive up to 4 LEDs.
There are a total of two registers, one to select the mode of operation and another that is used to control the LED behaviour depending on the mode.
The 4 LEDs are controlled individually.

## 1. Register mapping

### 1.1. MODE register (offset 0)

Bits | 31-8 | 7-6 | 5-4 | 3-2 | 1-0
---|---|---|---|---|---
Mode | Reserved | LED3 | LED2 | LED1 | LED0

### 1.2. VALUE register (offset 4)

Bits | 31-24 | 23-16 | 15-8 | 7-0
---|---|---|---|---
Value | LED3 | LED2 | LED1 | LED0

## 2. Modes of operation
There are a total of 4 modes of operation for each LED that can be configured through the first register (offset 0):

Mode | Name | Description
------|------|------------
0 | On/Off | Turn on or off the LED by setting the bit zero of **VALUE** register.
1 | Timed | The LED will turn on when bit zero of **VALUE** register is set and will go off after a fixed time period.
2 | Pulse | A variable PWM drives the LED, making it pulse. The pulsing speed is adjusting by setting the **VALUE** register.
3 | Dim | A PWM signal is sent to the LED, dimming it. The dimming factor can be adjusted by setting the **VALUE** register.

## 3. Examples

The following examples consider an instantiated LED controller IP with a base AXI address of 0x1000_0000.

### 3.1. Configure LED0 to ON/OFF mode and turn it ON and then OFF

```
xsdb% mwr 0x10000000 0
xsdb% mwr 0x10000004 0
xsdb% mwr 0x10000004 0
```

### 3.2. Set LED0 to a pulsing pattern

```
xsdb% mwr 0x10000000 2
xsdb% mwr 0x10000004 0xF
```

### 3.3. Set LED0 to a constant dim level

```
xsdb% mwr 0x10000000 3
xsdb% mwr 0x10000004 0x8
```

### 3.4. Let all LEDs pulse at different speeds
```
xsdb% mwr 0x10000000 0x55
xsdb% mwr 0x10000004 0x40100804
```

# 4. Revision log

* Revision 0.1:
    - First release, needs to be tested.

