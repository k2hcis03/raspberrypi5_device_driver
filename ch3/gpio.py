from gpiozero import LED
import time

led = LED(17)

while True:
    led.on()
    led.off()
