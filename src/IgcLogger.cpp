#include "IgcLogger.h"

#include <stdexcept>

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

void IgcLogger::writeHeader() {
  ostream->println("A" + String(manufacturer_id) + logger_id + id_extension);

  // Write the HFDTE (date) record.
  for (int i = 0; i < 6; i++) {
    if (date[i] < '0' || date[i] > '9') {
      throw std::runtime_error("Date must be in the format of DDMMYY");
    }
  }
  ostream->print("HFDTEDATE");
  ostream->println(date);

  // Write the HFPLT (pilot) record.
  if (!pilot.length()) {
    throw std::runtime_error("Pilot name must be set");
  }
  ostream->println("HFPLTPILOTINCHARGE:" + pilot);

  // Write the HFGTYGLIDERTYPE record.
  if (!glider_type.length()) {
    throw std::runtime_error("Glider type must be set");
  }
  ostream->println("HFGTYGLIDERTYPE:" + glider_type);

  // Write the HFDTMGPSDATUM record
  ostream->println("HFDTMGPSDATUM:WGS84");

  // Write the HFRFWFIRMWAREVERSION record
  if (!firmware_version.length()) {
    throw std::runtime_error("Firmware version must be set");
  }
  ostream->println("HFRFWFIRMWAREVERSION:" + firmware_version);

  // Write the HFRHWHARDWAREVERSION record
  if (!hardware_version.length()) {
    throw std::runtime_error("Hardware version must be set");
  }
  ostream->println("HFRHWHARDWAREVERSION:" + hardware_version);

  // Write the HFFTYFRTYPE record (logger free text manufacturer and model)
  if (!logger_type.length()) {
    throw std::runtime_error("Logger type must be set");
  }
  ostream->println("HFFTYFRTYPE:" + logger_type);

  // Write the HFGPSTYPE record (GPS type)
  if (!gps_type.length()) {
    throw std::runtime_error("GPS type must be set");
  }
  ostream->println("HFGPSTYPE:" + gps_type);

  // Write the HFPRSPRESSALTSENSOR record
  if (!pressure_type.length()) {
    throw std::runtime_error("Pressure sensor type must be set");
  }
  ostream->println("HFPRSPRESSALTSENSOR:" + pressure_type);

  // Write the HFTZNTIMEZONE (Timezone), if set.
  if (time_zone.length()) {
    ostream->println("HFTZNTIMEZONE:" + time_zone);
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
  ostream->println(b_record + extension);
}

void IgcLogger::writeLRecord(const String &comment) {
  ostream->println("L" + (String)manufacturer_id + comment);
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
    auto end_byte = currentOffset + extensions[i].size;

    char start_byte_str[3];
    char end_byte_str[3];
    sprintf(start_byte_str, "%02d", start_byte);
    sprintf(end_byte_str, "%02d", end_byte);
    i_record += start_byte_str;
    i_record += end_byte_str;

    i_record += extensions[i].code;
    currentOffset = end_byte + 1;
  }
  ostream->println(i_record);
}

void IgcLogger::writeGRecord() { ostream->println("GNotImplemented"); }
