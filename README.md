# Project Description
This is a class project for my operating systems course. We were tasked with creating a Unix shell in C. The shell needed to support a few custom commands such as exit, kill, jobs, and bg. It also needed to support all builtin unix commands and run those commands both in the foreground and background, and be able to switch a command between the two of those. The core concepts for this project were pipes and processes and how we can manage those, so my implementation uses both. 

# How to run
To run this program simply download main.c and Makefile into the same directory. Then navigate to that directory in a terminal window. Once there type "make" this compiles the code into the executable ./shell352. The program will then simulate the unix shell in your terminal window. To exit type "exit" and the program will end. Note: This code has been tested and run on a MacOS and Linux environment but not Windows.

# Examples
To see some functionality here are some commands you can run!
* 352> sleep 10
* This will cause the shell to wait for 10 seconds
* 352> sleep 10 &
* This lets the shell operate the sleep function in the background, sometimes an extra newline or other input is necessary for the "finished" message to appear
* 352> sleep 10 &
* 352> sleep 15 &
* 352> jobs
* (Wait 10 seconds from first sleep command)
* 352> jobs
* This executes two sleep commands in the background, then use the jobs command to see a list of all currently executing jobs, so if you run jobs right away after running both sleeps, you should see both sleeps and which job they are, if you then wait 10 seconds for when the first sleep should finish and run jobs again you will see only the 15 second sleep remaining in the list.

# Commands
There are many other combinations of commands, feel free to try things out and see how they work, to make it easier here are a few symbols and commands that can be run on this shell:
* Most builtin unix commands
* Use & at the end of a command to run it in the background
* Use | between two commands to pipe output from one as input to another command
* Use > to redirect output from stdout to a specified file
* Use < to redirect input from stdin to a specified file
* Use exit to stop the shell
* Use kill <processNumber> to stop a running background command (Try doing this on a command that isnt running or that you already stopped!) the process number is given when a command is run, the format [#] #### means that the number in brackets is the job number, and the number following that is the process number, use the number not in brackets to kill a process
* Use jobs to display a list of currently running or stopped commands
* To stop a command it must be executing in the foreground then press control+z, this stops it (note: if you do this on a sleep command it will continue counting as a quirk of the unix shell)
* Finally to resume a stopped command, run bg <processNumber>

# Last notes
* The output sometimes will not pop up right away as a nature from the main loop, enter another line or command to see the ouput of things like job finished/terminated
* Custom commands that require a process number will cause a segfault when running without a process number

# Citation
* The code is not completely mine, some was produced and given to me by my professor. The code with extensive comments is mine, the main parts not written by me were the parsing of the input and parts of the main loop, though most of the main function did end up being mine.
