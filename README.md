# Project Description
This is a class project for my operating systems course. We were tasked with creating a Unix shell in C. The shell needed to support a few custom commands such as exit, kill, jobs, and bg. It also needed to support all builtin unix commands and run those commands both in the foreground and background, and be able to switch a command between the two of those. The core concepts for this project were pipes and processes and how we can manage those, so my implementation uses both. 

# How to run
To run this program simply download main.c and Makefile into the same directory. Then navigate to that directory in a terminal window. Once there type "make" this compiles the code into the executable ./shell352. The program will then simulate the unix shell in your terminal window. To exit type "exit" and the program will end. Note: This code has been tested and run on a MacOS and Linux environment but not Windows.
