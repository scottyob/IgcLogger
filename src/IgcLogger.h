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
  explicit IgcLogger(Stream &ostream) : ostream(ostream) {}

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

  // Logs a comment to the file
  void writeLRecord(const String &comment);

  // Writes the H record (security).  Currently not implemented
  void writeHRecord();

  // For the H records.
  char date[7] = "";
  String pilot;
  String glider_type;
  String firmware_version;
  String hardware_version;
  String logger_type;
  String gps_type;
  String pressure_type;
  String time_zone;  // (optional)

 private:
  Stream &ostream;

  // For use in the A record
  char manufacturer_id[4] = "XSI";
  char logger_id[4] = "Igc";
  String id_extension = "LoggerLib";
};