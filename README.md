# Unix-Shell
Build a preliminary command-line interface. Support user command and batch mode


## Commmand Mode 
For instance, when user enter the main program
...
./wish
>
...

This will prompt user inputs 

## Batch Mode
The user could designate files that contains commands that could be fetched into the shell

...
./wish commands.txt
...

## Add New Features
Now the shell support redirection and if statements 
For example

...
ls -la /tmp > output
...

Will redirect the output to tmp 

...
if a = 1 then ls > output 
...
Suppose a is a program, the shell will evalute the condition first in order to finish the remaining part. 
