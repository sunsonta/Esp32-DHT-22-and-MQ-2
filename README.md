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

I use resistor 220 Ohm for each LED.

### Instructions

1. **Download AllCode**
   - Download the complete code and place it somewhere on your computer. You will need to maintain the folder structure to run the web server on Python.

2. **Add Library to Arduino**
   - Add the provided 7z file to your Arduino libraries for easy setup.

3. **Upload Arduino Code**
   - Upload the provided Arduino code from AllCode to your ESP32.
   - Edit the code to include your Wi-Fi SSID, password, and local IP address.

   ![image](https://github.com/user-attachments/assets/f0674956-c26b-4fe3-b256-90b9cefeb025)

4. **Edit Python Code**
   - Edit `webpython.py` to change the token for notifications, or leave it as is if not needed.

   ![image](https://github.com/user-attachments/assets/6f3ddd05-f835-4a04-a01b-29e8213af864)

5. **Change Python Port (Optional)**
   - You may change the port in the Python script if needed.

   ![image](https://github.com/user-attachments/assets/2308bfcc-c021-4481-92be-fac695d52703)

6. **Run Python Program**
   - Run the Python script, and you should see output similar to the image below (use this URL to see your web server).

   ![image](https://github.com/user-attachments/assets/66613c9c-1830-4a61-ab9f-a18fab092c9c)

7. **Completion**
   - Congratulations, you have successfully set everything up!

   ![image](https://github.com/user-attachments/assets/ba739e62-f810-4707-8cb5-4706ccd7947a)

8. **Change Language (Optional)**
   - You may need to change the `index` in the templates folder if you want the interface in English.

This is the adjusted README for the project.
