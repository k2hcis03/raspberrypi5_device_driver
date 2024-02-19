from gpiozero import LED
from gpiozero import Button

import time

led = LED(17, initial_value=False)
btn = Button(5, pull_up = True)

def pressed():
    led.on()
    print("button was pressed")


btn.when_pressed = pressed

while(True):
    time.sleep(0)
