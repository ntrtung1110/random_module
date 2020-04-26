# Random number module

## Overview

Small loadable module for generating random number from kernel mode.

Folder structure:

```
└── device_driver
    ├── Kbuild
    ├── Makefile
    ├── vchar_driver.c
    ├── vchar_driver.h
└── user_test
    ├── Makefile
    ├── user_test.c
```

## Usage

### Environment setup

```
# install supported tools
sudo apt-get install build-essentials
# download repo
git clone https://github.com/ntrtung1110/random_module.git
```
### Attached character driver to kernel
```
cd random_module/device_driver
# make file
make 
sudo insmod vchar_driver.ko
# check state of loadable module (optional)
dmesg
```
### System call from user space
Refer to ```user_test/user_test.c``` for example of using  ```read``` system call to transfer generated random number from kernel space to user space

Test user space sample code:

```
# make file
make 

# run application(sudo must be specified in case of os blocking of read device file)
(sudo) ./user_test 
```

Screenshot of returned results

<img src="/demo_image/demo.png" width=800 height=300 />

