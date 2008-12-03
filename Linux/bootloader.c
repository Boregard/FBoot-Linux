/**
 * Bootloader um dem Mikrocontroller Bootloader von Peter Dannegger anzusteuern
 * Teile des Codes sind vom original Booloader von Peter Dannegger (danni@alice-dsl.net)
 *
 * @author Bernhard Michler, based on linux source of Andreas Butti
 */


/// Includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <sys/times.h>
#include <sys/ioctl.h>

#include "com.h"
#include "protocol.h"


typedef struct bootInfo
{
    long    revision;
    long    signature;
    long    buffsize;
    long    flashsize;
    int     crc_on;
    int     txBlockSize;
} bootInfo_t;


/// Definitions
#define TIMEOUT   3  // 0.3s
#define TIMEOUTP  40 // 4s

typedef struct
{
    unsigned long   baudValue;
    speed_t         baudConst;
} baudInfo_t;
baudInfo_t baudRates[] = { 
    {     50,     B50 },
    {     75,     B75 },
    {    110,    B110 },
    {    134,    B134 },
    {    150,    B150 },
    {    200,    B200 },
    {    300,    B300 },
    {    600,    B600 },
    {   1200,   B1200 },
    {   1800,   B1800 },
    {   2400,   B2400 },
    {   4800,   B4800 },
    {   9600,   B9600 },
    {  19200,  B19200 },
    {  38400,  B38400 },
    {  57600,  B57600 },
    { 115200, B115200 },
    { 230400, B230400 }
};


typedef struct
{
    unsigned long   id;
    const char      *name;
} avrdev_t;
avrdev_t avrDevices[] = {
    { 0x01e9007,    "ATtiny13" },
    { 0x01e910a,    "ATtiny2313" },
    { 0x01e9205,    "ATmega48" },
    { 0x01e9206,    "ATtiny45" },
    { 0x01e9207,    "ATtiny44" },
    { 0x01e9208,    "ATtiny461" },
    { 0x01e9306,    "ATmega8515" },
    { 0x01e9307,    "ATmega8" },
    { 0x01e9308,    "ATmega8535" },
    { 0x01e930a,    "ATmega88" },
    { 0x01e930b,    "ATtiny85" },
    { 0x01e930c,    "ATtiny84" },
    { 0x01e930d,    "ATtiny861" },
    { 0x01e930f,    "ATmega88P" },
    { 0x01e9403,    "ATmega16" },
    { 0x01e9404,    "ATmega162" },
    { 0x01e9406,    "ATmega168" },
    { 0x01e9501,    "ATmega323" },
    { 0x01e9502,    "ATmega32" },
    { 0x01e9609,    "ATmega644" },
    { 0x01e9802,    "ATmega2561" }
};
    


/**
 * reads hex data from string
 */
int sscanhex (char          *str,
              unsigned int  *hexout,
              int           n)
{
    unsigned int hex = 0, x = 0;

    for(; n; n--)
    {
        x = *str;
        if(x >= 'a')
        {
            x += 10 - 'a';
        }
        else if(x >= 'A')
        {
            x += 10 - 'A';
        }
        else
        {
            x -= '0';
        }

        if(x >= 16)
        {
            break;
        }

        hex = hex * 16 + x;
        str++;
    }

    *hexout = hex;
    return n; // 0 if all digits
}


/**
 * Reads the hex file
 *
 * @return 1 to 255 number of bytes, -1 file end, -2 error or no HEX File
 */
int readhex (FILE           *fp,
             unsigned long  *addr,
             unsigned char  *data)
{
    char hexline[524]; // intel hex: max 255 byte
    char * hp = hexline;
    unsigned int byte;
    int i;
    unsigned int num;
    unsigned int low_addr;

    if(fgets( hexline, 524, fp ) == NULL)
    {
        return -1; // end of file
    }

    if(*hp++ != ':')
    {
        return -2; // no hex record
    }

    if(sscanhex(hp, &num, 2))
    {
        return -2; // no hex number
    }

    hp += 2;

    if(sscanhex(hp, &low_addr, 4))
    {
        return -2;
    }

    *addr &= 0xF0000L;
    *addr += low_addr;
    hp += 4;

    if(sscanhex( hp, &byte, 2))
    {
        return -2;
    }

    if(byte == 2)
    {
        hp += 2;
        if(sscanhex(hp, &low_addr, 4))
        {
            return -2;
        }
        *addr = low_addr * 16L;
        return 0; // segment record
    }

    if(byte == 1)
    {
        return 0; // end record
    }

    if(byte != 0)
    {
        return -2; // error, unknown record
    }

    for(i = num; i--;)
    {
        hp += 2;
        if(sscanhex(hp, &byte, 2))
        {
            return -2;
        }
        *data++ = byte;
    }
    return num;
}

/**
 * Read a hexfile
 */
char * readHexfile(const char * filename, int flashsize, unsigned long * lastaddr)
{
    char * data;
    FILE *fp;
    int len;
    int x;
    unsigned char line[512];
    unsigned long addr = 0;

    data = malloc(flashsize);
    if (data == NULL)
    {
        printf("Memory allocation error, could not get %d bytes for flash-buffer!\n",
               flashsize);
        return NULL;
    }

    memset (data, 0xff, flashsize);

    if(NULL == (fp = fopen(filename, "r")))
    {
        printf("File \"%s\" open failed: %s!\n\n", filename, strerror(errno));
        free(data);
        exit(0);
    }

    printf("Reading : %s... ", filename);


    // reading file to "data"
    while((len = readhex(fp, &addr, line)) >= 0)
    {
        if(len)
        {
            if( addr + len > flashsize )
            {
                fclose(fp);
                free(data);
                printf("\n  Hex-file to large for target!\n");
                return NULL;
            }
            for(x = 0; x < len; x++)
            {
                data[x + addr] = line[x];
            }

            addr += len;

            if(*lastaddr < addr-1)
            {
                *lastaddr = addr-1;
            }
            addr++;
        }
    }

    fclose(fp);

    printf("File read.\n");
    return data;
}


/**
 * Reads a value from bootloader
 *
 * @return value; -2 on error; exit on timeout
 */
long readval()
{
    int i;
    int j = 257;
    long val = 0;

    while(1)
    {
        i = com_getc(TIMEOUT);
        if(i == -1)
        {
            printf("...Device does not answer!\n");
            exit(0);
        }

        switch(j)
        {
            case 1:
                if(i == SUCCESS)
                {
                    return val;
                }
                break;

            case 2:
            case 3:
            case 4:
                val = val * 256 + i;
                j--;
                break;

            case 256:
                j = i;
                break;

            case 257:
                if(i == FAIL) {
                    return -2;
                }
                else if(i == ANSWER) {
                    j = 256;
                }
                break;

            default:
                printf("\nError: readval, i = %i, j = %i, val = %li\n", i, j, val);
                return -2;
        }
    }
    return -2;
}

/**
 * Print percentage line
 */
void printPercentage (char          *text,
                      unsigned long full_val,
                      unsigned long cur_val)
{
    int         i;
    int         cur_perc;
    int         cur100p;
    int         txtLen = 10;    // length of the add. text 2 * " | " "100%"
    unsigned short columns = 80;
    struct winsize tWinsize;

    if (text)
        txtLen += strlen (text);

    if (ioctl (STDIN_FILENO, TIOCGWINSZ, &tWinsize) >= 0)
    {
        // number of columns in terminal
        columns = tWinsize.ws_col;
    }
    cur100p  = columns - txtLen;
    cur_perc = (cur_val * cur100p) / full_val;

    printf ("%s | ", text ? text : "");

    for (i = 0; i < cur_perc; i++) printf ("#");
    for (     ; i < cur100p;  i++) printf (" ");

    printf (" | %3d%%\r", (int)((cur_val * 100) / full_val));

    fflush(stdout);
}


/**
 * Verify the controller
 */
int verifyFlash (char        * data,
                 unsigned long lastaddr,
                 bootInfo_t  * bInfo)
{
    struct tms  theTimes;
    clock_t start_time;       //time
    clock_t end_time;         //time
    float   seconds;

    int    i;
    unsigned char d1;
    unsigned long addr;

    start_time = times (&theTimes);

    // Sending commands to MC
    sendcommand(VERIFY);

    if(com_getc(TIMEOUT) == BADCOMMAND)
    {
        printf("Verify not available\n");
        return 0;
    }
    printf( "Verify        : 00000 - %05lX\n", lastaddr);

    // Sending data to MC
    addr = 0;
    i = lastaddr - addr;

    while (i > 0)
    {
        if ((i % 16) == 0)
            printPercentage ("Verifying", lastaddr, addr);

        d1 = data[addr];

        if ((d1 == ESCAPE) || (d1 == 0x13))
        {
            com_putc(ESCAPE);
            d1 += ESC_SHIFT;
        }
        if (i % bInfo->txBlockSize)
            com_putc_fast (d1);
        else
            com_putc (d1);

        i--;
        addr++;
    }

    printPercentage ("Verifying", 100, 100);

    end_time = times (&theTimes);
    seconds  = (float)(end_time-start_time)/sysconf(_SC_CLK_TCK);

    printf("\nElapsed time: %3.2f seconds, %.0f Bytes/sec.\n",
           seconds,
           (float)lastaddr / seconds);

    com_putc(ESCAPE);
    com_putc(ESC_SHIFT); // A5,80 = End

    if (com_getc(TIMEOUTP) == SUCCESS)
        return 1;

    return 0;
}


/**
 * Flashes the controller
 */
int programFlash (char        * data,
                  unsigned long lastaddr,
                  bootInfo_t *  bInfo)
{
    struct tms  theTimes;
    clock_t start_time;       //time
    clock_t end_time;         //time
    float   seconds;

    int    i;
    unsigned char d1;
    unsigned long addr;

    start_time = times (&theTimes);

    // Sending commands to MC
    printf("Programming   : 00000 - %05lX\n", lastaddr);
    sendcommand(PROGRAM);

    // Sending data to MC
    addr = 0;
    i = lastaddr - addr;
    if (i > bInfo->buffsize)
        i = bInfo->buffsize;

    while (i > 0)
    {
        printPercentage ("Writing", lastaddr, addr);

        // first write one buffer
        while (i > 0)
        {
            if ((i % 16) == 0)
                printPercentage ("Writing", lastaddr, addr);

            d1 = data[addr];

            if ((d1 == ESCAPE) || (d1 == 0x13))
            {
                com_putc(ESCAPE);
                d1 += ESC_SHIFT;
            }
            if (i % bInfo->txBlockSize)
                com_putc_fast (d1);
            else
                com_putc (d1);

            i--;
            addr++;
        }

        // set nr of bytes with next block
        i = lastaddr - addr;
        if (i > bInfo->buffsize)
            i = bInfo->buffsize;

        if (i > 0)
        {
            // now check if it is correctly burned
            if (com_getc (TIMEOUTP) != CONTINUE)
            {
                printf("\n ---------- Failed! ----------\n");
                free(data);
                return 0;
            }
        }
    }

    printPercentage ("Writing", 100, 100);

    end_time = times (&theTimes);
    seconds  = (float)(end_time-start_time)/sysconf(_SC_CLK_TCK);

    printf("\nElapsed time: %3.2f seconds, %.0f Bytes/sec.\n",
           seconds,
           (float)lastaddr / seconds);

    com_putc(ESCAPE);
    com_putc(ESC_SHIFT); // A5,80 = End

    if (com_getc(TIMEOUTP) == SUCCESS)
        return 1;

    return 0;
}



/**
 * prints usage
 */
void usage()
{
    printf("./booloader [-d /dev/ttyS0] [-b 9600] -[v|p] file.hex\n");
    printf("-d Device\n");
    printf("-b Baudrate\n");
    printf("-t TxD Blocksize\n");
    printf("-v Verify\n");
    printf("-p Programm\n");
    printf("-P Password\n");
    printf("Author: Bernhard Michler (based on code from Andreas Butti)\n");

    exit(1);
}

/**
 * Try to connect a device
 */
void connect_device ( char *password )
{
    const char * ANIM_CHARS = "-\\|/";

    int localecho = 0;
    int state = 0;
    int in = 0;

    printf("Waiting for device...  ");

    while (1)
    {
        usleep (25000);     // just to slow animation...
        printf("\b%c", ANIM_CHARS[state++ & 3]);
        fflush(stdout);

        const char *s = password;

        do 
        {
            if (*s)
                com_putc(*s);
            else
                com_putc(0x0ff);

            in = com_getc(0);
            if (in == password[1])
                localecho = 1;

            if (in == CONNECT)
            {
                if (localecho)
                {
                    while (com_getc(TIMEOUT) != -1);

                    printf ("\bconnected (one wire)!\n");
                    com_localecho();
                }
                else 
                {
                    printf ("\bconnected!\n");
                }

                sendcommand( COMMAND );

                while (1)
                {
                    switch(com_getc(TIMEOUT))
                    {
                        case SUCCESS:
                        case -1:
                            return;
                    }
                }
            }
        } while (*s++);
    }
}//void connect_device()


/**
 * Checking CRC Support
 *
 * @return 2 if no crc support, 0 if crc supported, 1 fail, exit on timeout
 */
int check_crc()
{
    int i;
    unsigned int crc1;

    sendcommand(CHECK_CRC);
    crc1 = crc;
    com_putc(crc1);
    com_putc(crc1 >> 8);

    i = com_getc(TIMEOUT);
    switch (i)
    {
        case SUCCESS:
            return 0;
        case BADCOMMAND:
            return 2;
        case FAIL:
            return 1;
        case -1:
            printf("...Device does not answer!\n\n");
            exit (0);
        default:
            return i;
    }
}

/**
 * prints the device signature
 *
 * @return true on success; exit on error
 */
int read_info (bootInfo_t *bInfo)
{
    long i, j;
    char s[256];
    FILE *fp;

    bInfo->crc_on = check_crc();
    sendcommand(REVISION);

    i = readval();
    if(i == -2)
    {
        printf("Bootloader Version unknown (Fail)\n");
        bInfo->revision = -1;
    }
    else
    {
        printf("Bootloader    : V%lX.%lX\n", i>>8, i&0xFF);
        bInfo->revision = i;
    }

    sendcommand(SIGNATURE);

    i = readval();
    if (i == -2)
    {
        printf("Reading device SIGNATURE failed!\n\n");
        exit (0);
    }
    bInfo->signature = i;

    if((fp = fopen("devices.txt", "r")) != NULL)
    {
        while(fgets(s, 256, fp))
        {
            if(sscanf(s, "%lX : %s", &j, s) == 2)
            { // valid entry
                if(i == j)
                {
                    break;
                }
            }
            *s = 0;
        }
        fclose(fp);
    }
    else
    {
        // search locally...
        for (j = 0; j < (sizeof (avrDevices) / sizeof (avrdev_t)); j++)
        {
            if (i == avrDevices[j].id)
            {
                strcpy (s, avrDevices[j].name);
                break;
            }
        }
        if (j == (sizeof (avrDevices) / sizeof (avrdev_t)))
        {
            sscanf ("(?)" , "%s", s);
            printf("File \"devices.txt\" not found!\n");
        }
    }
    printf("Target        : %06lX %s\n", i, s);

    sendcommand(BUFFSIZE);

    i = readval();
    if (i == -2)
    {
        printf("Reading BUFFSIZE failed!\n\n");
        exit (0);
    }
    bInfo->buffsize = i;

    printf("Buffer        : %ld Byte\n", i );

    sendcommand(USERFLASH);

    i = readval();
    if (i == -2)
    {
        printf("Reading FLASHSIZE failed!\n\n");
        exit (0);
    }
    if( i > MAXFLASH)
    {
        printf("Device and flashsize do not match!\n");
        exit (0);
    }
    bInfo->flashsize = i;

    printf("Size available: %ld Byte\n", i );

    if(bInfo->crc_on != 2)
    {
        bInfo->crc_on = check_crc();
        switch(bInfo->crc_on)
        {
            case 2:
                printf("No CRC support.\n");
                break;
            case 0:
                printf("CRC enabled and OK.\n");
                break;
            case 3:
                printf("CRC check failed!\n");
                break;
            default:
                printf("Checking CRC Error (%i)!\n", bInfo->crc_on);
                break;
        }
    }
    else
    {
        printf("No CRC support.\n\n");
    }

    return 1;
}//int read_info()



/**
 * Main, startup
 */
int main(int argc, char *argv[])
{
    // info buffer
    bootInfo_t bootInfo;

    // Filename of the HEX File
    const char * hexfile = NULL;

    char    verify  = 0;
    char    program = 0;

    // pointer to the loaded and converted HEX-file
    char    *data = NULL;

    // pointer to password...
    char    *password = "Peda";

    // last address in hexfile
    unsigned long lastAddr = 0;

    // Serial device
    const char * device = "/dev/ttyS0";

    // init bootinfo
    memset (&bootInfo, 0, sizeof (bootInfo));

    // set flashsize to 256k; should proalby later be corrected to
    // the real size of the used controller...
    bootInfo.flashsize = 256 * 1024;

    // default values
    int baud = 4800;
    int baudid = -1;
    bootInfo.txBlockSize = 16;

    // print header
    printf("\n");
    printf("=================================================\n");
    printf("|           BOOTLOADER, Target: V2.1            |\n");
    printf("=================================================\n");

    // Parsing / checking parameter
    int i;

    for(i = 1; i < argc; i++)
    {
        if (strcmp (argv[i], "-d") == 0)
        {
            i++;
            if (i < argc)
                device = argv[i];
        }
        else if (strcmp (argv[i], "-b") == 0)
        {
            i++;
            if (i < argc)
                baud = atoi(argv[i]);
        }
        else if (strcmp (argv[i], "-v") == 0)
        {
            verify = 1;
        }
        else if (strcmp (argv[i], "-p") == 0)
        {
            program = 1;
        }
        else if (strcmp (argv[i], "-t") == 0)
        {
            i++;
            if (i < argc)
                bootInfo.txBlockSize = atoi(argv[i]);
        }
        else if (strcmp (argv[i], "-P") == 0)
        {
            i++;
            if (i < argc)
                password = argv[i];
        }
        else
        {
            hexfile = argv[i];
        }
    }

    if (hexfile == NULL)
    {
        printf("No hexfile specified!\n");
        usage();
    }

    if ((verify == 0) && (program == 0))
    {
        printf("No Verify / Programm specified!\n");
        usage();
    }

    // Checking baudrate
    for(i = 0; i < (sizeof (baudRates) / sizeof (baudInfo_t)); i++)
    {
        if (baudRates[i].baudValue == baud)
        {
            baudid = i;
            break;
        }
    }

    if(baudid == -1)
    {
        printf("Unknown baudrate (%i)!\nUse one of:", baud);
        for(i = 0; i < (sizeof (baudRates) / sizeof (baudInfo_t)); i++)
        {
            printf (" %ld", baudRates[i].baudValue);
        }
        printf ("\n");
        usage();
    }

    printf("Port    : %s\n", device);
    printf("Baudrate: %i\n", baud);
    printf("File    : %s\n", hexfile);

    // read file first
    data = readHexfile (hexfile, bootInfo.flashsize, &lastAddr);

    if (data == NULL)
    {
        printf ("ERROR: no buffer allocated and filled, exiting!\n");
        return (-1);
    }
    printf("Size    : %ld Bytes\n", lastAddr);

    if (program & verify)
    {
        printf ("Program and verify device.\n");
    }
    else
    {
        if (program)
            printf ("Program device.\n");
        if (verify)
            printf ("Verify device.\n");
    }
    printf("-------------------------------------------------\n");

    if(!com_open(device, baudRates[baudid].baudConst))
    {
        printf("Opening com port \"%s\" failed (%s)!\n", 
               device, strerror (errno));
        exit(2);
    }

    // now start with target...
    connect_device (password);
    read_info (&bootInfo);

    if (program)
    {
        if (programFlash (data, lastAddr, &bootInfo))
        {
            if ((bootInfo.crc_on != 2) && (check_crc() != 0))
                printf("\n ---------- Programming failed (wrong CRC)! ----------\n\n");
            else
                printf("\n ++++++++++ Device successfully programmed! ++++++++++\n\n");
        }
        else
            printf("\n ---------- Programming failed! ----------\n\n");
    }
    if (verify)
    {
        if (verifyFlash (data, lastAddr, &bootInfo))
        {
            if ((bootInfo.crc_on != 2) && (check_crc() != 0))
                printf("\n ---------- Verification failed (wrong CRC)! ----------\n\n");
            else
                printf("\n ++++++++++ Device successfully verified! ++++++++++\n\n");
        }
        else
            printf("\n ---------- Verification failed! ----------\n\n");
    }

    printf("...starting application\n\n");
    sendcommand(START);         //start application
    sendcommand(START);

    com_close();                //close open com port
    return 0;
}

/* end of file */


