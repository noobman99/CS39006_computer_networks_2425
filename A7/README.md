# CLDP (Custom Lightweight Discovery Protocol)

## 1. Overview

CLDP is a custom network discovery protocol implemented using raw sockets. It enables nodes to announce their presence and query others for metadata (hostname, system time, and CPU load) within a closed network.

## 2. Files Included

- `cldp_server.c`: Implements the CLDP server, which listens for queries and responds with metadata.
- `cldp_client.c`: Implements the CLDP client, which sends queries and processes responses.
- `cldp_spec.pdf`: Detailed protocol specification document.
- `makefile`

## 3. Build Instructions

To compile the server and client, use the following commands:

```sh
gcc -o cldp_server cldp_server.c -Wall
gcc -o cldp_client cldp_client.c -Wall
```

OR

```sh
make
```

**Note**: Raw socket programs require root privileges. Run with `sudo`.

## 4. Running the Programs

### Start the Server

```sh
sudo ./cldp_server
```

### Run the Client

```sh
sudo ./cldp_client
```

## 5. Assumptions & Limitations

- Requires Linux with root privileges.
- Uses raw sockets (must be run with `sudo`).
- Limited to a closed network (not designed for public internet use).

## 6. Demo Output

Example output from the server:

```
CLDP Server starting up...
Listening for incoming packets...
Sent HELLO announcement
Sent HELLO announcement
Received QUERY from 192.168.137.39, trans_id: 1
Sent RESPONSE to 192.168.137.39
Sent HELLO announcement
Received QUERY from 192.168.137.39, trans_id: 2
Sent RESPONSE to 192.168.137.39
```

Example output from the client:

```
CLDP Client starting up...
Listening for server announcements...
No active servers to query
Added new server: 192.168.137.39
Sent QUERY to 192.168.137.39 (trans_id: 1)
Received RESPONSE from 192.168.137.39 (trans_id: 1): Hostname: parth-HP-Laptop-15s-fr1xxx, Time: 2025-03-31 18:04:31, CPU Load: 1.05%
Added new server: 192.168.137.87
Sent QUERY to 192.168.137.39 (trans_id: 2)
Sent QUERY to 192.168.137.87 (trans_id: 3)
Received RESPONSE from 192.168.137.39 (trans_id: 2): Hostname: parth-HP-Laptop-15s-fr1xxx, Time: 2025-03-31 18:04:41, CPU Load: 0.97%
Received RESPONSE from 192.168.137.87 (trans_id: 3): Hostname: acer-aspire-lite-52, Time: 2025-03-31 18:03:56, CPU Load: 0.98%
```

Output from TCP dump

```
tcpdump: data link type LINUX_SLL2
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on any, link-type LINUX_SLL2 (Linux cooked v2), snapshot length 262144 bytes
20:25:16.131148 lo    In  IP parth-HP-Laptop-15s-fr1xxx.mshome.net > parth-HP-Laptop-15s-fr1xxx.mshome.net:  exptest-253 8
        0x0000:  4500 001c ef55 0000 40fd f6ef c0a8 8927  E....U..@......'
        0x0010:  c0a8 8927 0200 0005 0000 0000            ...'........
20:25:16.131420 lo    In  IP parth-HP-Laptop-15s-fr1xxx.mshome.net > parth-HP-Laptop-15s-fr1xxx.mshome.net:  exptest-253 102
        0x0000:  4500 007a 730a 0000 40fd 72dd c0a8 8927  E..zs...@.r....'
        0x0010:  c0a8 8927 035e 0005 0000 0000 7061 7274  ...'.^......part
        0x0020:  682d 4850 2d4c 6170 746f 702d 3135 732d  h-HP-Laptop-15s-
        0x0030:  6672 3178 7878 0000 0000 0000 0000 0000  fr1xxx..........
        0x0040:  0000 0000 0000 0000 0000 0000 0000 0000  ................
        0x0050:  0000 0000 0000 0000 0000 0000 3230 3235  ............2025
        0x0060:  2d30 332d 3331 2032 303a 3235 3a31 3600  -03-31.20:25:16.
        0x0070:  302e 3735 0000 0000 0000                 0.75......
20:25:23.678357 wlo1  Out IP parth-HP-Laptop-15s-fr1xxx.mshome.net > 255.255.255.255:  exptest-253 8
        0x0000:  4500 001c 76d2 0000 40fd b943 c0a8 8927  E...v...@..C...'
        0x0010:  ffff ffff 0100 0000 0000 0000            ............
20:25:26.131721 lo    In  IP parth-HP-Laptop-15s-fr1xxx.mshome.net > parth-HP-Laptop-15s-fr1xxx.mshome.net:  exptest-253 8
        0x0000:  4500 001c 3a9e 0000 40fd aba7 c0a8 8927  E...:...@......'
        0x0010:  c0a8 8927 0200 0006 0000 0000            ...'........
20:25:26.132190 lo    In  IP parth-HP-Laptop-15s-fr1xxx.mshome.net > parth-HP-Laptop-15s-fr1xxx.mshome.net:  exptest-253 102
        0x0000:  4500 007a 597e 0000 40fd 8c69 c0a8 8927  E..zY~..@..i...'
        0x0010:  c0a8 8927 035e 0006 0000 0000 7061 7274  ...'.^......part
        0x0020:  682d 4850 2d4c 6170 746f 702d 3135 732d  h-HP-Laptop-15s-
        0x0030:  6672 3178 7878 0000 0000 0000 0000 0000  fr1xxx..........
        0x0040:  0000 0000 0000 0000 0000 0000 0000 0000  ................
        0x0050:  0000 0000 0000 0000 0000 0000 3230 3235  ............2025
        0x0060:  2d30 332d 3331 2032 303a 3235 3a32 3600  -03-31.20:25:26.
        0x0070:  302e 3738 0000 0000 0000                 0.78......
20:25:33.680511 wlo1  Out IP parth-HP-Laptop-15s-fr1xxx.mshome.net > 255.255.255.255:  exptest-253 8
        0x0000:  4500 001c 6e6b 0000 40fd c1aa c0a8 8927  E...nk..@......'
        0x0010:  ffff ffff 0100 0000 0000 0000            ............
20:25:36.132080 lo    In  IP parth-HP-Laptop-15s-fr1xxx.mshome.net > parth-HP-Laptop-15s-fr1xxx.mshome.net:  exptest-253 8
        0x0000:  4500 001c 21f5 0000 40fd c450 c0a8 8927  E...!...@..P...'
        0x0010:  c0a8 8927 0200 0007 0000 0000            ...'........
20:25:36.132399 lo    In  IP parth-HP-Laptop-15s-fr1xxx.mshome.net > parth-HP-Laptop-15s-fr1xxx.mshome.net:  exptest-253 102
        0x0000:  4500 007a 6d07 0000 40fd 78e0 c0a8 8927  E..zm...@.x....'
        0x0010:  c0a8 8927 035e 0007 0000 0000 7061 7274  ...'.^......part
        0x0020:  682d 4850 2d4c 6170 746f 702d 3135 732d  h-HP-Laptop-15s-
        0x0030:  6672 3178 7878 0000 0000 0000 0000 0000  fr1xxx..........
        0x0040:  0000 0000 0000 0000 0000 0000 0000 0000  ................
        0x0050:  0000 0000 0000 0000 0000 0000 3230 3235  ............2025
        0x0060:  2d30 332d 3331 2032 303a 3235 3a33 3600  -03-31.20:25:36.
        0x0070:  312e 3330 0000 0000 0000                 1.30......
```
