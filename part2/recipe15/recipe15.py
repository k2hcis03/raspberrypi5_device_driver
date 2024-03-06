import time
import fcntl
import os
import signal

def handler(signo, frame):
    print("signumber :", signo)

def main():
    data = bytearray([1,2,3])
    signal.signal(signal.SIGIO, handler)
    dev = os.open('/dev/recipedev', os.O_RDWR)
    fcntl.fcntl(dev, fcntl.F_SETOWN, os.getpid())
    fcntl.fcntl(dev, fcntl.F_SETFL, fcntl.fcntl(dev, fcntl.F_GETFL)|fcntl.FASYNC)

    while True:
        os.write(dev, data)
        time.sleep(2)
        print("while loop")

if __name__ == "__main__":
    main()
