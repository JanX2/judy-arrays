/*
 *  binary_macros.h
 *
 *  Binary constant generator macro for endianess convertion
 *  by Tom Torfs - donated to the public domain 
 *  
 */

/** turn a numeric literal into a hex constant
 *  (avoids problems with leading zeroes)
 *  8-bit constants max value 0x11111111, always fits in uint32_t
 */
#define HEX__(n) 0x##n##LU

/**
 * 8-bit conversion function 
 */
#define B8__(x) ((x&0x0000000FLU)?1:0)	\
+((x&0x000000F0LU)?2:0)	 \
+((x&0x00000F00LU)?4:0)	 \
+((x&0x0000F000LU)?8:0)	 \
+((x&0x000F0000LU)?16:0) \
+((x&0x00F00000LU)?32:0) \
+((x&0x0F000000LU)?64:0) \
+((x&0xF0000000LU)?128:0)

/** for upto 8-bit binary constants */
#define B8(d) ((uint8_t)B8__(HEX__(d)))

/** for upto 16-bit binary constants, MSB first */
#define B16(dmsb,dlsb) (((uint16_t)B8(dmsb)<<8) + B8(dlsb))

/** for upto 32-bit binary constants, MSB first */
#define B32(dmsb,db2,db3,dlsb) (((uint32_t)B8(dmsb)<<24) \
+ ((uint32_t)B8(db2)<<16) \
+ ((uint32_t)B8(db3)<<8)  \
+ B8(dlsb))

/** turn a numeric literal into a hex constant
 *(avoids problems with leading zeroes)
 * 8-bit constants max value 0x11111111, always fits in uint32_t
 */
#define HEX__(n) 0x##n##LU

/** 8-bit conversion function */
#define B8__(x) ((x&0x0000000FLU)?1:0)	\
+((x&0x000000F0LU)?2:0)  \
+((x&0x00000F00LU)?4:0)  \
+((x&0x0000F000LU)?8:0)  \
+((x&0x000F0000LU)?16:0) \
+((x&0x00F00000LU)?32:0) \
+((x&0x0F000000LU)?64:0) \
+((x&0xF0000000LU)?128:0)


/** for upto 8-bit binary constants */
#define B8(d) ((uint8_t)B8__(HEX__(d)))

/** for upto 16-bit binary constants, MSB first */
#define B16(dmsb,dlsb) (((uint16_t)B8(dmsb)<<8) + B8(dlsb))

/** for upto 32-bit binary constants, MSB first */
#define B32(dmsb,db2,db3,dlsb) (((uint32_t)B8(dmsb)<<24)	 \
+ ((uint32_t)B8(db2)<<16) \
+ ((uint32_t)B8(db3)<<8)  \
+ B8(dlsb))

/* Example usage:
 * B8(01010101) = 85
 * B16(10101010,01010101) = 43605
 * B32(10000000,11111111,10101010,01010101) = 2164238933
 */
