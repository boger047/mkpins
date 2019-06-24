# Program Make Pins (MKPINS)

## Function

Reads a comma-delimited file (prepared by a spreadsheet) containing the
pinout and signal names for a given embedded project (currently it works
on the Cortex M3 LPC17xx processor by NXP, but could easily be expanded
to other micros).

## Background

Back in mid-2000s, I found myself working on several different NXP ARM
processors, first some ARM7 flavors and later Cortex M3.  It was a pain
to manage all the I/O pin setups for each different design, especially
one project where I had to switch the design from an ARM7 to Cortex M3.
At the time, the manufacturers didn't provide tools to do this function
like are available today, or at least I wasn't aware of any.  So I
decided to write a simple processor that took pinout data that I
manually entered into a spreadsheet automatically generated the
necessary header and setup files.

I recently started a project using an ST Micro STM32F ARM Cortex M3
processor, so I will be updating this project to be flexible and support
different processors (or at least, different ARMs).

## Installation & Building

#### Files
  * `mkpins.c` source code
  * `Makefile` bare-bones make file
  * `pinout.xlsx` sample pinout spreadsheet
  * `pinout.csv` sample pinout csv made from xlsx
  * `doit.sh` script file to run the program
  * `zebra_gpio.c` example output header file
  * `zebra_gpio.h` example output code file

#### Build

Building is simple, just run `make` which compiles the one file. The
actual compilation command is just:
```bash
gcc mkpins.c -o mkpins
```

#### Running

Edit a spreadsheet to contain the design's pinout details.  See
`pinout.xlsx` as an example.  When done, save it as CSV file (for
example. `pinout.csv`)

Run the program to generate the pinout code and header files:
```bash
mkpins pinout.csv zebra
```
where the first argument is the CSV filename and the second argument is
the project name.  The project name will be used to generate all the
`#defines`, such as `ZEBRA_PINSEL0_INIT`.  

## To Do List

* Add mutli-processor support.

* Confirm compilation under Windows (I originally did this in Windows,
  migrated to Unix, and haven't recompiled in in Windows for some time).

* Clean up the code and documentation.

* Give control over the different kinds of output - not all of them are
  necessarily useful.

