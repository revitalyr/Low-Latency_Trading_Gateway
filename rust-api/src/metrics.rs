use prometheus::{Counter, Gauge, Histogram, Opts, Registry, register_counter, register_gauge, register_histogram};
use std::sync::Arc;

#[derive(Clone)]
pub struct Metrics {
    pub registry: Arc<Registry>,
    pub requests_total: Counter,
    pub errors_total: Counter,
    pub orders_created: Counter,
    pub trades_executed: Counter,
    pub request_duration: Histogram,
    pub active_connections: Gauge,
    pub health_checks: Counter,
}

impl Metrics {
    pub fn new() -> Self {
        let registry = Arc::new(Registry::new());
        
        let requests_total = register_counter!(
            "http_requests_total",
            "Total number of HTTP requests",
            registry
        ).unwrap();
        
        let errors_total = register_counter!(
            "http_errors_total",
            "Total number of HTTP errors",
            registry
        ).unwrap();
        
        let orders_created = register_counter!(
            "orders_created_total",
            "Total number of orders created",
            registry
        ).unwrap();
        
        let trades_executed = register_counter!(
            "trades_executed_total",
            "Total number of trades executed",
            registry
        ).unwrap();
        
        let request_duration = register_histogram!(
            "http_request_duration_seconds",
            "HTTP request duration in seconds",
            registry
        ).unwrap();
        
        let active_connections = register_gauge!(
            "active_connections",
            "Number of active connections",
            registry
        ).unwrap();
        
        let health_checks = register_counter!(
            "health_checks_total",
            "Total number of health checks",
            registry
        ).unwrap();
        
        Self {
            registry,
            requests_total,
            errors_total,
            orders_created,
            trades_executed,
            request_duration,
            active_connections,
            health_checks,
        }
    }
}
