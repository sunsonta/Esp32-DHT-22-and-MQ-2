ESP32 for measuring gas and heat index.

![Topic Picture](Photo/Ex1.jpg)
| **Component** | **Pin on Module** | **Pin on Arduino** |
|---------------|------------------|--------------------|
| **Buzzer**    | VCC              | 3.3-5V             |
|               | GND              | GND                |
|               | I/O              | D13                |
| **DHT22**     | VCC              | 3.3V               |
|               | GND              | GND                |
|               | Data             | D4                 |
| **MQ-2**      | VCC              | 5V                 |
|               | GND              | GND                |
|               | Analog Out       | A35 (Pin 35)       |
| **LEDs**      | Green            | D13 (Pin 13)       |
|               | Yellow           | D12 (Pin 12)       |
|               | Red              | D14 (Pin 14)       |
| **Buzzer**    | Control Pin      | D16 (Pin 16)       |


I use resister 220 Ohm for each LEDs.


