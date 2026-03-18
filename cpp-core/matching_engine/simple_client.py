import argparse
import socket
import struct
import time
import threading
import sys

# Константы типов сообщений
MSG_NEW_ORDER = 1
MSG_CANCEL_ORDER = 2
MSG_MODIFY_ORDER = 3
MSG_ORDER_ACK = 4
MSG_TRADE = 5

# Форматы структур (Little Endian, Packed)
# Header: Type(B), Length(Q), Timestamp(Q), Checksum(I)
HEADER_FMT = '<BQQI'
HEADER_SIZE = struct.calcsize(HEADER_FMT)

# Payloads
# NewOrder: ID(Q), Price(Q), Qty(I), Symbol(8s), Side(c)
NEW_ORDER_FMT = '<QQI8sc'

# CancelOrder: ID(Q)
CANCEL_ORDER_FMT = '<Q'

# ModifyOrder: ID(Q), NewPrice(Q), NewQty(I)
MODIFY_ORDER_FMT = '<QQI'

# OrderAck: ID(Q), Accepted(B/bool), Timestamp(Q), Reason(64s)
ORDER_ACK_FMT = '<QBQ64s'

# Trade: BuyID(Q), SellID(Q), Price(Q), Qty(Q), Symbol(8s), Timestamp(Q)
TRADE_FMT = '<QQQQ8sQ'

def calculate_checksum(data: bytes) -> int:
    """
    Эмуляция C++ алгоритма:
    checksum ^= static_cast<uint32_t>(data[i]) << (i % 4 * 8);
    
    Важно: C++ char может быть знаковым. 
    Мы эмулируем поведение приведения signed char -> uint32_t.
    """
    checksum = 0
    for i, byte in enumerate(data):
        # Эмуляция signed char
        s_byte = byte if byte < 128 else byte - 256
        # Приведение к uint32 (sign extension)
        val = s_byte & 0xFFFFFFFF
        
        shift = (i % 4) * 8
        term = (val << shift) & 0xFFFFFFFF
        checksum ^= term
    return checksum

class TradingClient:
    def __init__(self, host='127.0.0.1', port=8080, verbose=True):
        self.host = host
        self.port = port
        self.verbose = verbose
        self.sock = None
        self.running = False
        self.thread = None
        self.latencies = []
        self.sent_times = {}
        self.ack_count = 0
        self.lock = threading.Lock()

    def connect(self):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            print(f"Connected to {self.host}:{self.port}")
            
            self.running = True
            self.thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.thread.start()
        except ConnectionRefusedError:
            print("Connection failed. Is the server running?")
            sys.exit(1)

    def disconnect(self):
        self.running = False
        if self.sock:
            self.sock.close()
        if self.thread:
            self.thread.join(timeout=1.0)

    def send_new_order(self, order_id, price, quantity, symbol, side):
        # Подготовка payload
        symbol_bytes = symbol.encode('ascii')[:8].ljust(8, b'\x00')
        side_byte = side.encode('ascii')
        payload = struct.pack(NEW_ORDER_FMT, order_id, price, quantity, symbol_bytes, side_byte)
        with self.lock:
            self.sent_times[order_id] = time.time_ns()
        self._send_message(MSG_NEW_ORDER, payload)

    def send_cancel_order(self, order_id):
        payload = struct.pack(CANCEL_ORDER_FMT, order_id)
        self._send_message(MSG_CANCEL_ORDER, payload)

    def send_modify_order(self, order_id, new_price, new_quantity):
        payload = struct.pack(MODIFY_ORDER_FMT, order_id, new_price, new_quantity)
        self._send_message(MSG_MODIFY_ORDER, payload)

    def _send_message(self, msg_type, payload):
        timestamp = int(time.time_ns())
        length = HEADER_SIZE + len(payload)
        
        # Считаем checksum только от payload (как в C++ коде)
        checksum = calculate_checksum(payload)
        
        header = struct.pack(HEADER_FMT, msg_type, length, timestamp, checksum)
        message = header + payload
        
        try:
            self.sock.sendall(message)
            if self.verbose:
                print(f"-> Sent MsgType={msg_type}, Len={length}")
        except OSError as e:
            print(f"Error sending message: {e}")
            self.running = False

    def _receive_loop(self):
        buffer = b""
        while self.running:
            try:
                data = self.sock.recv(4096)
                if not data:
                    print("Server disconnected.")
                    break
                buffer += data
                
                while len(buffer) >= HEADER_SIZE:
                    # Распаковка заголовка
                    header_data = buffer[:HEADER_SIZE]
                    msg_type, length, timestamp, checksum = struct.unpack(HEADER_FMT, header_data)
                    
                    if len(buffer) < length:
                        break # Ждем остальных данных
                    
                    payload = buffer[HEADER_SIZE:length]
                    self._handle_message(msg_type, payload, checksum)
                    
                    # Сдвигаем буфер
                    buffer = buffer[length:]
                    
            except OSError:
                break
        self.running = False

    def _handle_message(self, msg_type, payload, checksum):
        recv_time = time.time_ns()
        # Валидация checksum (опционально, для проверки логики)
        calc_checksum = calculate_checksum(payload)
        valid_str = "OK" if calc_checksum == checksum else "FAIL"

        if msg_type == MSG_ORDER_ACK:
            order_id, accepted, ts, reason_bytes = struct.unpack(ORDER_ACK_FMT, payload)
            reason = reason_bytes.decode('ascii').rstrip('\x00')
            status = "ACCEPTED" if accepted else "REJECTED"
            
            with self.lock:
                if order_id in self.sent_times:
                    start_time = self.sent_times.pop(order_id)
                    latency_us = (recv_time - start_time) / 1000.0
                    self.latencies.append(latency_us)
                    self.ack_count += 1
            
            if self.verbose:
                print(f"<- ACK: OrderID={order_id}, Status={status}, Reason='{reason}', Csum={valid_str}")
            
        elif msg_type == MSG_TRADE:
            buy_id, sell_id, price, qty, sym_bytes, ts = struct.unpack(TRADE_FMT, payload)
            symbol = sym_bytes.decode('ascii').rstrip('\x00')
            if self.verbose:
                print(f"<- TRADE: {symbol} Qty={qty} @ {price}, BuyID={buy_id}, SellID={sell_id}, Csum={valid_str}")
            
        else:
            if self.verbose:
                print(f"<- Unknown message type: {msg_type}")

def run_load_test(client, num_orders):
    print(f"Starting load test: {num_orders} orders...")
    start_time = time.time()
    
    for i in range(1, num_orders + 1):
        client.send_new_order(order_id=10000+i, price=50000, quantity=1, symbol="BTCUSD", side='B')
        
    # Ожидание завершения
    timeout = 10
    wait_start = time.time()
    while True:
        with client.lock:
            if client.ack_count >= num_orders:
                break
        if time.time() - wait_start > timeout:
            print("Timeout waiting for ACKs")
            break
        time.sleep(0.01)

    duration = time.time() - start_time
    print(f"\n--- Load Test Results ---")
    print(f"Sent: {num_orders}, Received: {client.ack_count}")
    print(f"Total time: {duration:.4f}s")
    print(f"Throughput: {client.ack_count / duration:.2f} orders/sec")
    
    if client.latencies:
        latencies = sorted(client.latencies)
        avg_lat = sum(latencies) / len(latencies)
        p50 = latencies[int(len(latencies) * 0.5)]
        p99 = latencies[int(len(latencies) * 0.99)]
        print(f"Latency (us): Avg={avg_lat:.2f}, P50={p50:.2f}, P99={p99:.2f}, Min={latencies[0]:.2f}, Max={latencies[-1]:.2f}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Trading Gateway Client")
    parser.add_argument("--host", default="127.0.0.1", help="Gateway Host")
    parser.add_argument("--port", type=int, default=8080, help="Gateway Port")
    parser.add_argument("--mode", choices=["simple", "load"], default="simple", help="Test mode")
    parser.add_argument("--count", type=int, default=1000, help="Number of orders for load test")
    
    args = parser.parse_args()
    
    client = TradingClient(host=args.host, port=args.port, verbose=(args.mode == "simple"))
    client.connect()
    
    time.sleep(0.5)
    
    if args.mode == "simple":
        print("\n--- Sending Buy Order ---")
        client.send_new_order(order_id=101, price=50000, quantity=10, symbol="BTCUSD", side='B')
        time.sleep(0.2)
        client.send_new_order(order_id=201, price=50000, quantity=4, symbol="BTCUSD", side='S')
        time.sleep(0.2)
        client.send_cancel_order(order_id=101)
        time.sleep(1)
    else:
        run_load_test(client, args.count)
    
    client.disconnect()