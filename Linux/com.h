/**
 * Bootloader um dem Mikrocontroller Bootloader von Peter Dannegger anzusteuern
 *
 * License: GPL
 *
 * @author Andreas Butti
 */

#ifndef COM_H_INCLUDED
#define COM_H_INCLUDED

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

extern unsigned int crc;

/// Prototypes

/**
 * Set flag for one-wire local echo
 *
 */
void com_localecho ();

/**
 * Opens com port
 *
 * @return descriptor
 */
int com_open(const char * device, speed_t baud);

/**
 * Close com port and restore settings
 */
void com_close(int fd);

/**
 * Sends one char
 */
void com_putc_fast(int fd, unsigned char c);
void com_putc(int fd, unsigned char c);

/**
 * Receives one char or -1 if timeout
 */
int com_getc(int fd, int timeout);

/**
 * Read input string
 */
int com_read (int       fd,
              char      *pszIn,
              size_t    tLen);

/**
 * Sending a command
 */
void sendcommand(int fd, unsigned char c);

#endif //COM_H_INCLUDED
