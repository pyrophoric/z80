int addr_pins[] = { 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34 };
int data_pins[] = { 38, 39, 40, 41, 42, 43, 44, 45 };
int ce = 48;
int we = 49;
int oe = 50;

int LF = 10;

char record[267];
int record_len = 0;
int data_len = 0;
int target_addr = 0;

int data_buffer[256];

bool DEBUG = false;

void setup() {
  Serial.begin(9600);
  
  for (int i = 0; i < 13; ++i) {
    pinMode(addr_pins[i], OUTPUT);
  }
  for (int i = 0; i < 8; ++i) {
    pinMode(data_pins[i], OUTPUT);
    digitalWrite(data_pins[i], LOW);
  }
  pinMode(ce, OUTPUT); digitalWrite(ce, HIGH);
  pinMode(we, OUTPUT); digitalWrite(we, HIGH);
  pinMode(oe, OUTPUT); digitalWrite(oe, HIGH);
}

void loop() {
  if (Serial.available() > 0) {
    int c = Serial.read();
    if (c == LF) {
      if (record[0] == ':') {
        if (process_record()) {
          if (DEBUG) Serial.println("Record valid. Writing data...");
          write_eeprom(target_addr, data_len);
          if (DEBUG) Serial.println("Done writing. Checking data...");
          for (int i = 0; i < data_len; ++i) {
            int val = read_eeprom(target_addr);
            if (val != data_buffer[i]) {
              Serial.print("ERROR: value at "); Serial.print(target_addr, HEX); Serial.print(": "); Serial.print(val, HEX);Serial.print(" != "); Serial.println(data_buffer[i], HEX);
            }
            target_addr++;
          }
          Serial.println("OK");
        }
      }
      else {
        process_command();
      }
      record_len = 0;
      data_len = 0;
      target_addr = 0;
    }
    else {
      record[record_len] = c;
      record_len++;
    }
  }
}

void process_command() {
  // sanity check values
  // read and send over serial
  if (record[0] == 'r') {
    int start_upper = process_byte(record[1], record[2]);
    if (start_upper == -1) return;
    int start_lower = process_byte(record[3], record[4]);
    if (start_lower == -1) return;
    int start = start_upper * 256 + start_lower;
    int len_upper = process_byte(record[6], record[7]);
    if (len_upper == -1) return;
    int len_lower = process_byte(record[8], record[9]);
    if (len_lower == -1) return;
    int len = len_upper * 256 + len_lower;
    for (int i = 0; i < len; ++i) {
      int b = read_eeprom(start+i);
      if (b < 16) Serial.print('0');
      Serial.print(b, HEX);
      if (i % 16 == 15) {
        Serial.println();
      }
      else {
        Serial.print(" ");
      }
    }
  }
  else {
    Serial.println("Unrecognized command.");
  }
}

bool process_record() {
  // running total for checksum
  int calc_checksum = 0;

  // length check
  if (record_len < 11) {
    Serial.println("ERROR: record too short to be valid.");
    return false;
  }

  // data length
  data_len = process_byte(record[1], record[2]);
  if (data_len == -1) return false;
  if (DEBUG) {
    Serial.print("Data length: "); Serial.println(data_len, HEX);
  }
  if (data_len > 128) {
    Serial.println("ERROR: too many data bytes (only 128 supported).");
    return false;
  }
  calc_checksum += data_len;
  if (record_len < 11 + data_len * 2) {
    Serial.println("ERROR: too few data bytes.");
    return false;
  }

  // address
  int addr_upper = process_byte(record[3], record[4]);
  if (addr_upper == -1) return false;
  calc_checksum += addr_upper;
  int addr_lower = process_byte(record[5], record[6]);
  if (addr_lower == -1) return false;
  calc_checksum += addr_lower;
  target_addr = addr_upper * 256 + addr_lower;
  if (DEBUG) {
    Serial.print("Address: "); Serial.println(target_addr, HEX);
  }

  // record type
  int rec_type = process_byte(record[7], record[8]);
  if (rec_type == -1) return false;
  calc_checksum += rec_type;
  if (rec_type < 0 || rec_type > 1) {
    Serial.print("ERROR: unsupported record type: "); Serial.println(rec_type, HEX);
    return false;
  }
  if (DEBUG) {
    Serial.print("Record Type: "); Serial.println(rec_type, HEX);
  }

  // data body
  if (DEBUG) Serial.print("Data: ");
  int i;
  for (i = 0; i < data_len; ++i) {
    data_buffer[i] = process_byte(record[9 + i*2], record[10 + i*2]);
    if (data_buffer[i] == -1) return false;
    calc_checksum += data_buffer[i];
    if (DEBUG) {
      Serial.print(data_buffer[i], HEX); Serial.print(" ");
    }
  }

  // checksum
  int checksum = process_byte(record[9 + i*2], record[10 + i*2]);
  if (checksum == -1) return false;
  if (DEBUG) { 
    Serial.print("Checksum (read): "); Serial.println(checksum, HEX);
  }
  calc_checksum = (byte)(~calc_checksum + 1); // calculate actual checksum
  if (DEBUG) {
    Serial.print("Checksum (calc): "); Serial.println(calc_checksum, HEX);
  }
  if (calc_checksum != checksum) {
    Serial.println("ERROR: read checksum doesn't match calculated.");
    return false;
  }

  return true;
}

int process_byte(char upper_c, char lower_c) {
  int upper = from_char(upper_c);
  if (upper == -1) {
    Serial.print("ERROR: char is not a number: "); Serial.println(upper_c);
    return -1;
  }
  int lower = from_char(lower_c);
  if (lower == -1) {
    Serial.print("ERROR: char is not a number: "); Serial.println(lower_c);
    return -1;
  }
  return upper * 16 + lower;
}

int from_char(char c) {
  if (c >= '0' && c <= '9') {
    return c - 48;
  }
  else if (c >= 'A' && c <= 'F') {
    return c - 55;
  }
  else if (c >= 'a' && c <= 'f') {
    return c - 87;
  }
  else {
    return -1;
  }
}

int read_eeprom(int addr) {
  for (int i = 0; i < 13; ++i) {
    digitalWrite(addr_pins[i], get_bit(addr, i));
  }
  int ret = 0;
  for (int i = 0; i < 8; ++i) {
    pinMode(data_pins[i], INPUT);
  }
  digitalWrite(oe, LOW);
  digitalWrite(ce, LOW);
  for (int i = 0; i < 8; ++i) {
    if (digitalRead(data_pins[i])) {
      ret += (1 << i);
    }
  }
  digitalWrite(ce, HIGH);
  digitalWrite(oe, HIGH);
  return ret;
}

void write_eeprom(int addr, int len) {
  // set output mode on data pins
  for (int i = 0; i < 8; ++i) {
    pinMode(data_pins[i], OUTPUT);
  }

  // write data in page write mode
  digitalWrite(ce, LOW);
  for (int i = 0; i < len; ++i) {
    // put address on bus
    for (int j = 0; j < 13; ++j) {
      digitalWrite(addr_pins[j], get_bit(addr+i, j));
    }
    delayMicroseconds(20);
    digitalWrite(we, LOW); // latch addr

    for (int j = 0; j < 8; ++j) {
      digitalWrite(data_pins[j], get_bit(data_buffer[i], j));
    }
    delayMicroseconds(20);
    digitalWrite(we, HIGH);
  }
  
  delayMicroseconds(20);
  digitalWrite(we, HIGH);
  delayMicroseconds(20);
  digitalWrite(ce, HIGH);
  delay(50);
}

bool get_bit(int n, int k) {
  return ((1 << k) & n) != 0;
}


