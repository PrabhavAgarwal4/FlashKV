# Learning Note 04: Computer Networks, Sockets, and TCP Protocol Design

This document details the network systems architecture implemented in Milestone 4.

---

## 1. The TCP 3-Way Handshake Mapped to Socket APIs

In college networking theory, you learned the 3-Way Handshake:
`SYN -> SYN-ACK -> ACK`.

Here is where that **actually happens in code**:

```
SERVER CODE                                             CLIENT CODE
-----------                                             -----------
1. socket()  [Create OS socket descriptor]              socket()
2. bind()    [Bind to IP 0.0.0.0 & Port 6379]
3. listen()  [Tell OS to accept SYN packets]
                 │
                 │ ◄─────── SYN (Client initiates) ──────── connect()
                 ├─────── SYN-ACK (Kernel replies) ───────►
                 │ ◄─────── ACK (Handshake complete) ──────
                 │
4. accept()  [Pops the completed connection from OS backlog queue]
```

### Key Interview Detail:
* The TCP handshake happens **inside the OS kernel** before your code even wakes up!
* `accept()` blocks until the OS kernel finishes the 3-way handshake. When it wakes up, it returns a **brand-new socket descriptor** specifically for that client.

---

## 2. WHY does `accept()` Return a Brand-New Socket?

### The Question:
If the server is listening on port 6379, why does `accept()` return a new socket instead of using the original one?

### The Deep Answer:
A single listening socket cannot carry on private conversations with 500 different clients at once.
* **The Listening Socket (`listen_sock`):** Acts like the front desk of a hotel. Its only job is to greet incoming connections.
* **The Connected Socket (`client_sock`):** The front desk assigns a private room to the client. In networking, this connection is uniquely identified by the **4-Tuple**:
  $$\text{Connection ID} = (\text{Client IP}, \text{Client Port}, \text{Server IP}, \text{Server Port})$$
* This allows the server to keep listening for new guests on `listen_sock` while worker threads talk to hundreds of existing guests on their private client sockets simultaneously.

---

## 3. WHY TCP is a "Stream Protocol" (The Packet Framing Problem)

### The Question:
If a client sends `SET key1 value1\n` followed immediately by `SET key2 value2\n`, will the server receive them in two separate `recv()` calls?

### The Deep Answer:
**NOT NECESSARILY!**
TCP is a **byte-stream protocol**, not a packet protocol. It provides a continuous stream of bytes with zero guarantees about message boundaries:
1. **Packet Coalescence (TCP Nagle's Algorithm):** The OS might combine both commands into a single `recv()` buffer:
   `"SET key1 value1\nSET key2 value2\n"`
2. **Packet Fragmentation:** A single command might arrive chopped in half across two `recv()` calls:
   - Call 1: `"SET key1 val"`
   - Call 2: `"ue1\n"`

### How FlashKV Solves This (Buffer Framing):
We maintain a per-connection `string buffer_`. Every time `recv()` returns new bytes, we append them to `buffer_` and search for the newline delimiter `\n`. We only execute a command once a complete line has arrived, cleanly slicing off processed commands.
