# rCPU
A minimal webserver for remote CPU monitoring.  Everything is embedded in the executable, so you just need to run the program.  I'm planning to run it on my Raspberry Pi 2 to monitor the use of each core.  It should run on other Linux machines too.

The program will automatically adjust the display according to the numer of CPU cores.

**NOTE:** the code to display the core temperature is designed for the Raspberry Pi, it might not work on other machines.

###How to build and run
```
git clone https://github.com/davidsblog/rCPU
cd rCPU/rCPU/
make
sudo ./rcpu 80
```

###Changing the embedded content
The html and javascript files are embedded in the executable.  If you wish to change some of the html or javascript, you can edit them and then rebuild. The process is as follows:
```
make res
make
```

Each resource file becomes a C header file.  The `make res` command updates the header files and then you can just rebuild as normal using `make`.
