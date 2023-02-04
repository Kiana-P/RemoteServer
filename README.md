# Multi-threaded Server Program with Communication Protocols

 HOW TO RUN:
1. Open a terminal on your local computer or virtual machine and type make. This will run the Makefile.
2. Specify which port you want the server to run on, any free port should be fine. If you run the command netstat, you can get information on what ports are currently not in use, but usually any number between 5000 and 65536 will work.
3. The server is running!


HOW TO USE:
1. Open a new terminal and bind to the same port.
2. Run your client program. I am going to upload an example soon, but you can run any program that follows the communication protocols listed below.

COMMANDS:
1. GET key
2. SET key,value
3. DEL key

RESPONSE CODES:
1. OKS, OKG, OKD - successful
2. KNF
3. ERR BAD, ERR LEN, ERR SRV


Programming assignment for Systems Programming, Spring 2021.
