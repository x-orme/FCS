# Project Roadmap

This document outlines the future development plan for the K105A1 FCS project.

## 1. Relax Charge/Range Constraints (Completed)
**Goal:** Expand valid range overlaps for each charge to prevent "Dead Zones" where no charge is valid, while remaining fact-based.
- [x] Analyze current `fcs_math.c` firing tables.
- [x] Adjust Start/End ranges for Charges 1-7 to ensure continuous coverage from min range (1.0km) to max range (11.3km) based on 800mil logic.
- [x] Verify that overlaps exist so the user can choose between two charges at certain ranges.

## 2. Bluetooth Application & Encryption
**Goal:** Implement a secure wireless interface using a custom lightweight protocol.

### 2.1 Protocol Specification (Salted Rolling XOR) (DONE)
- **Structure:** `[STX] [CMD_ID] [SALT] [LEN] [PAYLOAD...] [CRC8] [ETX]`
  - `STX`: 0x02 (Start)
  - `CMD`: Command ID (e.g., 0xA1 for Target Input)
  - `SALT`: Random 1-byte value (Dynamic Factor)
  - `LEN`: Length of Payload
  - `PAYLOAD`: Encrypted Data
  - `CRC8`: Checksum (CMD ~ PAYLOAD)
  - `ETX`: 0x03 (End)

- **Encryption Algorithm (Symmetric):**
  - **Master Key:** `0xA5` (Fixed System Key)
  - **Dynamic Key Generation:** `SessionKey = MasterKey ^ SALT`
  - **Cipher Logic:** `Encrypted[i] = Raw[i] ^ (SessionKey + i)` (Rolling Index)

### 2.2 Tasks (Completed)
- [x] **Mobile App (Concept):** Implemented `fcs_terminal.py` (Modern GUI, Secure Packet Generator).
- [x] **STM32 Firmware:**
  - [x] Update `fcs_core.c` to implement a State Machine Parser.
  - [x] Implement the decryption routine using Salt.
  - [x] Verify CRC8 integrity before processing.
  - [x] Response Mechanism: Redirect ACK/ERR to Bluetooth Port (USART1).

## 3. Security Hardening
**Goal:** Remove debug artifacts for the final release candidate.
- [ ] Disable all plain-text `printf` outputs in `fcs_core.c` and `main.c` (except for the Bluetooth encrypted response if needed).
- [ ] Ensure no side-channel info (like raw coordinates) is leaked via UART.

## 4. Final Refactoring & Documentation
**Goal:** Polish the codebase for portfolio presentation.
- [ ] **Comments:** Add Doxygen-style comments to every function explaining *why* it exists, not just what it does.
- [ ] **Code Cleanup:** Remove any unused variables, commented-out test code, or legacy macros.
- [ ] **Final Architecture Doc:** Update `ARCHITECTURE_DEEP_DIVE.md` to reflect the final state including Bluetooth/Encryption.

## 5. Portfolio Creation
**Goal:** Compile all assets into a compelling story.
- [ ] Gather `Docs` (Roadmap, TroubleShooting, Architecture).
- [ ] Create a "Development Log" summary.
- [ ] Capture/Generate final UI screenshots.
