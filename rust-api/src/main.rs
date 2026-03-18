use actix_web::{web, App, HttpServer, HttpResponse, middleware::Logger};
use serde::{Deserialize, Serialize};
use sqlx::postgres::PgPoolOptions;
use redis::Client;
use prometheus::{Encoder, TextEncoder, register_counter, register_gauge, Counter, Gauge};
use std::env;
use tracing::{info, error};
use anyhow::Result;

mod models;
mod handlers;
mod metrics;
mod database;

use handlers::*;
use metrics::*;

#[actix_web::main]
async fn main() -> Result<()> {
    env_logger::init();

    // Initialize metrics
    let metrics = Metrics::new();
    
    // Database connection
    let database_url = env::var("DATABASE_URL")
        .unwrap_or_else(|_| "postgres://trading_user:trading_pass@localhost:5432/trading_platform".to_string());
    
    let pool = PgPoolOptions::new()
        .max_connections(10)
        .connect(&database_url)
        .await
        .map_err(|e| {
            error!("Failed to connect to database: {}", e);
            e
        })?;

    // Redis connection
    let redis_url = env::var("REDIS_URL")
        .unwrap_or_else(|_| "redis://localhost:6379".to_string());
    
    let redis_client = Client::open(redis_url.as_str())?;
    let redis_conn = redis_client.get_multiplexed_async_connection().await
        .map_err(|e| {
            error!("Failed to connect to Redis: {}", e);
            e
        })?;

    info!("Trading API starting on :8080");

    HttpServer::new(move || {
        App::new()
            .app_data(web::Data::new(pool.clone()))
            .app_data(web::Data::new(redis_conn.clone()))
            .app_data(web::Data::new(metrics.clone()))
            .wrap(Logger::default())
            .service(
                web::scope("/api/v1")
                    .route("/health", web::get().to(health_check))
                    .route("/metrics", web::get().to(metrics_handler))
                    .route("/orders", web::get().to(get_orders))
                    .route("/orders", web::post().to(create_order))
                    .route("/orders/{id}", web::get().to(get_order))
                    .route("/trades", web::get().to(get_trades))
                    .route("/orderbook/{symbol}", web::get().to(get_orderbook))
            )
    })
    .bind("0.0.0.0:8080")?
    .run()
    .await?;

    Ok(())
}
