# cube\_config\_linux

Please see the readme in the doc directory for more detailed usage of the SD card utilities. You can also run the executable with no arguments to see the help/usage menu. 

## Installation
```
git clone https://github.com/kemerelab/cube_config_linux.git
cd cube_config_linux
./compile_all.sh
```
## Example workflow

### Configuring SD card for recording
Find device name of the SD card:
```
dmesg | tail -n 10
```

On my machine, the SD card is /dev/sdc. It may be different for yours.

```
sudo ./write_config /dev/sdc config128.cfg
sudo ./card_enable /dev/sdc
```

Check the console output to confirm success

### Extracting the data
```
sudo ./sd_card_extract /dev/sdc install_06-21-2017_sd07.dat 
```

Other utilities such as read\_config and pcheck can be used to inspect the 
current configuration on the card and packet information, respectively
