# IgcLogger Library

This Arduino library provides a way for logging a flight in the format of an
IGC file to a Stream (typically on an SD card).

For more information on the igc file format, some good resources are:
* [Aerofiles igc writer](https://github.com/Turbo87/aerofiles/blob/master/aerofiles/igc/writer.py)
* Igc file spec from [xp-soaring](https://xp-soaring.github.io/igc_file_format/igc_format_2008.html#link_3)

## Library Usage.

### Setup Headers.
The A-Record to identify the device requires a manufacturer ID, a logger ID (to tell the difference if there are multiple devices from the same manufacturer), and an Optional Id Extension string to identify either the device or the flight.

Unless your device is tested or registered with GFAC, the manufacturer ID should start
with an 'X'.

#### Default Settings
```
Manufacturer_id: XSI ... for (S)cottyob/(I)gc-Logger
logger_id: Igc
id_extension: LoggerLib

Produces:
AXSIIgcLoggerLib
```

### Setup I Records
Before you can start logging position updates, "I" records, or, any additional attributes to a location update
must also be defined.

the ```logIRecords``` method does this.  If you want to log the GPS Fix Accuracy (FXA) to be between bytes 36 and 40 (just after the manditory fields), and Satellites In Use (SIU), you'd call the following

```
// Add I records to extend our fixes with "FXA" and
// "ISU" records (GPS Fix Accuracy, and Satellites In Use)
const IRecordExtension extensions[] = {IRecordExtension(3, "FXA"), IRecordExtension(2, "SIU")};
logger.writeIRecord(sizeof(extensions) / sizeof(extensions[0]), extensions);
```

### Logging comments
Comments can be logged and useful for however you want to display or parse them.

A popular mobile tool uses formats such as:
```
LXNA::PHASE:onGround
```

To do such a comment as this, you simply need to:
```cpp
logger.writeLRecord("::PHASE:onGround");
```

### Full Example
```cpp
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

// Write some B records
logger.writeBRecord("195816", "3728466N", "12151573W", true, 696, 694, "00504");
logger.writeBRecord("195817", "3728464N", "12151574W", true, 695, 693, "00504");
logger.writeBRecord("195818", "3728462N", "12151575W", true, 694, 693, "00504");
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
```

Will produce:
```
AXSIIgcLoggerLib
HFDTEDATE:012025
HFPLTPILOTINCHARGE:Some Pilot
HFGTYGLIDERTYPE:Awesome Glider 19
HFDTMGPSDATUM:WGS84
HFRFWFIRMWAREVERSION:1.0.0
HFRHWHARDWAREVERSION:Some Variometer
HFFTYFRTYPE:Igc-Logger based Vario
HFGPSTYPE:uBlox 7
HFPRSPRESSALTSENSOR:MS5611
HFTZNTIMEZONE:-8
I023639FXA4042SIU
B1958163728466N12151573WA006960069400504
B1958173728464N12151574WA006950069300504
B1958183728462N12151575WA006940069300504
B1958193728460N12151576WA006930069200504
B1958203728459N12151576WA006930069200504
B1958213728457N12151577WA006920069100504
B1958223728455N12151578WA006920069100504
B1958233728454N12151580WA006920069100504
B1958243728452N12151580WA006920069100504
B1958253728450N12151580WA006920069000504
B1958263728448N12151579WA006920069000504
B1958273728446N12151579WA006910069000504
GNotImplemented
```

## Record Types:
This library is currently pretty minimal.  Currently, these record types are supported:

### Supported:
    A - FR manufacturer and identification
    B - Fix
    G - Security
    H - File header
    I - List of extension data included at end of each fix B record
    L - Logbook/comments

### Unsupported:
    C - Task/declaration
    D - Differential GPS
    E - Event
    F - Constellation
    J - List of data included in each extension (K) Record
    K - Extension data

