# Speedometer-with-MSP430G2553
Speedormeter presenting the speed of bicycle using hall effect sensor and 16 character display

# Instructions
Instruction on how to connect LCD 16-Character display and hall effect sensor are inside the source file.

To compile on Linux:
```
msp430-gcc -mmcu=msp430g2553 speedometer.c
```
To program to uC
```
mspdebug rf2500
```
```
prog a.out
```
```
run
```
