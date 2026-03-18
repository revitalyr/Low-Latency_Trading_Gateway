use sqlx::PgPool;
use redis::aio::MultiplexedConnection;
use chrono::Utc;
use uuid::Uuid;
use serde_json;

use crate::models::*;

pub async fn get_all_orders(pool: &PgPool) -> Result<Vec<Order>, sqlx::Error> {
    sqlx::query_as!(
        Order,
        r#"
        SELECT id, symbol, side, quantity, price, status, created_at, updated_at
        FROM orders
        ORDER BY created_at DESC
        "#
    )
    .fetch_all(pool)
    .await
}

pub async fn create_new_order(pool: &PgPool, order_req: CreateOrderRequest) -> Result<Order, sqlx::Error> {
    let order_id = Uuid::new_v4();
    let now = Utc::now();
    
    sqlx::query_as!(
        Order,
        r#"
        INSERT INTO orders (id, symbol, side, quantity, price, status, created_at, updated_at)
        VALUES ($1, $2, $3, $4, $5, 'open', $6, $7)
        RETURNING id, symbol, side, quantity, price, status, created_at, updated_at
        "#,
        order_id,
        order_req.symbol,
        order_req.side,
        order_req.quantity,
        order_req.price,
        now,
        now
    )
    .fetch_one(pool)
    .await
}

pub async fn get_order_by_id(pool: &PgPool, order_id: &Uuid) -> Result<Option<Order>, sqlx::Error> {
    sqlx::query_as!(
        Order,
        r#"
        SELECT id, symbol, side, quantity, price, status, created_at, updated_at
        FROM orders
        WHERE id = $1
        "#,
        order_id
    )
    .fetch_optional(pool)
    .await
}

pub async fn get_all_trades(pool: &PgPool) -> Result<Vec<Trade>, sqlx::Error> {
    sqlx::query_as!(
        Trade,
        r#"
        SELECT id, symbol, buy_order_id, sell_order_id, quantity, price, executed_at
        FROM trades
        ORDER BY executed_at DESC
        "#
    )
    .fetch_all(pool)
    .await
}

pub async fn get_order_book_data(pool: &PgPool, symbol: &str) -> Option<OrderBook> {
    // Get best bids
    let bids_result = sqlx::query!(
        r#"
        SELECT price, SUM(quantity) as total_quantity, COUNT(*) as order_count
        FROM orders
        WHERE symbol = $1 AND side = 'buy' AND status = 'open'
        GROUP BY price
        ORDER BY price DESC
        LIMIT 10
        "#,
        symbol
    )
    .fetch_all(pool)
    .await
    .ok()?;

    // Get best asks
    let asks_result = sqlx::query!(
        r#"
        SELECT price, SUM(quantity) as total_quantity, COUNT(*) as order_count
        FROM orders
        WHERE symbol = $1 AND side = 'sell' AND status = 'open'
        GROUP BY price
        ORDER BY price ASC
        LIMIT 10
        "#,
        symbol
    )
    .fetch_all(pool)
    .await
    .ok()?;

    let bids: Vec<OrderBookEntry> = bids_result
        .into_iter()
        .map(|row| OrderBookEntry {
            price: row.price.unwrap_or(0),
            quantity: row.total_quantity.unwrap_or(0),
            orders: row.order_count.unwrap_or(0) as i64,
        })
        .collect();

    let asks: Vec<OrderBookEntry> = asks_result
        .into_iter()
        .map(|row| OrderBookEntry {
            price: row.price.unwrap_or(0),
            quantity: row.total_quantity.unwrap_or(0),
            orders: row.order_count.unwrap_or(0) as i64,
        })
        .collect();

    let spread = if bids.first().is_some() && asks.first().is_some() {
        Some(asks[0].price - bids[0].price)
    } else {
        None
    };

    Some(OrderBook {
        symbol: symbol.to_string(),
        bids,
        asks,
        spread,
        timestamp: Utc::now(),
    })
}

// Redis caching functions
pub async fn cache_order(redis_conn: &mut MultiplexedConnection, order: &Order) -> Result<(), redis::RedisError> {
    let key = format!("order:{}", order.id);
    let value = serde_json::to_string(order).unwrap_or_default();
    
    redis::cmd("SETEX")
        .arg(key)
        .arg(300) // 5 minutes TTL
        .arg(value)
        .query_async(redis_conn)
        .await
}

pub async fn get_cached_order(
    redis_conn: &mut MultiplexedConnection,
    order_id: &Uuid,
) -> Result<Option<Order>, redis::RedisError> {
    let key = format!("order:{}", order_id);
    let cached: Option<String> = redis::cmd("GET")
        .arg(key)
        .query_async(redis_conn)
        .await?;
    
    match cached {
        Some(json_str) => {
            serde_json::from_str(&json_str)
                .map(Some)
                .map_err(|_| redis::RedisError::from((redis::ErrorKind::TypeError, "Failed to deserialize")))
        }
        None => Ok(None),
    }
}
