# Learning Note 05: System Design — Consistent Hashing & Rate Limiting

This document details the distributed systems algorithms implemented in Milestone 5.

---

## 1. WHY Consistent Hashing instead of Modulo Sharding ($H(key) \pmod N$)?

### The Question:
If we have $N=3$ database nodes, the simplest way to shard keys is:
$$\text{Node Index} = \text{hash}(\text{key}) \pmod N$$
Why is this considered a fatal design flaw in distributed systems?

### The Deep Answer:
It causes the dreaded **Cache Stampede / 100% Key Churn**:

1. **The Scenario:**
   Suppose you have 1,000,000 keys cached across 3 servers using `hash(key) % 3`.
   Now server #3 crashes, or traffic doubles so you add a 4th server ($N=4$).
2. **The Disaster:**
   - With $N=4$, keys are now mapped via `hash(key) % 4`.
   - Mathematically, **over 75% to 90% of all keys suddenly hash to a different node!**
   - Result: All cache queries miss simultaneously. The backing database gets crushed with 1,000,000 requests at once (**Cache Stampede**), causing system downtime.

---

## 2. HOW Consistent Hashing Works ($0$ to $2^{32}-1$)

Instead of modulo $N$, consistent hashing maps both **nodes** and **keys** onto an abstract circle (a 32-bit integer ring from $0$ to $2^{32}-1$):

```
                   0 / 2³²
                 .  -  .
             '           '
          Node 1           Node 2
        (at 20%)          (at 50%)
          .                  .
           .                .
                 Node 3
                (at 80%)
```

1. **Routing:** To find which node owns a key, hash the key to a position on the ring, then move **clockwise** until you hit the first server node.
2. **Minimal Key Migration:**
   If a new node is added between Node 1 and Node 2, **only keys that fall in that specific segment are moved**. All other keys on the ring remain untouched.
   $$\text{Keys Moved} \approx \frac{1}{N} \quad (\text{instead of } \approx 100\%!)$$

---

## 3. WHY Virtual Nodes are Mandatory (Hot Spot Mitigation)

### The Problem with Real Nodes:
With only 3 physical nodes, their hashes might cluster together on the ring (e.g., at 10%, 12%, and 15%). Node 3 would be forced to handle 95% of the ring's traffic, creating a severe **hot spot**.

### The Virtual Nodes Solution:
Instead of placing each physical server on the ring once, we assign each server **100 to 200 virtual replicas**:
- `node1#0`, `node1#1`, `node1#2`, ..., `node1#99`
- `node2#0`, `node2#1`, `node2#2`, ..., `node2#99`

These virtual nodes are scattered uniformly across the entire ring.
- **Statistical Balance:** Standard deviation of key distribution drops to under 3-5%.
- **Heterogeneous Hardware:** A powerful 64-core machine can be given 300 virtual nodes, while an 8-core machine gets 100 virtual nodes, naturally balancing the load proportional to capacity.

---

## 4. Token Bucket vs. Leaky Bucket Rate Limiting

### The Algorithms:
1. **Leaky Bucket:** Water enters the bucket at bursty rates, but leaks out of a hole at a strictly constant rate. Smooths out traffic, but cannot handle legitimate bursts.
2. **Token Bucket (Our Implementation):**
   - The bucket holds up to $C$ tokens (Capacity).
   - Tokens are added at a constant rate $R$ (e.g., 50 tokens/sec).
   - Each incoming request consumes 1 token.
   - If the bucket has tokens $\to$ Request is **Allowed**.
   - If the bucket is empty $\to$ Request is **Rejected (HTTP 429 Too Many Requests)**.

### The C++ Optimization (No Background Refill Threads):
Instead of spawning a background thread that sleeps and adds tokens every 20ms, we compute tokens **lazily on demand**:
$$\text{Tokens to Add} = (\text{Current Time} - \text{Last Request Time}) \times R$$
This requires **zero background CPU overhead** and provides nanosecond-precision token refill.
