import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import random

# ==============================================================================
# [FCS K105A1 B-LINK]
# Modern Tactical Fire Control Client
# ==============================================================================

# Protocol Constants
STX = 0x02
ETX = 0x03
CMD_TARGET = 0xA1
MASTER_KEY = 0xA5

class FCS_ClientApp:
    def __init__(self, root):
        self.root = root
        self.root.title("K105A1 B-LINK v1.0")
        self.root.geometry("600x750")
        # Modern Style Setting
        self.style = ttk.Style()
        self.style.theme_use('clam') # Clean look
        
        self.ser = None
        self.connected = False
        self.retry_count = 0
        self.max_retries = 2
        self.last_packet = None # For retry
        
        self.init_ui()
        
        # Serial Listener Thread
        self.running = True
        self.rx_thread = threading.Thread(target=self.serial_listener)
        self.rx_thread.daemon = True
        self.rx_thread.start()

    def init_ui(self):
        # [Header] Connection Bar
        header_frame = ttk.Frame(self.root, padding="10")
        header_frame.pack(fill=tk.X)
        
        ttk.Label(header_frame, text="CONNECTION", font=("Segoe UI", 10, "bold")).pack(side=tk.LEFT)
        
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(header_frame, textvariable=self.port_var, width=15)
        self.refresh_ports()
        self.port_combo.pack(side=tk.LEFT, padx=10)
        
        self.btn_conn = ttk.Button(header_frame, text="CONNECT", command=self.toggle_connection)
        self.btn_conn.pack(side=tk.LEFT)
        
        self.status_lbl = ttk.Label(header_frame, text="OFFLINE", foreground="red")
        self.status_lbl.pack(side=tk.RIGHT)

        # [Main] Target Input Section
        main_frame = ttk.LabelFrame(self.root, text="TARGET ACQUISITION", padding="15")
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        # Grid Layout
        ttk.Label(main_frame, text="ZONE / BAND").grid(row=0, column=0, sticky="w", pady=5)
        
        zb_frame = ttk.Frame(main_frame)
        zb_frame.grid(row=0, column=1, sticky="w", pady=5)
        self.zone_var = tk.StringVar(value="52")
        self.band_var = tk.StringVar(value="S")
        ttk.Entry(zb_frame, textvariable=self.zone_var, width=5).pack(side=tk.LEFT, padx=0)
        ttk.Combobox(zb_frame, textvariable=self.band_var, values=["S", "T"], width=3).pack(side=tk.LEFT, padx=5)

        ttk.Label(main_frame, text="EASTING (m)").grid(row=1, column=0, sticky="w", pady=5)
        self.east_var = tk.StringVar(value="333712")
        ttk.Entry(main_frame, textvariable=self.east_var, width=20).grid(row=1, column=1, sticky="w")
        
        ttk.Label(main_frame, text="NORTHING (m)").grid(row=2, column=0, sticky="w", pady=5)
        self.north_var = tk.StringVar(value="4132894")
        ttk.Entry(main_frame, textvariable=self.north_var, width=20).grid(row=2, column=1, sticky="w")
        
        ttk.Label(main_frame, text="ALTITUDE (m)").grid(row=3, column=0, sticky="w", pady=5)
        self.alt_var = tk.StringVar(value="100")
        ttk.Entry(main_frame, textvariable=self.alt_var, width=10).grid(row=3, column=1, sticky="w")

        # Action Button
        self.btn_send = ttk.Button(main_frame, text="TRANSMIT FIRE DATA", command=self.prepare_and_send)
        self.btn_send.grid(row=4, column=0, columnspan=2, pady=20, sticky="ew")

        # [Result] Firing Data Return
        res_frame = ttk.LabelFrame(self.root, text="FIRE MISSION DATA", padding="15")
        res_frame.pack(fill=tk.BOTH, padx=10, pady=5)
        
        self.res_val_lbl = ttk.Label(res_frame, text="WAITING FOR DATA...", font=("Consolas", 14), anchor="center")
        self.res_val_lbl.pack(fill=tk.X, pady=10)

        # [Logs] Secure Comm Monitor (Collapsible-ish)
        log_frame = ttk.LabelFrame(self.root, text="SECURE COMM LOG (DEBUG)", padding="5")
        log_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.log_text = tk.Text(log_frame, height=8, font=("Consolas", 9), bg="#2b2b2b", fg="#00ff00")
        self.log_text.pack(fill=tk.BOTH, expand=True)

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        self.port_combo['values'] = [port.device for port in ports]
        if ports:
            self.port_combo.current(0)

    def log(self, msg, tag="INFO"):
        self.log_text.insert(tk.END, f"[{tag}] {msg}\n")
        self.log_text.see(tk.END)

    def toggle_connection(self):
        if not self.connected:
            try:
                port = self.port_var.get()
                self.ser = serial.Serial(port, 9600, timeout=0.1)
                self.connected = True
                self.btn_conn.config(text="DISCONNECT")
                self.status_lbl.config(text="ONLINE", foreground="green")
                self.log(f"Connected to {port}")
            except Exception as e:
                messagebox.showerror("Error", str(e))
        else:
            if self.ser:
                self.ser.close()
            self.connected = False
            self.btn_conn.config(text="CONNECT")
            self.status_lbl.config(text="OFFLINE", foreground="red")
            self.log("Disconnected")

    def calc_crc8(self, data):
        crc = 0
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x80:
                    crc = (crc << 1) ^ 0x07
                else:
                    crc <<= 1
            crc &= 0xFF
        return crc

    def prepare_and_send(self):
        if not self.connected:
            messagebox.showwarning("Warning", "Connect to port first!")
            return

        # 1. Prepare Payload
        # Format: "52,S,333712,4132894,100"
        payload_str = f"{self.zone_var.get()},{self.band_var.get()},{self.east_var.get()},{self.north_var.get()},{self.alt_var.get()}"
        payload_bytes = payload_str.encode('utf-8')
        
        self.send_secure_packet(CMD_TARGET, payload_bytes)
        
        # Start Timer for Retry? (Simplified: Just log sending)
        self.res_val_lbl.config(text="TRANSMITTING...", foreground="orange")

    def send_secure_packet(self, cmd_id, raw_payload):
        # 2. Generate Salt
        salt = random.randint(0, 255)
        
        # 3. Encrypt (Rolling XOR)
        # Key = Master ^ Salt
        session_key = MASTER_KEY ^ salt
        encrypted_payload = bytearray()
        
        for i, byte in enumerate(raw_payload):
            enc_byte = byte ^ ((session_key + i) & 0xFF)
            encrypted_payload.append(enc_byte)
            
        # 4. Build Packet
        # [STX] [CMD] [SALT] [LEN] [PAYLOAD...] [CRC] [ETX]
        packet = bytearray()
        packet.append(STX)
        packet.append(cmd_id)
        packet.append(salt)
        length = len(encrypted_payload)
        packet.append(length)
        packet.extend(encrypted_payload)
        
        # CRC Calc (CMD ~ PAYLOAD)
        crc_data = bytearray([cmd_id, salt, length])
        crc_data.extend(encrypted_payload)
        crc = self.calc_crc8(crc_data)
        packet.append(crc)
        
        packet.append(ETX)
        
        # Send
        self.ser.write(packet)
        self.log(f"SENT: {packet.hex().upper()}", "TX")
        self.log(f"RAW: {raw_payload.decode()}", "SYS")
        
        # Save for retry (omitted in this basic snippet, but structure ready)

    def serial_listener(self):
        buffer = b""
        while self.running:
            if self.connected and self.ser and self.ser.in_waiting:
                try:
                    data = self.ser.read(self.ser.in_waiting)
                    if data:
                        # Simple Parser for ACK (Assuming Text Response for now as per Roadmap)
                        # Or mixed. Let's just dump to log and check for ACK keywords
                        msg = data.decode(errors='ignore')
                        
                        # Check for ACK
                        if "[ACK]" in msg:
                            # Parse Result "AZ:3200..."
                            clean_msg = msg.split("[ACK]")[1].strip()
                            self.root.after(0, lambda m=clean_msg: self.update_result(m, True))
                            self.root.after(0, lambda m=msg: self.log(m.strip(), "RX"))
                        elif "[ERR]" in msg:
                            self.root.after(0, lambda: self.update_result("TRANSMISSION FAILED", False))
                            self.root.after(0, lambda m=msg: self.log(m.strip(), "RX"))
                        else:
                             # Just Log
                             self.root.after(0, lambda m=msg: self.log(m.strip(), "RX"))
                except Exception as e:
                    print(e)
            time.sleep(0.05)

    def update_result(self, text, success):
        self.res_val_lbl.config(text=text, foreground="blue" if success else "red")

if __name__ == "__main__":
    root = tk.Tk()
    app = FCS_ClientApp(root)
    root.mainloop()
