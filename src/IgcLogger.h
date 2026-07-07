#include <Arduino.h>

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
  void setManufacturerId(const char *manufacturer_id);

  // Sets the logger ID.  This is used in the A record to identify a different
  // logger from the same manufacturer.  This should be a 3 character string.
  void setLoggerId(const char *logger_id);

  // Sets the ID extension for the A record.  This can be used to identify
  // anything about either the logger, or the specific flight
  void setIdExtension(const String &id_extension) { this->id_extension = id_extension; }

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

  // Writes the G record (security).  Currently not implemented
  void writeGRecord();

  // For the H records.
  char date[7] = "";
  uint16_t fix_accuracy = 35;  // HFFXA: overall fix accuracy in meters
  String pilot;
  String glider_type;
  String firmware_version;
  String hardware_version;
  String logger_type;
  String gps_type;
  String pressure_type;
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
};
