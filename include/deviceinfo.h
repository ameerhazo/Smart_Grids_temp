#ifndef DEVICEINFO_HEADER
#define DEVICEINFO_HEADER

#define MSIZE 4096       /* maximal number of string                                                */


//device indentifiers (change for every device)
char device_ID[MSIZE] = "SUITE_019";                    // Change for every device

//firmware info
char firmwareVersion[MSIZE] = "2022-02-09";
char firmwareVersion_IKT[MSIZE] = "13062022";

//general device (ehz) information
char ServerID_string[MSIZE];
char ServerID_string_formatted[4096];
char eHZ_Message[MSIZE] = "";                           // buffer for input data
char manufacturer[MSIZE] = "hager";                     // change 

// booleans to send device metadata only once
bool sent_device_metadata_TD = false;
bool sent_meter_number_metadata = false;
bool sent_identification_metadata = false;
bool sent_firmwareVersion_metadata = false;

#endif