/**
 * Bootloader um dem Mikrocontroller Bootloader von Peter Dannegger anzusteuern
 * Teile des Codes sind vom original Booloader von Peter Dannegger (danni@alice-dsl.net)
 *
 * @author Andreas Butti
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
        printf("Memory allocation error!\n");
        return NULL;
    }

    memset (data, 0xff, flashsize);

    if(NULL == (fp = fopen(filename, "r")))
    {
        printf("File \"%s\" open failed: %s!\n\n", filename, strerror(errno));
        free(data);
        exit(0);
    }

    printf("Reading %s... ", filename);


    // reading file to "data"
    while((len = readhex(fp, &addr, line)) >= 0)
    {
        if(len)
        {
            if( addr + len > flashsize )
            {
                fclose(fp);
                free(data);
                printf("Hex-file to large for target!\n");
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
    printf("Verifying program memory...\n");

    if(com_getc(TIMEOUT) == BADCOMMAND)
    {
        printf("Verify not available\n");
        return 0;
    }
    printf( "Verify: 00000 - %05lX\n", lastaddr);

    // Sending data to MC
    addr = 0;
    i = lastaddr - addr;

    while (i > 0)
    {
        d1 = data[addr];

        if ((d1 == ESCAPE) || (d1 == 0x13))
        {
            com_putc(ESCAPE);
            d1 += ESC_SHIFT;
        }
        if (i % bInfo->txBlockSize)
        {
            com_putc_fast (d1);
        }
        else
        {
            printPercentage ("Verifying", lastaddr, addr);
            com_putc (d1);
        }
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
    printf("Writing program memory...\n");
    printf("Programming: 00000 - %05lX\n", lastaddr);
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
            d1 = data[addr];

            if ((d1 == ESCAPE) || (d1 == 0x13))
            {
                com_putc(ESCAPE);
                d1 += ESC_SHIFT;
            }
            if (i % bInfo->txBlockSize)
            {
                com_putc_fast (d1);
            }
            else
            {
                printPercentage ("Writing", lastaddr, addr);
                com_putc (d1);
            }
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
    printf("-p Programm\n\n");
    printf("Author: Andreas Butti (andreasbutti@bluewin.ch)\n");

    exit(1);
}

/**
 * Try to connect a device
 */
void connect_device()
{
    const char * ANIM_CHARS = "-\\|/";
    const char PASSWORD[6] = {'P', 'e', 'd', 'a', 0xff, 0};

    int i;
    int localecho = 0;
    int state = 0;
    int in = 0;
    int oldin = 0;

    printf("Waiting for device... Press CTRL+C to exit.  ");
#if 1
    for (;;)
    {
        printf("\b%c", ANIM_CHARS[state]);
        state++;
        state = state % 4;
        fflush(stdout);

        char *s = PASSWORD;

        do 
        {
            com_putc(*s);

            in = com_getc(0);
            if (in == PASSWORD[1])
                localecho = 1;

            if (in == CONNECT)
            {
                printf ("\n");
#if 1
                if (localecho)
                {
                    printf ("One wire!\n");
                    com_localecho();
                }
#endif
                sendcommand( COMMAND );
                for(;;)
                {
                    switch(com_getc(TIMEOUT))
                    {
                        case SUCCESS:
                            printf (" Success\n");
                        case -1:
                            return 0;
                    }
                }

            }

        } while (*s++);
    }
#else
    while(1)
    {
////////////check for echo ....
#if 1
        i = 0;
        while (PASSWORD[i] != '\0')
        {
            com_putc (PASSWORD[i]);
            in = com_getc(0);
            if (in == PASSWORD[0])
                com_localecho();
//            printf ("- send: '%c' %02x rec: '%c' %02x\n", 
//                    PASSWORD[i], PASSWORD[i] & 0x00ff, in & 0x00ff, in & 0x00ff);
            i++;
//        com_puts(PASSWORD);
//        usleep(10000);//wait 10ms
//        in = com_getc(1);
#else
        printf("\b%c", ANIM_CHARS[state]);
        fflush(stdout);

        com_puts(PASSWORD);

        in = com_getc(1);
#endif
        if(in == CONNECT)
        {
            printf("\n...Connect...!\n");
            while (com_getc(1) >= 0);

        usleep(10000);//wait 10ms
            printf("\n...Send Command!\n");
            sendcommand(COMMAND);
//            com_putc(COMMAND);
//            com_getc(1);

            // Empty buffer
            while(1)
            {
                switch(com_getc(TIMEOUT))
                {
                    case SUCCESS:
                        printf("\n...Connected!\n");
                        return;
                    case -1:
                        printf("\n...Connection timeout!\n\n");
                        exit (0);
                }
            }
        }
        }
        state++;
        state = state % 4;
    }
#endif
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
        printf("Bootloader Version unknonwn (Fail)\n");
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
        sscanf ("(?)" , "%s", s);
        printf("File \"devices.txt\" not found!\n");
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
        printf("Device an flashsize do not match!\n");
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

    // last address in hexfile
    unsigned long lastAddr = 0;

    // Serial device
    const char * device = "/dev/ttyS0";

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
    for(i = 0; i < BAUD_CNT; i++)
    {
        if (baud_value[i] == baud)
        {
            baudid = i;
            break;
        }
    }

    if(baudid == -1)
    {
        printf("Unknown baudrate (%i)!\n", baud);
        usage();
    }

    printf("Device  : %s\n", device);
    printf("Baudrate: %i\n", baud);
    if (program)
        printf ("Program ");
    if (verify)
        printf ("Verify");

    printf("\nFile    : %s\n", hexfile);
    printf("-------------------------------------------------\n");

    if(!com_open(device, baud_const[baudid]))
    {
        printf("Opening com port \"%s\" failed (%s)!\n", 
               device, strerror (errno));
        exit(2);
    }

    // read file first
    data = readHexfile (hexfile, bootInfo.flashsize, &lastAddr);

    // now start with target...
    connect_device();
    read_info(&bootInfo);

    if (program)
    {
        if (programFlash (data, lastAddr, &bootInfo))
        {
            if ((bootInfo.crc_on != 2) && (check_crc() != 0))
                printf("\n ---------- Programming failed (wrong CRC)! ----------\n\n");
            else
                printf("\n ++++++++++ Programming successful! ++++++++++\n\n");
        }
        else
            printf("\n ---------- Programming failed! ----------\n\n");
    }
    if (verify)
    {
        if (verifyFlash (data, lastAddr, &bootInfo))
        {
            if ((bootInfo.crc_on != 2) && (check_crc() != 0))
                printf("\n ---------- Verifying failed (wrong CRC)! ----------\n\n");
            else
                printf("\n ++++++++++ Verifying successful! ++++++++++\n\n");
        }
        else
            printf("\n ---------- Verifying failed! ----------\n\n");
    }

    printf("...starting application\n\n");
    sendcommand(START);         //start application
    sendcommand(START);

    com_close();                //close open com port
    return 0;
}

/* end of file */


