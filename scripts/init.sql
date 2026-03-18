-- Trading platform database schema

-- Orders table
CREATE TABLE IF NOT EXISTS orders (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    symbol VARCHAR(16) NOT NULL,
    side VARCHAR(4) NOT NULL CHECK (side IN ('buy', 'sell')),
    quantity BIGINT NOT NULL CHECK (quantity > 0),
    price BIGINT NOT NULL CHECK (price > 0),
    status VARCHAR(16) NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'filled', 'cancelled', 'rejected')),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Trades table
CREATE TABLE IF NOT EXISTS trades (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    symbol VARCHAR(16) NOT NULL,
    buy_order_id UUID NOT NULL REFERENCES orders(id),
    sell_order_id UUID NOT NULL REFERENCES orders(id),
    quantity BIGINT NOT NULL CHECK (quantity > 0),
    price BIGINT NOT NULL CHECK (price > 0),
    executed_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_orders_symbol ON orders(symbol);
CREATE INDEX IF NOT EXISTS idx_orders_status ON orders(status);
CREATE INDEX IF NOT EXISTS idx_orders_created_at ON orders(created_at);
CREATE INDEX IF NOT EXISTS idx_trades_symbol ON trades(symbol);
CREATE INDEX IF NOT EXISTS idx_trades_executed_at ON trades(executed_at);

-- Insert sample data
INSERT INTO orders (symbol, side, quantity, price) VALUES
    ('AAPL', 'buy', 100, 15000),
    ('AAPL', 'sell', 50, 15100),
    ('GOOGL', 'buy', 200, 28000),
    ('GOOGL', 'sell', 150, 28100)
ON CONFLICT DO NOTHING;
