import time  
import fcntl  
import mmap  
import time  
import os
import struct
import ctypes
import threading
import ctypes
import math

DATACNT = 201
FREQ = 50
SAMPLING = 0.0001 

class StructFMT(ctypes.Structure):  
    _pack_ = 1  
    _fields_ = [('frequency', ctypes.c_int), ('data',ctypes.c_uint16*DATACNT)]  

def main():  
    try: 
        dev = os.open("/dev/recipedev", os.O_RDWR)
        data = StructFMT()
        data.frequency = 1      # ms 단위
        for x in range(DATACNT):
            data.data[x] = int(1000*(math.sin(2*math.pi*FREQ*SAMPLING*x)+1))
            #print(data.data[x])
        os.write(dev, data)
        os.close(dev)  
    except OSError as e:  
        print(e.errno, "Fail to open device file: /dev/recipedev.")  
        print(e.strerror)  
        os.close(dev)      
if __name__ == "__main__":  
    main()  
