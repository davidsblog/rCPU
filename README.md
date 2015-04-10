# rCPU
A minimal webserver for remote CPU monitoring.  Everything is embedded in the executable, so you just need to run the program.  I'm planning to run it on my Raspberry Pi 2 to monitor the use of each core.  It should run on other Linux machines too.

The program will automatically adjust the display according to the numer of CPU cores.

###How to build and run
```
git clone https://github.com/davidsblog/rCPU
cd rCPU/rCPU/
make
sudo ./rcpu 80
```

**NOTE:** the code to display the core temperature is designed for the Raspberry Pi, it might not work on other machines.
