# This is driver to use xdma as a lib for dma engine

# How to build and install

make 

sudo insmod xdma_test.ko

cd /sys/bus/pci/drivers/xdma_test

sudo su

### assume VID is the Vendor ID, DID is the Device ID

echo "VID DID" > new_id

# How to test
sudo su

echo 1 > /dev/debug

### write to /dev/debug will trigger dma to device

Then , we can read from device to testify
