#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#include <asynOctetSyncIO.h>

#include "ADDriver.h"

#include "medipixDetector.h"
#include "mpxConnection.h"

// #######################################################################################
// ##################### Header Parsing Functions          ###############################
// #######################################################################################

// Constructor
mpxConnection::mpxConnection(asynUser* parentUser, asynUser* tcpUser,
        medipixDetector* parentObj)
{
    this->parentUser = parentUser;
    this->tcpUser = tcpUser;
    this->parentObj = parentObj;
}

// Data Frame Header Parser for frames from Merlin Quad
// (This data format intended to extend to future products)
// parses the data header and adds appropriate attributes to pImage
void mpxConnection::parseMqDataFrame(NDArray* pImage, const char* header,
        medipixDataHeader headerType)
{
    // TODO
}

// parses the start of the data header and returns its type
medipixDataHeader mpxConnection::parseDataHeader(const char* header)
{
    char buff[MPX_MSG_DATATYPE_LEN];
    medipixDataHeader headerType = MPXUnknownHeader;

    strncpy(buff, header, MPX_MSG_DATATYPE_LEN);

    if (!strncmp(buff, MPX_DATA_12, MPX_MSG_DATATYPE_LEN))
        headerType = MPXDataHeader12;
    else if (!strncmp(buff, MPX_DATA_24, MPX_MSG_DATATYPE_LEN))
        headerType = MPXDataHeader24;
    if (!strncmp(buff, MPX_GENERIC_IMAGE, MPX_MSG_DATATYPE_LEN))
        headerType = MPXGenericImageHeader;
    if (!strncmp(buff, MPX_PROFILE_12, MPX_MSG_DATATYPE_LEN))
        headerType = MPXProfileHeader12;
    else if (!strncmp(buff, MPX_PROFILE_24, MPX_MSG_DATATYPE_LEN))
        headerType = MPXProfileHeader24;
    else if (!strncmp(buff, MPX_GENERIC_PROFILE, MPX_MSG_DATATYPE_LEN))
        headerType = MPXGenericProfileHeader;
    else if (!strncmp(buff, MPX_DATA_ACQ_HDR, MPX_MSG_DATATYPE_LEN))
        headerType = MPXAcquisitionHeader;

    printf("header type is %s\n", buff);

    return headerType;
}

// Data Frame Header Parser for original Frames of type 12B and 24B
// Also parses generic Frames of type IMG (originally developed for UoM XBPM
//    on B21)
// parses the data header and adds appropriate attributes to pImage
void mpxConnection::parseDataFrame(NDAttributeList* pAttr, const char* header,
        medipixDataHeader headerType, size_t *xsize, size_t *ysize,
        int* pixelSize, int* profileMask)
{
    char buff[MPX_IMG_HDR_LEN + 1];
    unsigned long lVal;
    double dVal;
    int iVal, dacNum;
    char dacName[10];
    char* tok;

    // initialise member variables that should be set during this parse
    profileMask = 0;

    // make a copy since strtok is destructive
    strncpy(buff, header, MPX_IMG_HDR_LEN);
    buff[MPX_IMG_HDR_LEN + 1] = 0;

    asynPrint(this->parentUser, ASYN_TRACE_MPX, "Image frame Header: %s\n\n",
            buff);

    tok = strtok(buff, ",");
    tok = strtok(NULL, ",");  // skip the (HDR already parsed)
    if (tok != NULL)
    {
        iVal = atoi(tok);
        pAttr->add("Frame Number", "", NDAttrInt32, &iVal);
    }
    tok = strtok(NULL, ",");
    if (tok != NULL)
    {
        iVal = atoi(tok);
        pAttr->add("Counter Number", "", NDAttrInt32, &iVal);
    }
    tok = strtok(NULL, ",");
    if (tok != NULL)
    {
        time_t rawtime;
        unsigned long msecs;

        // Covert string representation to EPICS Time and store in attributes as
        // a unsigned long
        // format is 2012-02-01 11:26:00.000

        /*
         * NOTE it has been decided that this driver will provide a timestamp and will ignore the value
         * passed from medipix - this is because the FPGA does not have access to a clock while processing
         * and hence all frames in a given acquisition are reported as starting at the same microsecond
         **/
        rawtime = time(NULL);
        lVal = (unsigned long) rawtime;
        msecs = 0;

        pAttr->add("Start Time UTC seconds", "", NDAttrUInt32, &lVal);
        pAttr->add("Start Time millisecs", "", NDAttrUInt32, &msecs);
    }
    tok = strtok(NULL, ",");
    if (tok != NULL)
    {
        dVal = atof(tok);
        pAttr->add("Duration", "", NDAttrFloat64, &dVal);
    }
    if (headerType == MPXGenericImageHeader
            || headerType == MPXGenericProfileHeader)
    {
        tok = strtok(NULL, ",");
        if (tok != NULL)
        {
            iVal = atoi(tok);
            pAttr->add("X Offset", "", NDAttrInt32, &iVal);
        }
        tok = strtok(NULL, ",");
        if (tok != NULL)
        {
            iVal = atoi(tok);
            pAttr->add("Y Offset", "", NDAttrInt32, &iVal);
        }
        tok = strtok(NULL, ",");
        if (tok != NULL)
        {
            iVal = atoi(tok);
            *xsize = iVal;
            pAttr->add("X Size", "", NDAttrInt32, &iVal);
        }
        tok = strtok(NULL, ",");
        if (tok != NULL)
        {
            iVal = atoi(tok);
            *ysize = iVal;
            pAttr->add("Y Size", "", NDAttrInt32, &iVal);
        }
        tok = strtok(NULL, ",");
        if (tok != NULL)
        {
            iVal = atoi(tok);
            pAttr->add("Pixel Depth", "", NDAttrInt32, &iVal);
        }
        tok = strtok(NULL, ",");
        if (tok != NULL)
        {
            iVal = atoi(tok);
            *pixelSize = iVal;
            pAttr->add("Pixel Size", "", NDAttrInt32, &iVal);
        }
    }
    tok = strtok(NULL, ",");
    if (tok != NULL)
    {
        dVal = atof(tok);
        pAttr->add("Threshold 0", "", NDAttrFloat64, &dVal);
    }
    tok = strtok(NULL, ",");
    if (tok != NULL)
    {
        dVal = atof(tok);
        pAttr->add("Threshold 1", "", NDAttrFloat64, &dVal);
    }
    for (dacNum = 1; dacNum <= 25; dacNum++ && tok != NULL)
    {
        tok = strtok(NULL, ",");
        if (tok != NULL)
        {
            iVal = atoi(tok);
            sprintf(dacName, "DAC %03d", dacNum);
            printf("dac %d = %d, ", dacNum, iVal);
            pAttr->add(dacName, "", NDAttrInt32, &iVal);
        }
    }
    tok = strtok(NULL, ",");
    if (tok != NULL)
    {
        iVal = atoi(tok);
        *profileMask = iVal;
        pAttr->add("Profile Mask", "", NDAttrInt32, &iVal);
    }
}

// #######################################################################################
// ##################### Labview communications primitives ###############################
// #######################################################################################

asynStatus mpxConnection::mpxSet(char* valueId, char* value, double timeout)
{
    int buff_len = 0;
    int msg_len = 0;
    asynStatus status;
    char *tok = NULL;

    // default to this error for any following parsing issues
    fromLabviewError = MPX_ERR_UNEXPECTED;

    if ((valueId == NULL))
        return asynError;

    // Build up command to be sent.
    // length is header + length specifier (10 decimal digits) + "SET" + length of variable name + 3 commas
    buff_len = strlen(MPX_HEADER) + MPX_MSG_LEN_DIGITS + strlen(MPX_SET)
            + strlen(value) + strlen(valueId) + 4;
    if (buff_len > MPX_MAXLINE)
        return asynError;

    // the message length specifier contains the count of characters including the ',' after itself
    // i.e. total length minus the header length (including 1 comma)
    msg_len = buff_len - MPX_MSG_LEN_DIGITS - strlen(MPX_HEADER) - 1;

    sprintf(toLabview, "%s,%010u,%s,%s,%s", MPX_HEADER, msg_len, MPX_SET,
            valueId, value);

    if ((status = mpxWriteRead(MPX_SET, valueId, timeout)) != asynSuccess)
    {
        return status;
    }

    // items in the response are comma delimited
    tok = strtok(fromLabviewBody, ",");
    tok = strtok(NULL, ",");

    // 3rd Item is Error Number
    tok = strtok(NULL, ",");
    if (tok == NULL)
        return asynError;
    fromLabviewError = atoi(tok);

    if (fromLabviewError != MPX_OK)
        return asynError;

    return asynSuccess;
}

asynStatus mpxConnection::mpxCommand(char* commandId, double timeout)
{
    int buff_len = 0;
    int msg_len = 0;
    asynStatus status;
    char *tok = NULL;

    // default to this error for any following parsing issues
    fromLabviewError = MPX_ERR_UNEXPECTED;

    if ((commandId == NULL))
        return asynError;

    // Build up command to be sent.
    // length is header + length specifier (10 decimal digits) + "SET" + length of variable name + 3 commas
    buff_len = strlen(MPX_HEADER) + MPX_MSG_LEN_DIGITS + strlen(MPX_CMD)
            + strlen(commandId) + 3;
    if (buff_len > MPX_MAXLINE)
        return asynError;

    // the message length specifier contains the count of characters including the ',' after itself
    // i.e. total length minus the header length (including 1 comma)
    msg_len = buff_len - MPX_MSG_LEN_DIGITS - strlen(MPX_HEADER) - 1;

    sprintf(toLabview, "%s,%010u,%s,%s", MPX_HEADER, msg_len, MPX_CMD,
            commandId);

    if ((status = mpxWriteRead(MPX_CMD, commandId, timeout)) != asynSuccess)
    {
        return status;
    }

    // items in the response are comma delimited
    tok = strtok(fromLabviewBody, ",");
    tok = strtok(NULL, ",");

    // 3rd Item is Error Number
    tok = strtok(NULL, ",");
    if (tok == NULL)
        return asynError;
    fromLabviewError = atoi(tok);

    if (fromLabviewError != MPX_OK)
        return asynError;
    return asynSuccess;
}

/**
 * Get the specified named value from Labview
 */
asynStatus mpxConnection::mpxGet(char* valueId, double timeout)
{
    int buff_len = 0;
    int msg_len = 0;
    asynStatus status;
    char *tok = NULL;

    // default to this error for any following parsing issues
    fromLabviewError = MPX_ERR_UNEXPECTED;

    if ((valueId == NULL))
    {
        return asynError;
    }

    // Build up command to be sent.
    // length is header + length specifier (10 decimal digits) + "GET" + length of variable name + 3 commas
    buff_len = strlen(MPX_HEADER) + MPX_MSG_LEN_DIGITS + strlen(MPX_GET)
            + strlen(valueId) + 3;
    if (buff_len > MPX_MAXLINE)
    {
        return asynError;
    }
    // the message length specifier contains the count of characters including the ',' after itself
    // i.e. total length minus the header length (including 1 comma)
    msg_len = buff_len - MPX_MSG_LEN_DIGITS - strlen(MPX_HEADER) - 1;

    sprintf(toLabview, "%s,%010u,%s,%s", MPX_HEADER, msg_len, MPX_GET, valueId);

    if ((status = mpxWriteRead(MPX_GET, valueId, timeout)) != asynSuccess)
    {
        return status;
    }

    // items in the response are comma delimited
    tok = strtok(fromLabviewBody, ",");
    tok = strtok(NULL, ",");

    // 3rd Item is Value
    tok = strtok(NULL, ",");
    if (tok == NULL)
        return asynError;

    strncpy(fromLabviewValue, tok, MPX_MAXLINE);

    // 4th Item is Error Number
    tok = strtok(NULL, ",");
    if (tok == NULL)
        return asynError;
    fromLabviewError = atoi(tok);

    if (fromLabviewError != MPX_OK)
        return asynError;

    return asynSuccess;
}

/**
 * Send a request to labview
 */
asynStatus mpxConnection::mpxWrite(double timeout)
{
    char *functionName = "mpxWrite";
    size_t nwrite;
    asynStatus status;

    asynPrint(this->parentUser, ASYN_TRACE_MPX, "mpxWrite: Request: %s\n",
            toLabview);

    // pasynOctetSyncIO->flush(this->tcpUser);
    status = pasynOctetSyncIO->write(this->tcpUser, this->toLabview,
            strlen(this->toLabview), timeout, &nwrite);
    // make sure buffers are written out for short messages
    // pasynOctetSyncIO->flush(this->tcpUser);

    if (status)
        asynPrint(this->tcpUser, ASYN_TRACE_ERROR,
                "%s:%s, status=%d, sent\n%s\n", driverName, functionName,
                status, this->toLabview);

    parentObj->toLabViewStr(this->toLabview);

    return asynSuccess;
}

/**
 * Reads in a raw MPX frame from a pasynOctetSyncIO handle
 *
 * This function skips any leading data, looking for the pattern
 * MPX,0000000000,
 *
 * Where
 *      0000000000 = the no. of bytes in body of the
 *          frame in decimal (inclusive of comma after 000000000)
 * Reads the rest of the body into the passed bodyBuf
 *
 */
asynStatus mpxConnection::mpxRead(asynUser* pasynUser, char* bodyBuf,
        int bufSize, int* bytesRead, double timeout)
{
    size_t nread = 0;
    asynStatus status = asynSuccess;
    int eomReason;
    const char *functionName = "mpxRead";
    int headerSize = strlen(MPX_HEADER) + MPX_MSG_LEN_DIGITS + 2;
    int mpxLen = strlen(MPX_HEADER);
    int bodySize;
    int readCount = 0;
    int leadingJunk = 0;
    int headerChar = 0;

    char headerStr[] = MPX_HEADER;
    char* tok;
    char header[MPX_MAXLINE];

    // default to this error for any following parsing issues
    fromLabviewError = MPX_ERR_UNEXPECTED;
    // clear previous contents of buffer in case of error
    bodyBuf[0] = 0;
    *bytesRead = 0;

    // look for MPX in the stream, throw away any preceding data
    // this is to re-synch with server after an error or reboot
    while (headerChar < mpxLen)
    {
        status = pasynOctetSyncIO->read(pasynUser, header + headerChar, 1,
                timeout, &nread, &eomReason);
        if (status != asynSuccess)
            return status;

        if (header[headerChar] == headerStr[headerChar])
        {
            headerChar++;
        }
        else
        {
            leadingJunk++;
            headerChar = 0;
        }
    }

    if (leadingJunk > 0)
    {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s:%s, status=%d %d bytes of leading garbage discarded before header %s \n",
                driverName, functionName, status, leadingJunk,
                this->fromLabview);
    }

    // read the rest of the header block including message length
    readCount = 0;
    do
    {
        status = pasynOctetSyncIO->read(pasynUser, header + mpxLen + readCount,
                (headerSize - mpxLen) - readCount, timeout, &nread, &eomReason);
        if (status == asynSuccess)
            readCount += nread;
    } while (nread != 0 && readCount < (headerSize - mpxLen)
            && status == asynSuccess);

    if (status != asynSuccess)
    {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s:%s, timeout=%f, status=%d received %d bytes\n%s\n",
                driverName, functionName, timeout, status, readCount,
                this->fromLabview);
    }
    else
    {
        if (readCount != (headerSize - mpxLen))
            return asynError;

        // terminate the response for string handling
        header[readCount + mpxLen] = (char) NULL;
        strncpy(fromLabviewHeader, header, MPX_MAXLINE);

        asynPrint(this->parentUser, ASYN_TRACE_MPX,
                "mpxRead: Response Header: %s\n", header);

        // parse the header
        tok = strtok(header, ","); // this first element already verified above

        tok = strtok(NULL, ",");
        if (tok == NULL)
            return asynError;

        // subtract one from bodySize since we already read the 1st comma
        bodySize = atoi(tok) - 1;

        if (bodySize == 0 || bodySize >= bufSize)
            return asynError;

        // now read the rest of the message (the body)
        readCount = 0;
        do
        {
            status = pasynOctetSyncIO->read(pasynUser, bodyBuf + readCount,
                    bodySize - readCount, timeout, &nread, &eomReason);
            if (status == asynSuccess)
                readCount += nread;
        } while (nread != 0 && readCount < bodySize && status == asynSuccess);

        if (readCount < bodySize)
        {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s:%s, timeout=%f, status=%d received %d bytes in MPX command body, expected %d\n",
                    driverName, functionName, timeout, status, readCount,
                    bodySize);
            fromLabviewError = MPX_ERR_LEN;
            return status = asynSuccess ? asynError : status;
        }

        *bytesRead = readCount;
    }

    fromLabviewError = MPX_OK;
    return status;
}

/**
 * Reads in the MPX command header and body from labview
 * verifies the header and places the body in this->fromLabviewBody
 * for parsing by the caller
 */
asynStatus mpxConnection::mpxReadCmd(char* cmdType, char* cmdName,
        double timeout)
{
    static const char *functionName = "mpxReadCmd";
    int nread = 0;
    asynStatus status = asynSuccess;
    char buff[MPX_MAXLINE];
    char* tok;

    // default to this error for any following parsing issues
    fromLabviewError = MPX_ERR_UNEXPECTED;
    // clear out previous values for the member variables for response text
    fromLabview[0] = 0;
    fromLabviewBody[0] = 0;
    fromLabviewHeader[0] = 0;
    fromLabviewValue[0] = 0;

    // repeat read of responses until correct response is received or no more
    // data is available - this approach re-syncs client and server after an error or reboot
    // at either end
    while (status == asynSuccess)
    {
        status = mpxRead(this->tcpUser, buff, MPX_MAXLINE, &nread, timeout);

        if (status == asynSuccess)
        {
            // terminate the response for string handling
            buff[nread] = (char) NULL;
            // update the member variables with relevant parts of the response
            strncpy(fromLabviewBody, buff, MPX_MAXLINE);
            strncpy(fromLabview, fromLabviewHeader, MPX_MAXLINE);
            strncat(fromLabview, fromLabviewBody, MPX_MAXLINE);

            parentObj->fromLabViewStr(this->fromLabview);

            // items in the response are comma delimited -
            // 1st item is the command type
            tok = strtok(buff, ",");
            if (!(tok == NULL || strncmp(cmdType, tok, MPX_MAXLINE)))
            {
                // 2nd item is command (or variable) name which should be echoed back
                tok = strtok(NULL, ",");
                if (!(tok == NULL || strncmp(cmdName, tok, MPX_MAXLINE)))
                    break; // success - exit the while loop
            }
        }

        // if we get here then the expected response was not received
        // report an error and retry

        asynPrint(this->tcpUser, ASYN_TRACE_ERROR,
                "%s:%s error, status=%d unexpected response from labview: '%s%s'\n",
                driverName, functionName, status, fromLabviewHeader,
                fromLabviewBody);

        fromLabviewHeader[0] = 0; // clear for consistent error reporting
        fromLabviewBody[0] = 0;
        fromLabview[0] = 0;
    }

    asynPrint(this->parentUser, ASYN_TRACE_MPX,
            "mpxReadCmd: Full Response: %s\n", fromLabview);

    fromLabviewError = MPX_OK;
    return status;
}

asynStatus mpxConnection::mpxWriteRead(char* cmdType, char* cmdName,
        double timeout)
{
    asynStatus status;

    // this->lock(); // make sure commands from different threads are not interleaved
    // removed above because I do not believe you can nest locks and the following unlock
    // would therefore free the AsynPort Thread when called from WriteInt32 for example

    if ((status = mpxWrite(timeout)) != asynSuccess)
    {
        return status;
    }

    if ((status = mpxReadCmd(cmdType, cmdName, timeout)) != asynSuccess)
    {
        return status;
    }

    // removed - see above comment
    // this->unlock();

    return asynSuccess;
}

// #######################################################################################
// ##################### Helper functions                         ########################
// #######################################################################################

/** helper functions for endian conversion
 *
 */
inline void endian_swap(unsigned short& x)
{
    x = (x >> 8) | (x << 8);
}

inline void endian_swap(unsigned int& x)
{
    x = (x >> 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00)
            | (x << 24);
}

inline void endian_swap(uint64_t& x)
{
    x = ((((x) & 0x00000000000000FFLL) << 0x38)
            | (((x) & 0x000000000000FF00LL) << 0x28)
            | (((x) & 0x0000000000FF0000LL) << 0x18)
            | (((x) & 0x00000000FF000000LL) << 0x08)
            | (((x) & 0x000000FF00000000LL) >> 0x08)
            | (((x) & 0x0000FF0000000000LL) >> 0x18)
            | (((x) & 0x00FF000000000000LL) >> 0x28)
            | (((x) & 0xFF00000000000000LL) >> 0x38));
}

/** memory dump of data for diagnostics
 */
void mpxConnection::dumpData(char* sdata, int size)
{
    int w = 20;  // width
    int b = 10;  // break at points in row
    unsigned char* data = (unsigned char*) sdata;

    printf("\nData String - %s\n", data);
    for (int i = 0; i < size; i += w)
    {
        printf("%08d", i);
        for (int c = 0; c < w; c++)
        {
            if (c % b == 0)
            {
                printf("  ");
            }
            if (c + i > size)
            {
                printf(" ");
            }
            else
            {
                char t = data[i + c];
                if (t < 0x20 || t > 0x7e)
                {
                    t = '.';
                }
                printf("%c", t);
            }
        }
        for (int c = 0; c < w; c++)
        {
            if (c % b == 0)
            {
                printf("  ");
            }
            if (c + i <= size)
            {
                printf("%02x ", data[i + c]);
            }
        }
        printf("\n");
    }
    printf("\n\n");
}

