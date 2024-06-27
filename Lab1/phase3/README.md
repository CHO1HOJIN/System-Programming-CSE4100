20201654 최호진
# project2: phase1
## Configuration file
### csapp.h
### csapp.c
### myshell.c
### Makefile

# Compile and execute the program
### enter the command 'make' to compile and enter the command './myshell' to execute the shell

# Myshell Command (Phase 1)
## cd: change the current directory
### (option)
#### '~': go to home directory
#### '../{directory name}/': relatvie paths
#### '/home/{dir}': absolute pa
#### ls: print the file in directory
#### mkdir: make new directoy
#### rmdir: remove the directory
#### touch: make new file
#### cat: read the file 
#### echo: print the input text
#### exit: exit the shell

# New features extended in the phase2
## pipeline 
#### ex) ls | grep filename
#### cat filename | less
#### cat filename | grep -i "abc" | sort -r

# New feautres and command extended in the phaes3
### 1) run processes in background : Enter the '&' to the end of command
### 2) Command
#### jobs: print the list of background process data(job, state, cmdline)
#### bg %{job_id}: Change astopped background process to a running background process.
#### fg %{job_id}: Change a background process to a running in the foreground process.
#### kill %{job_id}: Terminate a process.
#### Ctrl+C: terminate the current foreground process.
#### Ctrl+Z: stopped the current foreground process and add to background process job list.
