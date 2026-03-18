use actix_web::{web, HttpResponse, Result};
use serde_json::json;
use sqlx::PgPool;
use redis::aio::MultiplexedConnection;
use prometheus::Encoder;
use tracing::{info, error, warn};
use chrono::Utc;
use uuid::Uuid;

use crate::models::*;
use crate::metrics::Metrics;
use crate::database::*;

pub async fn health_check(
    pool: web::Data<PgPool>,
    redis_conn: web::Data<MultiplexedConnection>,
    metrics: web::Data<Metrics>,
) -> Result<HttpResponse> {
    let db_status = match sqlx::query("SELECT 1").fetch_one(pool.get_ref()).await {
        Ok(_) => "healthy",
        Err(e) => {
            error!("Database health check failed: {}", e);
            "unhealthy"
        }
    };

    let redis_status = match redis::cmd("PING").query_async::<_, String>(redis_conn.get_ref()).await {
        Ok(_) => "healthy",
        Err(e) => {
            error!("Redis health check failed: {}", e);
            "unhealthy"
        }
    };

    let overall_status = if db_status == "healthy" && redis_status == "healthy" {
        "healthy"
    } else {
        "unhealthy"
    };

    let response = HealthResponse {
        status: overall_status.to_string(),
        timestamp: Utc::now(),
        version: "0.1.0".to_string(),
        database: db_status.to_string(),
        redis: redis_status.to_string(),
    };

    if overall_status == "healthy" {
        metrics.health_checks.inc();
        Ok(HttpResponse::Ok().json(response))
    } else {
        Ok(HttpResponse::ServiceUnavailable().json(response))
    }
}

pub async fn metrics_handler(metrics: web::Data<Metrics>) -> Result<HttpResponse> {
    let encoder = TextEncoder::new();
    let metric_families = prometheus::gather();
    let mut buffer = Vec::new();
    encoder.encode(&metric_families, &mut buffer).unwrap();
    
    Ok(HttpResponse::Ok()
        .content_type("text/plain; version=0.0.4")
        .body(buffer))
}

pub async fn get_orders(
    pool: web::Data<PgPool>,
    metrics: web::Data<Metrics>,
) -> Result<HttpResponse> {
    metrics.requests_total.with_label_values(&["get_orders"]).inc();
    
    match get_all_orders(pool.get_ref()).await {
        Ok(orders) => {
            info!("Retrieved {} orders", orders.len());
            Ok(HttpResponse::Ok().json(orders))
        }
        Err(e) => {
            error!("Failed to get orders: {}", e);
            metrics.errors_total.with_label_values(&["get_orders"]).inc();
            Ok(HttpResponse::InternalServerError().json(json!({
                "error": "Failed to retrieve orders"
            })))
        }
    }
}

pub async fn create_order(
    pool: web::Data<PgPool>,
    redis_conn: web::Data<MultiplexedConnection>,
    order_req: web::Json<CreateOrderRequest>,
    metrics: web::Data<Metrics>,
) -> Result<HttpResponse> {
    metrics.requests_total.with_label_values(&["create_order"]).inc();
    
    // Validate request
    if order_req.quantity <= 0 {
        return Ok(HttpResponse::BadRequest().json(json!({
            "error": "Quantity must be positive"
        })));
    }
    
    if !["buy", "sell"].contains(&order_req.side.as_str()) {
        return Ok(HttpResponse::BadRequest().json(json!({
            "error": "Side must be 'buy' or 'sell'"
        })));
    }

    match create_new_order(pool.get_ref(), order_req.into_inner()).await {
        Ok(order) => {
            info!("Created new order: {}", order.id);
            metrics.orders_created.inc();
            
            // Cache in Redis for quick access
            let _ = cache_order(redis_conn.get_ref(), &order).await;
            
            Ok(HttpResponse::Created().json(order))
        }
        Err(e) => {
            error!("Failed to create order: {}", e);
            metrics.errors_total.with_label_values(&["create_order"]).inc();
            Ok(HttpResponse::InternalServerError().json(json!({
                "error": "Failed to create order"
            })))
        }
    }
}

pub async fn get_order(
    pool: web::Data<PgPool>,
    redis_conn: web::Data<MultiplexedConnection>,
    path: web::Path<Uuid>,
    metrics: web::Data<Metrics>,
) -> Result<HttpResponse> {
    metrics.requests_total.with_label_values(&["get_order"]).inc();
    
    let order_id = path.into_inner();
    
    // Try Redis cache first
    if let Ok(Some(cached_order)) = get_cached_order(redis_conn.get_ref(), &order_id).await {
        info!("Retrieved order {} from cache", order_id);
        return Ok(HttpResponse::Ok().json(cached_order));
    }
    
    // Fallback to database
    match get_order_by_id(pool.get_ref(), &order_id).await {
        Ok(Some(order)) => {
            info!("Retrieved order {} from database", order_id);
            
            // Cache for future requests
            let _ = cache_order(redis_conn.get_ref(), &order).await;
            
            Ok(HttpResponse::Ok().json(order))
        }
        Ok(None) => {
            warn!("Order {} not found", order_id);
            Ok(HttpResponse::NotFound().json(json!({
                "error": "Order not found"
            })))
        }
        Err(e) => {
            error!("Failed to get order {}: {}", order_id, e);
            metrics.errors_total.with_label_values(&["get_order"]).inc();
            Ok(HttpResponse::InternalServerError().json(json!({
                "error": "Failed to retrieve order"
            })))
        }
    }
}

pub async fn get_trades(
    pool: web::Data<PgPool>,
    metrics: web::Data<Metrics>,
) -> Result<HttpResponse> {
    metrics.requests_total.with_label_values(&["get_trades"]).inc();
    
    match get_all_trades(pool.get_ref()).await {
        Ok(trades) => {
            info!("Retrieved {} trades", trades.len());
            Ok(HttpResponse::Ok().json(trades))
        }
        Err(e) => {
            error!("Failed to get trades: {}", e);
            metrics.errors_total.with_label_values(&["get_trades"]).inc();
            Ok(HttpResponse::InternalServerError().json(json!({
                "error": "Failed to retrieve trades"
            })))
        }
    }
}

pub async fn get_orderbook(
    pool: web::Data<PgPool>,
    path: web::Path<String>,
    metrics: web::Data<Metrics>,
) -> Result<HttpResponse> {
    metrics.requests_total.with_label_values(&["get_orderbook"]).inc();
    
    let symbol = path.into_inner();
    
    match get_order_book_data(pool.get_ref(), &symbol).await {
        Some(orderbook) => {
            info!("Retrieved orderbook for {}", symbol);
            Ok(HttpResponse::Ok().json(orderbook))
        }
        None => {
            warn!("No orderbook data found for {}", symbol);
            Ok(HttpResponse::NotFound().json(json!({
                "error": "Orderbook not found"
            })))
        }
    }
}
