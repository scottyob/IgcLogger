#include "IgcLogger.h"

void setup() {
  Serial.begin(9600);
  delay(2000);

  IgcLogger logger(Serial);

  // Write the header the header
  strcpy(logger.date, "012025");
  logger.pilot = "Some Pilot";
  logger.glider_type = "Awesome Glider 19";
  logger.firmware_version = "1.0.0";
  logger.hardware_version = "Some Variometer";
  logger.logger_type = "Igc-Logger based Vario";
  logger.gps_type = "uBlox 7";
  logger.pressure_type = "MS5611";
  logger.time_zone = "-8";
  logger.writeHeader();

  // Add I records to extend our fixes with "FXA" and
  // "ISU" records (GPS Fix Accuracy, and Satellites In Use)
  const IRecordExtension extensions[] = {IRecordExtension(3, "FXA"), IRecordExtension(2, "SIU")};
  logger.writeIRecord(sizeof(extensions) / sizeof(extensions[0]), extensions);

  // Write a task declaration before the first B record
  logger.writeCDeclarationRecord("012025", "195815", "000000", "0000", 1, "Training task");
  logger.writeCPointRecord("0000000N", "00000000E");
  logger.writeCPointRecord("3728466N", "12151573W", "START Start Point");
  logger.writeCPointRecord("3729466N", "12152573W", "TURN Turn Point");
  logger.writeCPointRecord("3730466N", "12153573W", "FINISH Finish Point");
  logger.writeCPointRecord("0000000N", "00000000E");
  logger.writeLRecord("OZN=0,Style=1,R1=1000.0m,A1=180.0,R2=0.0m,A2=0.0");

  // Write some B records
  logger.writeBRecord("195816", "3728466N", "12151573W", true, 696, 694, "00504");
  logger.writeBRecord("195817", "3728464N", "12151574W", true, 695, 693, "00504");
  logger.writeBRecord("195818", "3728462N", "12151575W", true, 694, 693, "00504");
  logger.writeERecord("195819", "PEV", "SAVE POINT");
  logger.writeBRecord("195819", "3728460N", "12151576W", true, 693, 692, "00504");
  logger.writeBRecord("195820", "3728459N", "12151576W", true, 693, 692, "00504");
  logger.writeBRecord("195821", "3728457N", "12151577W", true, 692, 691, "00504");
  logger.writeBRecord("195822", "3728455N", "12151578W", true, 692, 691, "00504");
  logger.writeBRecord("195823", "3728454N", "12151580W", true, 692, 691, "00504");
  logger.writeBRecord("195824", "3728452N", "12151580W", true, 692, 691, "00504");
  logger.writeBRecord("195825", "3728450N", "12151580W", true, 692, 690, "00504");
  logger.writeBRecord("195826", "3728448N", "12151579W", true, 692, 690, "00504");
  logger.writeBRecord("195827", "3728446N", "12151579W", true, 691, 690, "00504");

  logger.writeGRecord();
}

void loop() {}
