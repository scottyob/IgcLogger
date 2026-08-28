#include "IgcLogger.h"

#include <ctype.h>
#include <stdexcept>

// ===========================================================================
// SHA-256 / HMAC-SHA256
// Straight FIPS 180-4 / RFC 2104; no external crypto dependency so the library
// builds on any Arduino core and can be exercised on a host compiler.
// ===========================================================================
namespace {
  const uint32_t kSha256K[64] = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
      0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
      0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
      0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
      0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
      0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
      0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
      0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
      0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
      0xc67178f2u};

  inline uint32_t ror32(uint32_t value, int bits) {
        return (value >> bits) | (value << (32 - bits));
  }

  void sha256Compress(uint32_t state[8], const uint8_t block[64]) {
        uint32_t w[64];
    for (int i = 0; i < 16; i++) {
      w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
                     ((uint32_t)block[i * 4 + 2] << 8) | ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++) {
      uint32_t s1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);
      uint32_t ch = (e & f) ^ ((~e) & g);
      uint32_t temp1 = h + s1 + ch + kSha256K[i] + w[i];
      uint32_t s0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);
      uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t temp2 = s0 + maj;

      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
  }
}  // namespace

void IgcSha256::begin() {
    state[0] = 0x6a09e667u;
  state[1] = 0xbb67ae85u;
  state[2] = 0x3c6ef372u;
  state[3] = 0xa54ff53au;
  state[4] = 0x510e527fu;
  state[5] = 0x9b05688cu;
  state[6] = 0x1f83d9abu;
  state[7] = 0x5be0cd19u;
  bit_length = 0;
  buffer_length = 0;
}

void IgcSha256::update(const uint8_t *data, size_t length) {
    bit_length += (uint64_t)length * 8;
  while (length > 0) {
    size_t take = 64 - buffer_length;
    if (take > length) take = length;
    memcpy(buffer + buffer_length, data, take);
    buffer_length += take;
    data += take;
    length -= take;
    if (buffer_length == 64) {
      sha256Compress(state, buffer);
      buffer_length = 0;
    }
  }
}

void IgcSha256::finish(uint8_t digest[32]) {
    uint64_t total_bits = bit_length;
  uint8_t pad = 0x80;
  update(&pad, 1);
  uint8_t zero = 0x00;
  while (buffer_length != 56) update(&zero, 1);

  uint8_t length_bytes[8];
  for (int i = 0; i < 8; i++) length_bytes[7 - i] = (uint8_t)((total_bits >> (i * 8)) & 0xFF);
  // update() would corrupt bit_length, but we have already captured it.
  memcpy(buffer + buffer_length, length_bytes, 8);
  sha256Compress(state, buffer);
  buffer_length = 0;

  for (int i = 0; i < 8; i++) {
    digest[i * 4] = (uint8_t)(state[i] >> 24);
    digest[i * 4 + 1] = (uint8_t)(state[i] >> 16);
    digest[i * 4 + 2] = (uint8_t)(state[i] >> 8);
    digest[i * 4 + 3] = (uint8_t)(state[i]);
  }
}

void IgcHmacSha256::begin(const uint8_t *key, size_t key_length) {
    uint8_t block_key[64];
  memset(block_key, 0, sizeof(block_key));
  if (key_length > 64) {
    IgcSha256 shrink;
    shrink.begin();
    shrink.update(key, key_length);
    shrink.finish(block_key);
  } else {
    memcpy(block_key, key, key_length);
  }

  uint8_t ipad[64];
  for (int i = 0; i < 64; i++) {
    ipad[i] = block_key[i] ^ 0x36;
    opad[i] = block_key[i] ^ 0x5C;
  }

  inner.begin();
  inner.update(ipad, sizeof(ipad));
  memset(ipad, 0, sizeof(ipad));
  memset(block_key, 0, sizeof(block_key));
}

void IgcHmacSha256::update(const uint8_t *data, size_t length) { inner.update(data, length); }

void IgcHmacSha256::finish(uint8_t mac[32]) {
    uint8_t inner_digest[32];
  inner.finish(inner_digest);

  IgcSha256 outer;
  outer.begin();
  outer.update(opad, sizeof(opad));
  outer.update(inner_digest, sizeof(inner_digest));
  outer.finish(mac);
}

namespace {
  void validateDigits(const String &value, uint8_t length, const char *field_name) {
        if (value.length() != length) {
      throw std::runtime_error((String(field_name) + " has invalid length").c_str());
        }
    for (auto digit : value) {
      if (digit < '0' || digit > '9') {
        throw std::runtime_error((String(field_name) + " must contain only digits").c_str());
      }
    }
  }

  void validateAlphanumeric(const String &value, uint8_t length, const char *field_name) {
        if (value.length() != length) {
      throw std::runtime_error((String(field_name) + " has invalid length").c_str());
        }
    for (auto character : value) {
      if (!isalnum(character)) {
        throw std::runtime_error((String(field_name) + " must be alphanumeric").c_str());
      }
    }
  }

  void validatePrintableAscii(const String &value, const char *field_name) {
        for (auto character : value) {
      if (character < 0x20 || character > 0x7E) {
        throw std::runtime_error((String(field_name) + " must contain printable ASCII characters").c_str());
      }
        }
  }

  void validateLatitude(const String &latitude) {
        if (latitude.length() != 8) {
      throw std::runtime_error("Latitude must be 8 characters long");
        }
    if (latitude[7] != 'N' && latitude[7] != 'S') {
      throw std::runtime_error("Latitude must end in N or S");
    }
    for (int i = 0; i < 7; i++) {
      if (latitude[i] < '0' || latitude[i] > '9') {
        throw std::runtime_error("Latitude must be in the format DDMMmmmN/S");
      }
    }
  }

  void validateLongitude(const String &longitude) {
        if (longitude.length() != 9) {
      throw std::runtime_error("Longitude must be 9 characters long");
        }
    if (longitude[8] != 'E' && longitude[8] != 'W') {
      throw std::runtime_error("Longitude must end in E or W");
    }
    for (int i = 0; i < 8; i++) {
      if (longitude[i] < '0' || longitude[i] > '9') {
        throw std::runtime_error("Longitude must be in the format DDDMMmmmE/W");
      }
    }
  }
}  // namespace

void IgcLogger::setManufacturerId(const char *manufacturer_id) {
    if (strlen(manufacturer_id) > 3) {
    // Throw a runtime error
    throw std::runtime_error("Manufacturer ID must be 3 characters long");
    }
  strncpy(this->manufacturer_id, manufacturer_id, 3);
}

void IgcLogger::setLoggerId(const char *logger_id) {
    if (strlen(logger_id) > 3) {
    // Throw a runtime error
    throw std::runtime_error("Logger ID must be 3 characters long");
    }
  strncpy(this->logger_id, logger_id, 3);
}

void IgcLogger::setSigningKey(const uint8_t *key, size_t key_length) {
    if (key_length == 0 || key_length > sizeof(signing_key)) {
    throw std::runtime_error("Signing key must be between 1 and 64 bytes");
    }
  memcpy(signing_key, key, key_length);
  signing_key_length = key_length;
  signing_key_is_real = true;
}

void IgcLogger::startSecurity() {
    if (signing_key_length == 0) {
    const char *fallback = IGC_SIGNING_KEY;
    size_t length = strlen(fallback);
    if (length > sizeof(signing_key)) length = sizeof(signing_key);
    memcpy(signing_key, fallback, length);
    signing_key_length = length;
    }
  hmac.begin(signing_key, signing_key_length);
  hmac_started = true;
}

void IgcLogger::emitRecord(const String &record) {
    if (!hmac_started) startSecurity();

  // FAI SC7H 3.1.4.2: every record is protected except H records with the "O"
  // (observer) source and L records that do not carry our own three-letter
  // code.  This library never emits either, so everything written here is
  // covered.  Hash the record bytes plus a single '\n' so the signature is
  // stable across CRLF <-> LF translation in transit.
  hmac.update((const uint8_t *)record.c_str(), record.length());
  const uint8_t newline = '\n';
  hmac.update(&newline, 1);

  ostream->println(record);
}

void IgcLogger::writeHeader() {
    startSecurity();

  emitRecord("A" + String(manufacturer_id) + logger_id + id_extension);

  // Write the HFDTE (date) record.
  for (int i = 0; i < 6; i++) {
    if (date[i] < '0' || date[i] > '9') {
      throw std::runtime_error("Date must be in the format of DDMMYY");
    }
  }
  emitRecord("HFDTE" + String(date));

  // Write the HFFXA (fix accuracy) record.
  char fix_accuracy_str[4];
  snprintf(fix_accuracy_str, sizeof(fix_accuracy_str), "%03d", fix_accuracy);
  emitRecord("HFFXA" + String(fix_accuracy_str));

  // Write the HFPLT (pilot) record.
  if (!pilot.length()) {
    throw std::runtime_error("Pilot name must be set");
  }
  emitRecord("HFPLTPILOTINCHARGE:" + pilot);

  // Write the HFCM2 (second crew member) record.
  emitRecord("HFCM2CREW2:" + (crew2.length() ? crew2 : String("NIL")));

  // Write the HFGTYGLIDERTYPE record.
  if (!glider_type.length()) {
    throw std::runtime_error("Glider type must be set");
  }
  emitRecord("HFGTYGLIDERTYPE:" + glider_type);

  // Write the HFGID (glider registration) record.
  emitRecord("HFGIDGLIDERID:" + (glider_id.length() ? glider_id : String("NKN")));

  // Write the HFDTMGPSDATUM record
  emitRecord("HFDTM100GPSDATUM:WGS84");

  // Write the HFRFWFIRMWAREVERSION record
  if (!firmware_version.length()) {
    throw std::runtime_error("Firmware version must be set");
  }
  emitRecord("HFRFWFIRMWAREVERSION:" + firmware_version);

  // Write the HFRHWHARDWAREVERSION record
  if (!hardware_version.length()) {
    throw std::runtime_error("Hardware version must be set");
  }
  emitRecord("HFRHWHARDWAREVERSION:" + hardware_version);

  // Write the HFFTYFRTYPE record (logger free text manufacturer and model)
  if (!logger_type.length()) {
    throw std::runtime_error("Logger type must be set");
  }
  emitRecord("HFFTYFRTYPE:" + logger_type);

  // Write the HFGPSTYPE record (GPS type)
  if (!gps_type.length()) {
    throw std::runtime_error("GPS type must be set");
  }
  emitRecord("HFGPSTYPE:" + gps_type);

  // Write the HFPRSPRESSALTSENSOR record
  if (!pressure_type.length()) {
    throw std::runtime_error("Pressure sensor type must be set");
  }
  emitRecord("HFPRSPRESSALTSENSOR:" + pressure_type);

  // Altitude datum records required of non-IGC flight recorders (FAI SC7H 3.2.3).
  emitRecord("HFALGALTGPS:" + gnss_altitude_datum);
  emitRecord("HFALPALTPRESSURE:" + pressure_altitude_datum);

  // Write the HFTZNTIMEZONE (Timezone), if set.
  if (time_zone.length()) {
    emitRecord("HFTZNTIMEZONE:" + time_zone);
  }
}

void IgcLogger::writeBRecord(String time, String latitude, String longitude, bool gps_fix,
                             int pressure_altitude, int gps_altitude, String extension) {
    // Check the time is 6 bytes, and in the format of HHMMSS
  if (time.length() != 6) {
    throw std::runtime_error("Time must be 6 characters long");
  }
  for (auto digit : time) {
    if (digit < '0' || digit > '9') {
      throw std::runtime_error("Time must be in the format HHMMSS");
    }
  }

  String b_record = "B";
  b_record += time;

  // Latitude is 8 bytes, in the format of DDMMmmmN/S
  if (latitude.length() != 8) {
    throw std::runtime_error("Latitude must be 8 characters long");
  }
  if (latitude[7] != 'N' && latitude[7] != 'S') {
    throw std::runtime_error("Latitude must end in N or S");
  }
  for (int i = 0; i < 7; i++) {
    if (latitude[i] < '0' || latitude[i] > '9') {
      throw std::runtime_error("Latitude must be in the format DDMMmmmN/S");
    }
  }
  b_record += latitude;

  // Longitude is 9 bytes, in the format of DDDMMmmmE/W
  if (longitude.length() != 9) {
    throw std::runtime_error("Longitude must be 9 characters long");
  }
  if (longitude[8] != 'E' && longitude[8] != 'W') {
    throw std::runtime_error("Longitude must end in E or W");
  }
  for (int i = 0; i < 8; i++) {
    if (longitude[i] < '0' || longitude[i] > '9') {
      throw std::runtime_error("Longitude must be in the format DDDMMmmmE/W");
    }
  }
  b_record += longitude;

  // Fix valid is a single character, either 'A' or 'V'
  if (gps_fix) {
    b_record += 'A';
  } else {
    b_record += 'V';
  }

  // Pressure altitude is 5 bytes, in the format of PPPPP
  // to the ICAO ISA above the 1013.25 HPa sea level datum, valid characters 0-9 and negative sign
  // "-". Negative values to have negative sign instead of leading zero Pressure altitude is an
  // integer, format it to 5 characters with leading zeros or negative sign
  char pressure_altitude_str[6];
  snprintf(pressure_altitude_str, sizeof(pressure_altitude_str), "%05d", pressure_altitude);
  b_record += pressure_altitude_str;

  // GNSS Altitude is an integer, format it to 5 characters with leading zeros
  char gps_altitude_str[6];
  snprintf(gps_altitude_str, sizeof(gps_altitude_str), "%05d", gps_altitude);
  b_record += gps_altitude_str;

  // Print the B record
  emitRecord(b_record + extension);
}

void IgcLogger::writeCDeclarationRecord(String declaration_date, String declaration_time,
                                        String flight_date, String task_number,
                                        uint8_t turnpoint_count, const String &description) {
    validateDigits(declaration_date, 6, "Declaration date");
  validateDigits(declaration_time, 6, "Declaration time");
  validateDigits(flight_date, 6, "Flight date");
  validateAlphanumeric(task_number, 4, "Task number");
  validatePrintableAscii(description, "Declaration description");

  if (turnpoint_count > 99) {
    throw std::runtime_error("Turnpoint count must be less than 100");
  }

  if (description.length() > 51) {
    throw std::runtime_error("Declaration description must fit within the IGC 76 character line limit");
  }

  char turnpoint_count_str[3];
  snprintf(turnpoint_count_str, sizeof(turnpoint_count_str), "%02d", turnpoint_count);

  emitRecord(String("C") + declaration_date + declaration_time + flight_date + task_number +
                 String(turnpoint_count_str) + description);
}

void IgcLogger::writeCPointRecord(String latitude, String longitude, const String &description) {
    validateLatitude(latitude);
  validateLongitude(longitude);
  validatePrintableAscii(description, "Point description");

  if (description.length() > 58) {
    throw std::runtime_error("Point description must fit within the IGC 76 character line limit");
  }

  emitRecord(String("C") + latitude + longitude + description);
}

void IgcLogger::writeLRecord(const String &comment) {
    emitRecord("L" + (String)manufacturer_id + comment);
}

void IgcLogger::writeERecord(String time, const char *code, const String &text) {
    // Check the time is 6 bytes, and in the format of HHMMSS
  if (time.length() != 6) {
    throw std::runtime_error("Time must be 6 characters long");
  }
  for (auto digit : time) {
    if (digit < '0' || digit > '9') {
      throw std::runtime_error("Time must be in the format HHMMSS");
    }
  }

  if (code == NULL || strlen(code) != 3) {
    throw std::runtime_error("Event code must be 3 characters long");
  }
  for (int i = 0; i < 3; i++) {
    if (!isalnum(code[i])) {
      throw std::runtime_error("Event code must be alphanumeric");
    }
  }

  if (text.length() > 66) {
    throw std::runtime_error("Event text must fit within the IGC 76 character line limit");
  }
  for (auto character : text) {
    if (character < 0x20 || character > 0x7E) {
      throw std::runtime_error("Event text must contain printable ASCII characters");
    }
  }

  emitRecord(String("E") + time + String(code) + text);
}

void IgcLogger::writeIRecord(uint8_t num_extensions, const IRecordExtension *extensions) {
    int currentOffset = 36;  // The length of the B record without any extensions.

  String i_record = "I";
  // Add the number of extensions as a 2 byte ASCII to the I record.
  char num_extensions_str[3];
  sprintf(num_extensions_str, "%02d", num_extensions);
  i_record += num_extensions_str;

  for (uint8_t i = 0; i < num_extensions; i++) {
    // Work out the offsets for this extension
    auto start_byte = currentOffset;
    auto end_byte = currentOffset + extensions[i].size - 1;

    char start_byte_str[3];
    char end_byte_str[3];
    sprintf(start_byte_str, "%02d", start_byte);
    sprintf(end_byte_str, "%02d", end_byte);
    i_record += start_byte_str;
    i_record += end_byte_str;

    i_record += extensions[i].code;
    currentOffset = end_byte + 1;
  }
  emitRecord(i_record);
}

void IgcLogger::writeGRecord() {
    // FAI Sporting Code Section 7H para 3.1.4: the recorder must append a
  // digital security signature of the recorded data as a G record.  Para
  // 3.1.4.3 permits a shared secret key with HMAC-SHA256 for HG/PG recorders
  // instead of the per-instrument asymmetric keys required of IGC-approved
  // sailplane recorders.
  //
  // The corresponding validation program (vali-<code>.exe, see
  // http://vali.fai-civl.org/documents/CIVL_IGCValiSpecification_proposal.pdf)
  // recomputes this value over every record up to but not including the G
  // record(s) and compares.
  if (!hmac_started) startSecurity();

  uint8_t mac[32];
  hmac.finish(mac);
  hmac_started = false;

  static const char kHex[] = "0123456789ABCDEF";
  // 64 hex characters plus the leading "G" is 65 bytes, inside the 76
  // character IGC record limit, so a single G record is always enough.
  char line[2 + 64];
  line[0] = 'G';
  for (int i = 0; i < 32; i++) {
    line[1 + i * 2] = kHex[(mac[i] >> 4) & 0x0F];
    line[2 + i * 2] = kHex[mac[i] & 0x0F];
  }
  line[65] = '\0';

  // Not emitRecord(): the G record is not part of the signed data.
  ostream->println(line);
}
