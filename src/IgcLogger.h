#include <Arduino.h>

// ---------------------------------------------------------------------------
// IGC security (G record) support
//
// FAI Sporting Code Section 7H (CIVL Flight Recorder Specification), para
// 3.1.4.3, allows HG/PG flight recorders to use "an industry-standard message
// authentication system like HMAC" instead of the asymmetric per-instrument
// keys required of IGC-approved sailplane recorders, and recommends
// "a minimum of HMAC-SHA256 ... using a 256-bit key".
//
// The signing key MUST NOT be published.  See vali.fai-civl.org FAQ,
// "What happen if my key to encrypt is public available as Open Source":
// GPL/CDDL do not require publishing keys; obfuscate/inject them at build time.
//
// Define IGC_SIGNING_KEY at build time (e.g. platformio.ini build_flags:
//   -DIGC_SIGNING_KEY='"<32+ random bytes as a C string>"'
// ) or call IgcLogger::setSigningKey() before writeHeader().  If neither is
// done the library signs with a well-known placeholder and isSigned() is false.
// ---------------------------------------------------------------------------
#ifndef IGC_SIGNING_KEY
#define IGC_SIGNING_KEY "PLACEHOLDER-UNSIGNED-KEY-REPLACE-AT-BUILD-TIME"
#define IGC_SIGNING_KEY_IS_PLACEHOLDER 1
#endif

// Minimal, dependency-free SHA-256 so the library stays portable across
// Arduino cores (no mbedTLS / BearSSL requirement) and is unit-testable on a
// host compiler.
struct IgcSha256 {
  uint32_t state[8];
  uint64_t bit_length;
  uint8_t buffer[64];
  uint8_t buffer_length;

  void begin();
  void update(const uint8_t *data, size_t length);
  void finish(uint8_t digest[32]);
};

// Incremental HMAC-SHA256 (RFC 2104).
struct IgcHmacSha256 {
  IgcSha256 inner;
  uint8_t opad[64];

  void begin(const uint8_t *key, size_t key_length);
  void update(const uint8_t *data, size_t length);
  void finish(uint8_t mac[32]);
};

struct IRecordExtension {
  uint8_t size;

  char code[4];  // 3 characters and a null terminator

  IRecordExtension(uint8_t size, const char *code) : size(size) {
    strncpy(this->code, code, 3);
    this->code[3] = '\0';
  }
};

class IgcLogger {
 public:
  explicit IgcLogger(Print &ostream) : ostream(&ostream) {}
  explicit IgcLogger() : ostream(NULL) {}

  void writeHeader();

  // Sets the manufacturer ID as used in the header
  // See https://xp-soaring.github.io/igc_file_format/igc_format_2008.html#link_2.5.6
  // This should be a 3 character string starting with 'X', unless you have a device tested with
  // GFAC.
  //
  // IMPORTANT: the code must be one that CIVL has allocated to *you*
  // (http://vali.fai-civl.org/supported.html).  Re-using another project's
  // code makes online contests run that project's vali-XXX.exe against your
  // files, which will always report FAILED.
  void setManufacturerId(const char *manufacturer_id);

  // Sets the logger ID.  This is used in the A record to identify a different
  // logger from the same manufacturer.  This should be a 3 character string.
  void setLoggerId(const char *logger_id);

  // Sets the ID extension for the A record.  This can be used to identify
  // anything about either the logger, or the specific flight
  void setIdExtension(const String &id_extension) { this->id_extension = id_extension; }

  // Sets the secret key used to sign the file (G record).  Must be called
  // before writeHeader().  A 256-bit (32 byte) or longer key is recommended.
  void setSigningKey(const uint8_t *key, size_t key_length);

  // True if a key other than the compiled-in placeholder is in use, i.e. the
  // G record this file will carry is actually meaningful.
  bool isSigned() const { return signing_key_is_real; }

  // Logs an of I record defining extra attributes.  These are used to store additional information
  // These should at least contain an FXA attribute, which is the Fix Accuracy.
  // https://xp-soaring.github.io/igc_file_format/igc_format_2008.html#link_3.4
  void writeIRecord(uint8_t num_extensions, const IRecordExtension *extensions);

  // Logs the first line of a task/declaration (C record).
  // This should be written after the H, I and J records, and before the first B record.
  // Arguments:
  //   @param declaration_date 6 bytes, UTC date in the format DDMMYY
  //   @param declaration_time 6 bytes, UTC time in the format HHMMSS
  //   @param flight_date 6 bytes, intended flight date in the format DDMMYY, or 000000
  //   @param task_number 4 bytes, alphanumeric task number, or 0000
  //   @param turnpoint_count number of turn points excluding start and finish
  //   @param description optional text string to append after the turnpoint count
  void writeCDeclarationRecord(String declaration_date, String declaration_time, String flight_date,
                               String task_number, uint8_t turnpoint_count,
                               const String &description = "");

  // Logs a task/declaration point (C record).
  // Arguments:
  //   @param latitude 8 bytes, in the format of DDMMmmmN/S
  //   @param longitude 9 bytes, in the format of DDDMMmmmE/W
  //   @param description optional point description, such as TAKEOFF, START, TURN, FINISH or LANDING
  void writeCPointRecord(String latitude, String longitude, const String &description = "");

  // @brief Logs a position (B record) with the mandatory fields.
  // Arguments:
  //   @param time 6 bytes, in the format of HHMMSS
  //   @param latitude 8 bytes, in the format of DDMMmmmN/S
  //   @param longitude 9 bytes, in the format of DDDMMmmmE/W
  //   @param gps_fix true if the GPS is a 3D fix
  //   @param pressure_altitude 5 bytes, in the format of PPPPP
  //   @param gps_altitude 5 bytes, in the format of GGGGG
  //   @param extension any additional data to be added to the record (as dictated by the I record)
  void writeBRecord(String time, String latitude, String longitude, bool gps_fix,
                    int pressure_altitude, int gps_altitude, String extension);

  // Logs an event (E record) with a three-letter code and optional text.
  // Arguments:
  //   @param time 6 bytes, in the format of HHMMSS
  //   @param code 3 bytes, three-letter code from the IGC spec, such as PEV
  //   @param text optional text string to append after the code
  void writeERecord(String time, const char *code, const String &text = "");

  // Logs a comment to the file
  void writeLRecord(const String &comment);

  // Writes the G record (security): the HMAC-SHA256 of every record written so
  // far, as uppercase hex.  Must be the last thing written to the file.
  void writeGRecord();

  // For the H records.
  char date[7] = "";
  uint16_t fix_accuracy = 35;  // HFFXA: overall fix accuracy in meters
  String pilot;
  String crew2 = "NIL";      // HFCM2: second crew member, or NIL
  String glider_type;
  String glider_id = "NKN";  // HFGID: glider registration, or NKN
  String firmware_version;
  String hardware_version;
  String logger_type;
  String gps_type;
  String pressure_type;
  // Altitude datum declarations required of non-IGC (CIVL) flight recorders,
  // FAI SC7H para 3.2.3.  GNSS altitude above the WGS84 geoid is "GEO"
  // (what NMEA GGA reports); above the ellipsoid is "ELL".
  String gnss_altitude_datum = "GEO";      // HFALG: GEO | ELL | NKM | NIL
  String pressure_altitude_datum = "ISA";  // HFALP: ISA | MSL | NKM | NIL
  String time_zone;  // (optional)

  void setOutput(Print& target) {
        ostream = &target;
  }

 private:
  Print* ostream;

  // For use in the A record
  char manufacturer_id[4] = "XSI";
  char logger_id[4] = "Igc";
  String id_extension = "LoggerLib";

  // Security state.  Every record emitted is fed into the HMAC as its bytes
  // followed by a single '\n', so the signature is unaffected by CRLF/LF
  // translation while the file is copied around or uploaded.
  IgcHmacSha256 hmac;
  bool hmac_started = false;
  bool signing_key_is_real =
#ifdef IGC_SIGNING_KEY_IS_PLACEHOLDER
      false;
#else
      true;
#endif
  uint8_t signing_key[64];
  size_t signing_key_length = 0;

  void startSecurity();
  // Writes one record to the output and adds it to the running signature.
  void emitRecord(const String &record);
};
