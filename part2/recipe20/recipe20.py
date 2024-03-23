import fcntl
import os
import ctypes
import struct
# Initialize the ioctl constants

class StructIOCTL(ctypes.Structure):  
    _pack_ = 1  
    _fields_ = [('pwm_on_time_ns',ctypes.c_int),('pwm_period_ns',ctypes.c_int)]  

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
        data = StructIOCTL(5000, 20000)

        while True:
            data.pwm_on_time_ns = int(input("pwm_duty_ns: "))
            data.pwm_period_ns = int(input("pwm_period_ns: "))
            
            if data.pwm_period_ns == 0:
                break
            SET_DATA = _ioctl._IOW(IOCTL_MAGIC, 2, ctypes.sizeof(data))
            fcntl.ioctl(dev,SET_DATA,data)
        os.close(dev)
    except OSError as e:
        print(e.errno, "Fail to open device file: /dev/recipedev.")
        print(e.strerror)

if __name__ == "__main__":
    main()
