import time
import fcntl
import os
import ctypes
import struct
# Initialize the ioctl constants
from ioctl import IOCTL

class StructFMT(ctypes.Structure):  
    _pack_ = 1  
    _fields_ = [('led_red',ctypes.c_char),('led_green',ctypes.c_char),('led_blue',ctypes.c_char)]  

IOCTL_MAGIC = ord('R')

class IOCTLRequest():
    IOCPARM_MASK = 0x3fff
    IOC_NONE = 0x20000000
    IOC_WRITE = 0x40000000
    IOC_READ = 0x80000000

    def FIX(self,x): 
        return struct.unpack("i", struct.pack("I", x))[0]
    def _IO(self,type,number): 
        return self.FIX(self.IOC_NONE|type<<8|number)
    def _IOR(self,type,number,size): 
        return self.FIX(self.IOC_READ|((size&self.IOCPARM_MASK)<<16)|type<<8|number)
    def _IOW(self,type,number,size): 
        return self.FIX(self.IOC_WRITE|((size&self.IOCPARM_MASK)<<16)|type<<8|number)
    def _IOWR(self,type,number,size): 
        return self.FIX(self.IOC_READ|self.IOC_WRITE|((size&self.IOCPARM_MASK)<<16)|type<<8|number)

def main():
    try:
        dev = os.open("/dev/recipedev", os.O_RDWR)
        _ioctl = IOCTLRequest()
        data = StructFMT(1, 1, 1)
        # data1 = bytes(data)
        SET_DATA = _ioctl._IOW(IOCTL_MAGIC, 2, ctypes.sizeof(data))
        # print(SET_DATA, " ", ctypes.sizeof(data))
        fcntl.ioctl(dev,SET_DATA,data)
        time.sleep(1)
        data = StructFMT(0, 0, 0)
        # data1 = bytes(data)
        SET_DATA = _ioctl._IOW(IOCTL_MAGIC, 2, ctypes.sizeof(data))
        # print(SET_DATA, " ", ctypes.sizeof(data))
        fcntl.ioctl(dev,SET_DATA,data)
        time.sleep(1)
        GET_DATA = _ioctl._IOR(IOCTL_MAGIC, 3, ctypes.sizeof(data))
        fcntl.ioctl(dev,GET_DATA,data)
        print("GET_DATA ledred = {}, ledgreen = {}, ledblue = {}".format(data.led_red, 
        data.led_green, data.led_blue))
        os.close(dev)
    except OSError as e:
        print(e.errno, "Fail to open device file: /dev/recipedev.")
        print(e.strerror)

if __name__ == "__main__":
    main()
