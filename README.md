# VE489 Project

Final project for VE489 SU2020



## Requirements

Ubuntu Linux system, Mininet (see project description for details)



## Part 2 - TCP File Transfer Server

For convenience, in the following parts, I actually share the `finalproject` folder between my windows host and guest Linux OS in VirtualBox.



### Compile and run

The examples given in the following steps assume the server runs at 10.0.0.1:8001 (h1:8001), and client runs at 10.0.0.2:8002 (h2:8002).

1. Compile using `Makefile`, so that `ftrans` is generated

2. Launch the Mininet network topology `p1_topo.py`

   ```shell
   sudo python p1_topo.py
   ```

3. Open two terminals for server and client, respectively

   For example, 

   ```shell
   xterm h1 h2
   ```

4. Run the server in the current directory

   ```shell
   ./ftrans -s -p <port>
   ```

   For example,

   ```shell
   ./ftrans -s -p 8001
   ```

5. Create a new directory for client and copy `ftrans` into it

   For example, 

   ```shell
   mkdir client
   ```

6. (Optional; For writing the report)

   Capture the packets sent and received on client with `tcpdump` and dumping the output into a `.pcap` file

   ```shell
   tcpdump tcp port 8002 -s 65535 -w <filename>.pcap
   ```

   Then open the generated `.pcap` file in Wireshark

7. Run the client in the `client/` directory

   ```shell
   ./ftrans -c -h <server's IP/hostname> -sp <server port> -f <filename> -cp <client port>
   ```

   For example,

   ```shell
   ./ftrans -c -h 10.0.0.1 -sp 8001 -f 10MB.txt -cp 8002
   ./ftrans -c -h 10.0.0.1 -sp 8001 -f file3.jpg -cp 8002
   ```

8. Check the difference between delivered and original files. They are expected to be the same.

   For example,

   ```shell
   diff 10MB.txt client/10MB.txt
   ```

   

### Test files

In this project, test files with different sizes are needed. For instance, the following command could generate a test txt file whose size is about 1000KB.

```shell
cat /dev/urandom | sed 's/[^a-zA-Z0-9]//g'| strings -n 5 | head -n 160000 > 1000KB.txt
```

To know the exact file size, windows users may just check the file properties, or use

```shell
wc -c <filename>
```

in Linux to see how many bytes the `filename` contains.



## Part 3 - Reliable Transmission

The 3 subparts in this part share the same format for running. Examples here assume the sender runs at 10.0.0.1:8001 (h1:8001), receiver runs at 10.0.0.2:8002 (h2:8002), and a man-in-the-middle proxy runs at 10.0.0.3:8003 (h3:8003). More details could be found in the project description.



1. Compile using `Makefile`, so that `sr` and `mitm` are generated

2. Launch the Mininet network topology `normal_topo.py`

   ```shell
   sudo python normal_topo.py
   ```

3. Open three terminals for server, client and the man-in-the-middle proxy, respectively

   ```shell
   xterm h1 h2 h3
   ```

4. Run the man-in-middle proxy

   ```shell
   ./mitm <shuffle> <loss> <error> <receiver's ip> <receiver's port> <sender's ip> <sender's port>
   ```

   For example, to simulate the case of *reordering, 5% loss, 5% error*

   ```shell
   ./mitm 1 5 5 10.0.0.2 8002 10.0.0.1 8001
   ```

5. Create a new directory for receiver and copy `sr` into it

   For example, 

   ```shell
   mkdir recv
   ```

   Change directory to `recv/`, and create a new directory for the output delivered files

   For example,

   ```shell
   mkdir recv_dir
   ```

6. Run the receiver in the `recv/` directory

   ```shell
   ./sr -r <port> <window size> <recv dir> <log file>
   ```

   For example,

   ```shell
   ./sr -r 8002 10 recv_dir/ recv.log
   ```

7. Run the sender in the original directory

   ```shell
   ./sr -s <receiver's IP> <receiver's port> <sender's port> <window size> <file to send> <log file>
   ```

   For example,

   ```shell
   ./sr -s 10.0.0.3 8003 8001 10 10MB.txt send.log
   ```

   In this case, sender actually transmit files to `mitm`, and `mitm` relay packets between sender and receiver.

   In addition, to measure the time on the sender side, use

   ```shell
   time ./sr -s 10.0.0.2 8002 8001 10 file3.jpg send.log
   ```

   and find the `real` time printed on the terminal.

8. Check the difference between delivered and original files. They are expected to be the same.

   For example,

   ```shell
   diff 10MB.txt recv/recv_dir/file_0.txt
   diff file3.jpg recv/recv_dir/file_1.txt
   ```

   Additionally, view the `send.log` and `recv.log` file to see the header information for checking the correctness.



## Directory structure

```
.
├── README.md
├── part2
│   ├── 100B.txt
│   ├── 100KB.txt
│   ├── 10MB.txt               // test files 
│   ├── Makefile
│   ├── dump.pcap              // output file for tcpdump 
│   ├── file3.jpg              // test file 
│   ├── ftrans.cc              // implemention of the TCP file transfer server
│   └── p1_topo.py             // Mininet network topology used in this part 
├── part3
│   ├── part3-1
│   │   ├── 1000KB.txt
│   │   ├── 100KB.txt
│   │   ├── 10MB.txt           // test files  
│   │   ├── Makefile
│   │   ├── SRHeader.h         // definition of SRHeader used in sr.c   
│   │   ├── crc32.h            // crc32 function used in sr.c    
│   │   ├── file3.jpg          // test file
│   │   ├── lossy_topo.py
│   │   ├── mitm.c             // man-in-the-middle (MITM) written by TA 
│   │   ├── normal_topo.py     // Mininet network topology used in this part
│   │   └── sr.c               // implementation of the simple SR
│   ├── part3-2
│   │   ├── 1000KB.txt
│   │   ├── 100KB.txt
│   │   ├── 10MB.txt
│   │   ├── Makefile
│   │   ├── SRHeader.h
│   │   ├── crc32.h
│   │   ├── file3.jpg
│   │   ├── lossy_topo.py
│   │   ├── mitm.c
│   │   ├── normal_topo.py
│   │   └── sr.c
│   └── part3-3
│       ├── 1000KB.txt
│       ├── 100KB.txt
│       ├── 10MB.txt
│       ├── Makefile
│       ├── SRHeader.h
│       ├── crc32.h
│       ├── file3.jpg
│       ├── lossy_topo.py
│       ├── mitm.c
│       ├── normal_topo.py
│       └── sr.c
├── report.pdf                 // report
└── ve489_project.pdf          // project description                         
```

