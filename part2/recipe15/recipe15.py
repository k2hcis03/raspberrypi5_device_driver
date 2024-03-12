import time
import os
import signal

SIGNUM = 50

def handler(signo, frame):
    print("signumber :", signo)

def main():
    data = bytearray([1,2,3])
    signal.signal(SIGNUM, handler)
    dev = os.open('/dev/recipedev', os.O_RDWR)

    while True:
        os.write(dev, data)
        time.sleep(2)
        print("while loop")

if __name__ == "__main__":
    main()
