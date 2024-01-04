# This is driver to use xdma as a lib for dma engine

# How to build and install

make 

sudo insmod nupanet.ko

cd /sys/bus/pci/drivers/nupanet

sudo su

### assume VID is the Vendor ID, DID is the Device ID

echo "VID DID" > new_id

# How to test
ls /dev/
/dev/debug char device is generated for user to read/write dma device
just run user_debug_test in super user mode
sudo ./user_debug_test

### write to /dev/debug will trigger dma to device

Then , we can read from device to testify
