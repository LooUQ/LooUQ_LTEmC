/*---------------------------------------------------------------------------/
/  LTEm1 Functional Configurations
/---------------------------------------------------------------------------*/

// Defaults defined here, copy this header to your project to change

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define LTEM1_SOCKETS	1
/* This option switches TCP/UDP configuration. 
 * 0: No APN/PDP context support
 * 1: TCP/UDP/SSL PDP contexts available
 */

#define LTEM1_MQTT	    1
/* This option switches support for MQTT protocol support. 
 * 0: No MQTT protocol support
 * 1: MQTT protocol available
 */

#define LTEM1_HTTP	    0
/* This option switches support for HTTP protocol support. 
 * 0: No HTTP protocol support
 * 1: HTTP protocol available
 */


#define LTEM1_FTP	    0
/* This option switches support for FTP protocol support. 
 * 0: No FTP protocol support
 * 1: FTP protocol available
 */


#define LTEM1_GNSS      0
/* This option switches support for GNSS location services. 
 * 0: No GNSS services
 * 1: GNSS services available
 */


#if LTEM1_GNSS == 1
#define LTEM1_GEOFENCE  0
/* This option switches support for Geofencing functionality.
 * 0: No Geofence functionality
 * 1: Geofence functionality is available
 * 
 * NOTE: GNSS is required for geo-fencing support */
#endif


#define LTEM1_FILES     0
/* This option switches support for file storage (file system) on LTEm1 device. 
 * 0: No storage on LTEm1 available
 * 1: Filesystem support on LTEm1 device available
 */


#define LTEM1_FOTA      0
/* This option switches support for FOTA (LTEm1 device Firmware-over-the-Air update) services. 
 * 0: No FOTA support
 * 1: FOTA supported
 */

