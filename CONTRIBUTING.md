# Contributing to Low-Latency Trading Gateway

Thank you for your interest in contributing to this high-performance trading system! This document provides guidelines for contributors.

## Development Philosophy

This project focuses on **ultra-low latency** and **high throughput** trading systems. All contributions should prioritize:

- **Deterministic performance** - No blocking operations in critical paths
- **Memory efficiency** - Cache-friendly data structures and minimal allocations
- **Concurrent safety** - Lock-free algorithms where possible
- **Code quality** - Modern C++ practices with clear documentation

## Getting Started

### Prerequisites
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2022)
- Rust 1.75+ (for API components)
- Docker & Docker Compose
- PostgreSQL client tools

### Development Setup

1. **Clone the repository**:
```bash
git clone https://github.com/revitalyr/Low-Latency_Trading_Gateway.git
cd Low-Latency_Trading_Gateway
```

2. **Build C++ components**:
```bash
cd cpp-core
mkdir build && cd build
cmake ..
make -j$(nproc)
```

3. **Build tests**:
```bash
cd tests
mkdir build && cd build
cmake ..
make -j$(nproc)
```

4. **Run tests**:
```bash
./tests/build/trading_gateway_tests
```

## Code Style Guidelines

### C++ Code
- Use **C++20** features where appropriate
- Follow **RAII** principles consistently
- Use **`constexpr`** and **`noexcept`** where possible
- Prefer **`std::atomic`** over mutexes for synchronization
- Use **`alignas(64)`** for cache line alignment of critical structures
- Follow **snake_case** for variables and functions
- Follow **PascalCase** for types and classes

### Rust Code
- Use **`rustfmt`** for formatting
- Prefer **`unwrap()`** only in tests, use **`?`** operator otherwise
- Use **`Arc<Mutex<T>>`** sparingly - prefer lock-free alternatives
- Follow conventional Rust naming patterns

### Documentation
- All public APIs must have **Doxygen** comments (C++) or **rustdoc** comments (Rust)
- Include **performance characteristics** in documentation
- Document **memory ordering** semantics for atomic operations
- Provide **usage examples** for complex APIs

## Performance Requirements

### Latency Targets
- Order entry: **< 20μs** average
- Order matching: **< 25μs** average
- Message parsing: **< 5μs** average
- Network I/O: **< 10μs** average

### Throughput Targets
- Message processing: **> 500K msg/sec**
- Order matching: **> 100K orders/sec**
- Concurrent connections: **> 1000**

### Memory Requirements
- Zero dynamic allocation in critical paths
- Pre-allocated memory pools for frequent operations
- Cache-friendly data layout (struct of arrays where beneficial)

## Testing Guidelines

### Unit Tests
- Use **Boost.UT** framework for C++ tests
- Test **concurrent scenarios** with multiple threads
- Include **performance benchmarks** in test suites
- Verify **memory safety** with tools like Valgrind/AddressSanitizer

### Integration Tests
- Test **end-to-end latency** with realistic workloads
- Verify **protocol compliance** with wire format specifications
- Test **error handling** and edge cases
- Include **load testing** scenarios

### Performance Tests
- All performance tests must include:
  - **Latency distribution** (P50, P90, P99)
  - **Throughput measurements**
  - **Memory usage** tracking
  - **CPU utilization** metrics

## Submission Process

### 1. Create Issue
- Open an issue describing the proposed change
- Discuss implementation approach and performance implications
- Get approval from maintainers before starting work

### 2. Development
- Create feature branch: `git checkout -b feature/description`
- Implement changes with comprehensive tests
- Ensure all tests pass and performance targets are met
- Update documentation as needed

### 3. Code Review
- Submit pull request with detailed description
- Include performance benchmarks in PR description
- Address all review feedback promptly
- Ensure CI/CD pipeline passes successfully

### 4. Merge
- PR must have at least one approval from maintainer
- All tests must pass
- Performance regression tests must pass
- Documentation must be up to date

## Performance Review Checklist

Before submitting PR, verify:

- [ ] No dynamic memory allocation in critical paths
- [ ] Proper memory ordering for atomic operations
- [ ] Cache line alignment for frequently accessed structures
- [ ] Minimal branching in hot paths
- [ ] Efficient data structure choices (vector vs list, etc.)
- [ ] Proper benchmarking with realistic workloads
- [ ] No performance regressions in benchmark suite

## Areas for Contribution

### High Priority
- **Network optimizations**: RDMA, DPDK integration
- **Algorithm improvements**: More efficient matching algorithms
- **Memory management**: Advanced allocator implementations
- **Protocol extensions**: Additional message types and features

### Medium Priority
- **Monitoring**: Enhanced metrics and observability
- **Configuration**: Dynamic configuration system
- **Security**: TLS implementation and authentication
- **Documentation**: Architecture deep-dive documents

### Low Priority
- **Tooling**: Performance analysis and debugging tools
- **Examples**: Sample client implementations
- **CI/CD**: Enhanced testing and deployment pipelines

## Questions and Support

- Open an issue for questions or discussion
- Join our Discord/Slack for real-time discussion
- Check existing issues and documentation first
- Provide detailed information when reporting issues

## License

By contributing to this project, you agree that your contributions will be licensed under the same **MIT License** as the project.

---

Thank you for helping build high-performance trading systems! 🚀
