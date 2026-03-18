import socket
import struct
import time
import threading
import argparse
import sys

# --- Конфигурация протокола (дублирование для автономности скрипта) ---
HEADER_FMT = '<BQQI'
HEADER_SIZE = struct.calcsize(HEADER_FMT)
NEW_ORDER_FMT = '<QQI8sc'
MSG_NEW_ORDER = 1

def calculate_checksum(data: bytes) -> int:
    checksum = 0
    for i, byte in enumerate(data):
        s_byte = byte if byte < 128 else byte - 256
        val = s_byte & 0xFFFFFFFF
        shift = (i % 4) * 8
        term = (val << shift) & 0xFFFFFFFF
        checksum ^= term
    return checksum

# --- Простой StatsD Сервер для сбора метрик из C++ ---
class StatsDListener:
    def __init__(self, port=8125):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('0.0.0.0', port))
        self.running = True
        self.latencies = []
        self.lock = threading.Lock()
        self.count = 0

    def start(self):
        t = threading.Thread(target=self._loop, daemon=True)
        t.start()
        print(f"[StatsD] Listening on UDP :{8125} for metrics from C++ Core...")

    def _loop(self):
        while self.running:
            try:
                self.sock.settimeout(1.0)
                data, _ = self.sock.recvfrom(4096)
                line = data.decode('ascii').strip()
                
                # Парсинг формата: engine.process_latency_us:42|ms
                if 'engine.process_latency_us' in line:
                    parts = line.split(':')
                    if len(parts) > 1:
                        val_part = parts[1].split('|')[0]
                        val = int(val_part)
                        with self.lock:
                            self.latencies.append(val)
                elif 'engine.orders_processed' in line:
                    with self.lock:
                        self.count += 1
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"StatsD Error: {e}")

    def stop(self):
        self.running = False
        self.sock.close()

    def get_stats(self):
        with self.lock:
            if not self.latencies:
                return 0, 0, 0, 0
            avg = sum(self.latencies) / len(self.latencies)
            p99 = sorted(self.latencies)[int(len(self.latencies) * 0.99)]
            return len(self.latencies), avg, min(self.latencies), p99

# --- Клиент для отправки ордеров ---
class LoadGenerator:
    def __init__(self, host, port, count):
        self.host = host
        self.port = port
        self.count = count
        self.sock = None

    def connect(self):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            print(f"[Client] Connected to Gateway {self.host}:{self.port}")
        except Exception as e:
            print(f"Failed to connect: {e}")
            sys.exit(1)

    def send_orders(self):
        print(f"[Client] Sending {self.count} orders...")
        start_time = time.time()
        
        for i in range(self.count):
            # Формируем пакет NewOrder
            order_id = 100000 + i
            price = 50000
            qty = 1
            symbol = b"BTCUSD".ljust(8, b'\x00')
            side = b'B'
            
            payload = struct.pack(NEW_ORDER_FMT, order_id, price, qty, symbol, side)
            
            timestamp = int(time.time_ns())
            length = HEADER_SIZE + len(payload)
            checksum = calculate_checksum(payload)
            
            header = struct.pack(HEADER_FMT, MSG_NEW_ORDER, length, timestamp, checksum)
            
            # Отправка без ожидания ответа (Fire and Forget) для максимальной нагрузки
            try:
                self.sock.sendall(header + payload)
            except OSError:
                break
                
            # Небольшая пауза каждые 1000 ордеров, чтобы не переполнить TCP буфер ОС
            if i % 1000 == 0:
                time.sleep(0.001)

        duration = time.time() - start_time
        print(f"[Client] Sent {self.count} orders in {duration:.4f}s ({self.count/duration:.0f} ops)")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Gateway Benchmark & StatsD Sink")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--count", type=int, default=10000)
    args = parser.parse_args()

    # 1. Запуск слушателя метрик (эмуляция StatsD сервера)
    statsd = StatsDListener()
    statsd.start()

    # 2. Запуск генератора нагрузки
    client = LoadGenerator(args.host, args.port, args.count)
    client.connect()
    
    # Даем время на инициализацию
    time.sleep(1)
    
    # Запуск теста
    client.send_orders()
    
    # 3. Ожидание обработки (ждем, пока метрики перестанут приходить)
    print("[Benchmark] Waiting for processing to finish...")
    last_count = 0
    stale_checks = 0
    while stale_checks < 5:
        time.sleep(0.5)
        processed_count, avg, min_lat, p99 = statsd.get_stats()
        if processed_count == last_count and processed_count > 0:
            stale_checks += 1
        else:
            stale_checks = 0
        last_count = processed_count
        
        if processed_count >= args.count:
            break
            
    # 4. Вывод результатов
    count, avg_lat, min_lat, p99_lat = statsd.get_stats()
    print("\n" + "="*40)
    print(f"INTERNAL MATCHING ENGINE LATENCY (us)")
    print("="*40)
    print(f"Processed: {count}/{args.count}")
    print(f"Average:   {avg_lat:.2f} us")
    print(f"Min:       {min_lat} us")
    print(f"99% Pctl:  {p99_lat} us")
    print("="*40)

    statsd.stop()