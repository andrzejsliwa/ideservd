# ideservd verion 0.32a

## Installation on MacOSX
  
Install Libs:

  $ brew install libftdi
  $ brew instsll libusb
  
Install: FTDIUSBSerialDriver_v2_4_4.dmg
  
Build Project:
  
  $ cd src
  $ make -f Makefile.macos
  
## Test 

  Verify output:

  $ ./ideservd
  ideservd 0.32A 20201219
  Using USB driver for device *
  ftdi_usb_open_dev: unable to claim usb device. Make sure the default FTDI driver is not in use
  
If you see such message please verify USB drivers:

  kextstat | grep -i ftdi
  158    0 0xffffff7f846a5000 0x7000     0x7000     com.FTDI.driver.FTDIUSBSerialDriver (2.4.4) B137605C-32D8-3E81-89B6-E1F8039FC427 <88 62 6 5 3 1>
  
Stop driver:

  sudo kextunload -bundle-id com.FTDI.driver.FTDIUSBSerialDriver
  
Run ideservd again:

  $ ./ideservd
  ideservd 0.32A 20201219
  Using USB driver for device *
  Served device: A4YR7X3Z (IDE64 USB DEVICE)
  
Now its working correctly.  
