# udp2tcp-translator
Receives traffic on the UDP port, adds 4 bytes to the packet, and forwards it to the server via TCP.

## Build
* make

## Run
```./Usage: usd2tcp [-h] -l <log_file> -s <prefix> --udp_ip <udp_ip> --udp_port <udp_port> --tcp_ip <server_address> --tcp_port <server_port>```

## Check
Run TCP server

* ```./test -s <listen_ip> <listen_port>```

Generate traffic and send it to the UDP port:

* ```./test -g <udp2tcp_addr> <udp2tcp_port>```
