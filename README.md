# rCPU
A minimal webserver for remote CPU monitoring (on Linux).  Everything is embedded in the executable, so you just need to run the program.  I'm planning to run it on my Raspberry Pi 2 to monitor the use of each core.  Although it should run on other Linux machines too.

The program will automatically adjust the display according to the number of CPU cores.

**UPDATE:** this latest version (February 2016) changes the graph plotting library from 
[Smoothie Charts](https://github.com/joewalnes/smoothie/) to [Flot]
(http://www.flotcharts.org/) in the hope of having better browser support.

**NOTE:** the code to display the core temperature is designed for the Raspberry Pi, it might not work on other machines.

###How to build and run

#####On the Raspberry Pi (and other Debian based systems probaby)
You should be able to do this:
```
git clone https://github.com/davidsblog/rCPU
cd rCPU/rCPU/
sudo make install
```
...which will build everything and install it as a service (it will run at system start-up).  **The server will run on port 8111.** That means you can view the graphs by visiting http://192.168.1.2:8111/ (you need to substitute your Raspberry Pi's IP address). You can remove it from your system like this (assuming you are still in the `rCPU/rCPU/` directory):
```
sudo make uninstall
```

**NOTE:** the `sudo` before calling make above is important, since you're installing services.

#####Runing manually (or on different Linux versions)
Just do this:

```
git clone https://github.com/davidsblog/rCPU
cd rCPU/rCPU/
make
sudo ./rcpu 80
```

You might have a webserver already running on port 80, in which case you can specify a different port by passing a different parameter than **80** in the last line above.

###Embedded content
The html and javascript files are embedded in the executable.  Each resource file becomes a C header file.  The makefile updates the header files as dependencies during the build process.

###Thanks

I am using the following javascript libraries:
- https://jquery.com/ jQuery (v2.1.0)
- http://www.flotcharts.org/ Flot (v0.8.3) 

Thanks to all involved with those projects.
