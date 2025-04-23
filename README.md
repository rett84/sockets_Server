# Overview

Multi-threaded TCP sockets server, using linked lists to manage client connections. The server receives client data packets and writes them into a file in the /tmp folder.\
The server uses a mutex lock to prevent overwriting data from parallel requests. 
