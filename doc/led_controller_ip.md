# LED controller IP

The LED controller IP provides an AXI interface for a controller than can drive up to 16 LEDs. The IP can be configured with system designer where the total number of LED interfaces can be defined and if their activation polarity is reversed.

The number of available registers will depend on the total number of LED interfaces and each interface will have a register to select the mode (**MODE** register) and another to define the factor (**FACTOR** register).



## 1. Register mapping

### 1.1. MODE register (offset 0)

Bits | 31-2 | 1-0
---|---|---
Mode | - | MODE

### 1.2. VALUE register (offset 4)

Bits | 31-8 | 7-0
---|---|---
Value | - | LED0

## 2. Modes of operation
There are a total of 3 modes of operation for each LED that can be configured through the MODE register (offset 0):

Mode | Name | Description
------|------|------------
0 | On/Off | Turn on or off the LED by setting the bit zero of **VALUE** register.
1 | Pulse | A variable PWM drives the LED, making it pulse. The pulsing speed is adjusting by setting the **VALUE** register.
2 | Dim | A PWM signal is sent to the LED, dimming it. The dimming factor can be adjusted by setting the **VALUE** register.

## 3. Examples

```c
LED_Controller led_intf[2];
int i = 0, k;
u8 state[6] = {0};

LED_Controller *target_ctrl;
u32 target_intf;
u32 mode = 0;

LED_Controller_Initialize(&(led_intf[0]), 0);
LED_Controller_Initialize(&(led_intf[1]), 1);

while (1) {
    for (i = 0; i < 6; i++) {
        if (i < 2) {
            target_ctrl = &(led_intf[0]);
            target_intf = i;
        } else {
            target_ctrl = &(led_intf[1]);
            target_intf = i - 2;
        }

        if (mode == 0) {
            LED_Controller_SetOnOff(target_ctrl, target_intf, state[i]);
            state[i] = (state[i]!=0?0:1);
        } else if (mode == 1) {
            LED_Controller_Pulse(target_ctrl, target_intf, LED_PULSE_NORMAL);
        } else {
            LED_Controller_Dim(target_ctrl, target_intf, LED_DIM_50);
        }


        if (i == 5) {
            mode++;
            if (mode > 2) mode = 0;
        }

        usleep(200000);
    }
}
```

# 4. Revision log

* Revision 0.1:
    - First release, needs to be tested.
* Revision 1.0:
    - Driver name changed from **led** to **led_controller**.
    - C driver and BSP generation scripts added to the IP.
