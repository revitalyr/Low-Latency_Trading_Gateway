# Technical Decisions

## Architecture Decisions

### 1. C++ for Core Engine

**Decision**: Use C++20 for the matching engine and order gateway

**Rationale**:
- **Performance**: Zero-cost abstractions and direct memory control
- **Latency**: Deterministic performance, no garbage collection pauses
- **Ecosystem**: Mature high-frequency trading libraries and tools
- **Hardware Access**: Direct control over CPU, memory, and network optimizations

**Alternatives Considered**:
- **Java**: GC pauses unacceptable for microsecond latency
- **Rust**: Excellent safety but smaller HFT ecosystem
- **Go**: Runtime overhead too high for latency requirements

### 2. Rust for REST API

**Decision**: Use Rust for the REST API layer

**Rationale**:
- **Safety**: Memory safety without performance penalty
- **Concurrency**: Fearless concurrency with async/await
- **Web Framework**: Actix-web provides excellent performance
- **Type System**: Compile-time guarantees reduce runtime errors

**Alternatives Considered**:
- **Node.js**: Event loop overhead too high
- **Python**: GIL limits concurrency, too slow
- **Java**: Good performance but more verbose than Rust

### 3. Custom Binary Protocol

**Decision**: Implement custom binary protocol instead of standard formats

**Rationale**:
- **Latency**: Minimal serialization overhead (~1μs vs ~10μs for protobuf)
- **Size**: Compact message format reduces network bandwidth
- **Parsing**: Simple, predictable parsing time
- **Control**: No external dependencies or version compatibility issues

**Message Format**:
```
[Header: 32 bytes][Payload: variable]
├── Length (4 bytes): Total message size
├── Sequence (4 bytes): Message ordering
├── Timestamp (8 bytes): Unix nanoseconds
├── Type (1 byte): Message type enum
└── Checksum (4 bytes): Integrity verification
```

**Alternatives Considered**:
- **Protocol Buffers**: Too slow for microsecond latency
- **FlatBuffers**: Better but still overhead
- **MessagePack**: Good but not optimal for our use case

### 4. Lock-free Data Structures

**Decision**: Use lock-free algorithms for shared data structures

**Rationale**:
- **Latency**: No blocking, predictable performance
- **Scalability**: Better performance with increasing core count
- **Fairness**: No thread starvation issues
- **Deadlock Prevention**: Impossible to have deadlocks

**Implementation**:
- **Lock-free Queue**: For message passing between threads
- **Atomic Operations**: For counters and flags
- **Memory Ordering**: Careful use of memory barriers

**Alternatives Considered**:
- **Mutex-based**: Simpler but blocking under contention
- **Read-write Locks**: Better read performance but still blocking
- **Actor Model**: Higher overhead for message passing

### 5. TCP for Transport

**Decision**: Use TCP instead of UDP for client connections

**Rationale**:
- **Reliability**: Guaranteed message delivery and ordering
- **Flow Control**: Built-in congestion control
- **Compatibility**: Works through firewalls and NAT
- **Implementation**: Mature, well-understood protocol

**Optimizations**:
- **TCP_NODELAY**: Disable Nagle's algorithm
- **Socket Buffers**: Tuned for low latency
- **Keepalive**: Detect dead connections quickly

**Alternatives Considered**:
- **UDP**: Lower latency but reliability concerns
- **SCTP**: Better features but limited support
- **Custom Reliable UDP**: Complex to implement correctly

### 6. In-memory Order Book

**Decision**: Keep order book entirely in memory

**Rationale**:
- **Latency**: Memory access is ~100ns vs disk ~1ms
- **Performance**: No I/O bottlenecks during matching
- **Simplicity**: Easier to implement and optimize
- **Recovery**: Can reconstruct from trade log

**Persistence Strategy**:
- **Trade Log**: All trades written to PostgreSQL
- **Snapshot**: Periodic order book snapshots
- **Recovery**: Rebuild from log + latest snapshot

**Alternatives Considered**:
- **Database-backed**: Too slow for real-time operations
- **Hybrid**: Complex with limited benefits
- **Distributed**: Added latency for consistency

### 7. PostgreSQL for Persistence

**Decision**: Use PostgreSQL for trade storage

**Rationale**:
- **ACID Compliance**: Guaranteed data integrity
- **Performance**: Excellent for write-heavy workloads
- **Features**: Rich query capabilities for analytics
- **Reliability**: Mature, battle-tested database

**Schema Design**:
```sql
CREATE TABLE orders (
    id UUID PRIMARY KEY,
    symbol VARCHAR(16) NOT NULL,
    side VARCHAR(4) NOT NULL,
    quantity BIGINT NOT NULL,
    price BIGINT NOT NULL,
    status VARCHAR(16) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE TABLE trades (
    id UUID PRIMARY KEY,
    symbol VARCHAR(16) NOT NULL,
    buy_order_id UUID NOT NULL REFERENCES orders(id),
    sell_order_id UUID NOT NULL REFERENCES orders(id),
    quantity BIGINT NOT NULL,
    price BIGINT NOT NULL,
    executed_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);
```

**Alternatives Considered**:
- **MongoDB**: No ACID guarantees
- **TimescaleDB**: Good for time-series but overkill
- **ClickHouse**: Excellent for analytics but limited for OLTP

### 8. Redis for Caching

**Decision**: Use Redis for high-speed caching

**Rationale**:
- **Performance**: In-memory, microsecond latency
- **Data Structures**: Rich set of data types
- **Persistence**: Optional disk persistence
- **Scalability**: Easy to cluster and scale

**Use Cases**:
- **Order Book Snapshots**: Quick market data access
- **Session Data**: Client authentication and state
- **Rate Limiting**: Per-client rate limiting
- **Real-time Analytics**: Temporary aggregations

**Alternatives Considered**:
- **Memcached**: Simpler but fewer features
- **Hazelcast**: More complex than needed
- **Custom Cache**: More maintenance overhead

### 9. Docker for Deployment

**Decision**: Use Docker containers for deployment

**Rationale**:
- **Consistency**: Same environment everywhere
- **Isolation**: Process and resource isolation
- **Portability**: Run anywhere Docker runs
- **Scaling**: Easy to scale with orchestration

**Container Strategy**:
```yaml
services:
  postgres:
    image: postgres:15-alpine
    environment:
      POSTGRES_DB: trading_platform
  
  redis:
    image: redis:7-alpine
    command: redis-server --appendonly yes
  
  rust-api:
    build: ./rust-api
    depends_on:
      - postgres
      - redis
```

**Alternatives Considered**:
- **Bare Metal**: Maximum performance but complex deployment
- **VMs**: Heavier than containers
- **Kubernetes**: Overkill for single deployment

### 10. Prometheus for Monitoring

**Decision**: Use Prometheus for metrics collection

**Rationale**:
- **Time Series**: Optimized for metrics data
- **Pull Model**: Simple, scalable architecture
- **Ecosystem**: Rich ecosystem of exporters and tools
- **Querying**: Powerful query language (PromQL)

**Key Metrics**:
- **Latency**: P50, P90, P99 latencies
- **Throughput**: Messages/second per component
- **Errors**: Error rates and types
- **Resources**: CPU, memory, network usage

**Alternatives Considered**:
- **StatsD**: Simple but less powerful
- **InfluxDB**: Good but more complex
- **Custom Solution**: More maintenance overhead

## Performance vs. Trade-offs

### What We Optimized For
1. **Latency**: Microsecond-level response times
2. **Throughput**: Hundreds of thousands of messages per second
3. **Determinism**: Predictable performance under load
4. **Scalability**: Linear scaling with CPU cores

### What We Sacrificed
1. **Simplicity**: Complex lock-free algorithms
2. **Development Speed**: More complex than simple solutions
3. **Portability**: Some platform-specific optimizations
4. **Memory Usage**: Pre-allocated structures use more memory

### Future Considerations
1. **DPDK**: Kernel bypass for network I/O
2. **FPGA**: Hardware acceleration for matching
3. **RDMA**: Direct memory access for networking
4. **Persistent Memory**: Intel Optane for in-memory persistence

## Lessons Learned

### What Worked Well
1. **Binary Protocol**: Significant latency improvement
2. **Lock-free Structures**: Excellent scalability
3. **Memory Pooling**: Reduced allocation overhead
4. **Thread Pinning**: Consistent performance

### What Could Be Better
1. **Testing**: More comprehensive performance testing
2. **Monitoring**: More granular metrics collection
3. **Documentation**: Better performance tuning guides
4. **Tooling**: More automated benchmarking tools

### What We'd Do Differently
1. **Start with Rust**: Could have used Rust for more components
2. **Earlier Performance Testing**: Should have benchmarked earlier
3. **More Abstraction**: Some optimizations made code harder to maintain
4. **Better Error Handling**: Performance optimizations sometimes obscured error paths
