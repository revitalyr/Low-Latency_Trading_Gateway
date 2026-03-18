use serde::{Deserialize, Serialize};
use chrono::{DateTime, Utc};
use sqlx::FromRow;
use uuid::Uuid;

#[derive(Debug, Serialize, Deserialize, FromRow)]
pub struct Order {
    pub id: Uuid,
    pub symbol: String,
    pub side: String, // "buy" or "sell"
    pub quantity: i64,
    pub price: i64,
    pub status: String, // "open", "filled", "cancelled"
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
}

#[derive(Debug, Serialize, Deserialize, FromRow)]
pub struct Trade {
    pub id: Uuid,
    pub symbol: String,
    pub buy_order_id: Uuid,
    pub sell_order_id: Uuid,
    pub quantity: i64,
    pub price: i64,
    pub executed_at: DateTime<Utc>,
}

#[derive(Debug, Deserialize)]
pub struct CreateOrderRequest {
    pub symbol: String,
    pub side: String,
    pub quantity: i64,
    pub price: Option<i64>, // None for market orders
}

#[derive(Debug, Serialize)]
pub struct OrderBookEntry {
    pub price: i64,
    pub quantity: i64,
    pub orders: i64, // number of orders at this price level
}

#[derive(Debug, Serialize)]
pub struct OrderBook {
    pub symbol: String,
    pub bids: Vec<OrderBookEntry>,
    pub asks: Vec<OrderBookEntry>,
    pub spread: Option<i64>,
    pub timestamp: DateTime<Utc>,
}

#[derive(Debug, Serialize)]
pub struct HealthResponse {
    pub status: String,
    pub timestamp: DateTime<Utc>,
    pub version: String,
    pub database: String,
    pub redis: String,
}
