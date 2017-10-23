//*****************************************************************************
//
// opus_enc_dec.c - Opus Encoder and Decoder example with command line inputs.
//
// Copyright (c) 2013-2015 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
//
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
//
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
//
// This is part of revision 2.1.2.111 of the EK-TM4C1294XL Firmware Package.
//
//*****************************************************************************
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <debug.h>
#include "opus.h"
#include "opus_types.h"
#include "opus_private.h"
#include "opus_multistream.h"
#include "opus_internal.h"
#include "artik_opus.h"
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#define FRESULT int

int get_task_load(void)
{
    int fd;
    char proc_iobuffer[16];
    ssize_t nread;
    char *filepath; 
    pid_t pid;
    pid = getpid();

    asprintf(&filepath, "/proc/%d/loadavg", pid);
    
 /* Open the file */
    fd = open(filepath, O_RDONLY);
    if (fd < 0) { 
        printf("Open %s error\n", filepath);
        return ERROR;
    }    

    nread = read(fd, proc_iobuffer, 16-1);
    proc_iobuffer[nread] = '\0';
    printf("%s\n", proc_iobuffer);
    close(fd);
    return nread;
}


int get_cpu_load(void)
{
    int fd;
    char proc_iobuffer[16];
    ssize_t nread;
    /* Open the file */
    fd = open("/proc/cpuload", O_RDONLY);
    if (fd < 0) { 
        printf("Open file error\n");
        return ERROR;
    }    

    nread = read(fd, proc_iobuffer, 16-1);
    proc_iobuffer[nread] = '\0';
    printf("%s\n", proc_iobuffer);
    close(fd);
    return nread;
}

static int read_from_flash(char *dst, int size, int curpos)
{
    unsigned char *start= (unsigned char*)0x04400000;
    unsigned char *pout = (unsigned char *)dst; 
    unsigned char *pin = (unsigned char*)(start + curpos);
    int bytes = size; 

    while (size-- > 0)
        *pout++= *pin++;
    
    return bytes;
}

//****************************************************************************
//
// Opus Encode and Decoder structure variables are declared here.
//
//****************************************************************************

//*****************************************************************************
//
// This function implements the "enc" command. It is provided with 2 parameters
// The first parameter is an input wav file and the second parameter is the
// output opx file compressed with OPUS.
//
//*****************************************************************************
int
Cmd_encode(int argc, char *argv[])
{
    FRESULT  iFRdResult;
    FRESULT  iFWrResult;
    uint8_t  *pui8data;
    uint8_t  ui8ScaleFactor;
    char     cOpxDelimiter[4];
    char     *pcRdBuf;
    uint32_t ui32BytesRead;
    //uint32_t ui32BytesWrite;
    uint32_t ui32Sizeofpopi16fmtBuffer;
    uint32_t ui32Loop;
    uint32_t ui32EncodedLen=0;
    uint32_t ui32RawLen=0;
    uint32_t ui32SizeOfRdBuf;
    int32_t  i32error;
    int32_t  i32len;

    tWaveHeader sWaveHeader;
    opus_int16 *popi16fmtBuffer;
    OpusEncoder *sOpusEnc;
    int fout;
    //char *buf;
    int complexity = 7;
    //char outpath[20];

    int fin;
    printf("in file path %s\n", argv[2]);
    fin = open(argv[2], O_RDWR);

    //
    // If there was some problem opening the file, then return an error.
    //
    if(fin < 0)
    {
        printf("open in file %s failed\n", argv[2]);
        return -1;
    }

    //
    // Get the Header Information
    //
    iFRdResult = read(fin, (void*)&sWaveHeader, sizeof(sWaveHeader));
    printf("Header Readed: %d\n", iFRdResult);

    //
    // Check if the WAV file header is correctly formatted
    //
    if((memcmp((char *)&sWaveHeader.ui8ChunkID[0],"RIFF",4) != 0) ||
       (memcmp((char *)&sWaveHeader.ui8Format[0],"WAVE",4) != 0) ||
       (memcmp((char *)&sWaveHeader.ui8SubChunk1ID[0],"fmt ",4) != 0) ||
       (memcmp((char *)&sWaveHeader.ui8SubChunk2ID[0],"data",4) != 0))
    {
        printf("ENC_ERR: WAV file is invalid\n");
        //fclose(fin);
        return(0);
    }

    printf("Hi\n");

    //
    // Since OPUS supports 8k, 16k, 24k, 32k or 48k sampling rates a check is
    // is made to see if the incoming audio stream also has the same property
    //
    if((sWaveHeader.ui32SampleRate != 8000)  &&
       (sWaveHeader.ui32SampleRate != 16000) &&
       (sWaveHeader.ui32SampleRate != 24000) &&
       (sWaveHeader.ui32SampleRate != 32000) &&
       (sWaveHeader.ui32SampleRate != 48000) )
    {
        printf("ENC_ERR: Sample Rate Must be either 8k, 16k, 24k, 32k or ");
        printf("48k\n");
        printf("Current Sample Rate: %d\n", sWaveHeader.ui32SampleRate);
        //fclose(fin);
        return(0);
    }

    printf("Do\n");

    //
    // Only Linear PCM format is supported
    //
    if(sWaveHeader.ui16AudioFormat != 0x1)
    {
        printf("ENC_ERR: Only Linear PCM Format Supported\n");
        //fclose(fin);
        return(0);
    }

    //
    // Open the file for writing.
    //
    printf("out file path: %s\n", argv[3]);
    fout = open(argv[3], O_RDWR | O_CREAT | O_TRUNC);

    //
    // If there was some problem opening the file, then return an error.
    //
    if(fout < 0)
    {
        close(fin);
        printf("open out file %s failed\n", argv[3]); 
        return -1;
    }

    //
    // Create the encoder
    //
#if 0
    sOpusEnc = opus_encoder_create(sWaveHeader.ui32SampleRate,
            sWaveHeader.ui16NumChannels, OPUS_APPLICATION_VOIP, &i32error);
#endif
    
    sOpusEnc = opus_encoder_create(sWaveHeader.ui32SampleRate,
            sWaveHeader.ui16NumChannels, OPUS_APPLICATION_AUDIO, &i32error);


    printf("sOpusEnc = %p\n", sOpusEnc); 
    //
    // If there is an error creating the encoder then close the input and
    // output file handle. Else print the information on the file parameters
    //
    if (i32error != OPUS_OK)
    {
       printf("ENC_ERR: Cannot create encoder: %s\n",
               opus_strerror(i32error));
       close(fin);
       close(fout);
       return(0);
    }
    else
    {
           printf("Encoding %d channel %d bits %d Hz WAV file\n",
                   sWaveHeader.ui16NumChannels,sWaveHeader.ui16BitsPerSample,
                sWaveHeader.ui32SampleRate);
    }

    //
    // Set the OPUS encoder parameters
    //
    opus_encoder_ctl(sOpusEnc, OPUS_SET_BITRATE((sWaveHeader.ui32SampleRate*OPUS_BITRATE_SCALER)));
    opus_encoder_ctl(sOpusEnc, OPUS_SET_BANDWIDTH(OPUS_AUTO));
//    opus_encoder_ctl(sOpusEnc, OPUS_SET_VBR(1));
 //   opus_encoder_ctl(sOpusEnc, OPUS_SET_VBR_CONSTRAINT(0));
    opus_encoder_ctl(sOpusEnc, OPUS_SET_COMPLEXITY(complexity));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_INBAND_FEC(0));
    //opus_encoder_ctl(sOpusEnc, OPUS_SET_FORCE_CHANNELS(OPUS_AUTO));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_DTX(0));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_PACKET_LOSS_PERC(0));
    //opus_encoder_ctl(sOpusEnc, OPUS_SET_LSB_DEPTH(16));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_EXPERT_FRAME_DURATION(
     //       OPUS_FRAMESIZE_ARG));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_FORCE_MODE(MODE_CELT_ONLY));

     opus_encoder_ctl(sOpusEnc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));


    //
    // Dynamic allocation of memory for the sd card read buffer, output from
    // the codec and formatted input buffer for the codec
    //
    ui8ScaleFactor = (sWaveHeader.ui16BitsPerSample) >> 3;

    pui8data = (uint8_t *)calloc(OPUS_MAX_PACKET,sizeof(uint8_t));
    
    printf("pui8data = %p\n", pui8data);

    pcRdBuf = (char *)calloc((((sWaveHeader.ui32SampleRate*
            OPUS_FRAME_SIZE_IN_MS*
            sWaveHeader.ui16NumChannels*
            ui8ScaleFactor)/1000)+1),
            sizeof(uint8_t));

    printf("pcRdBuf = %p\n", pcRdBuf);

    popi16fmtBuffer = (opus_int16 *)calloc((((sWaveHeader.ui32SampleRate*
            OPUS_FRAME_SIZE_IN_MS*
            sWaveHeader.ui16NumChannels)/1000)+1),
            sizeof(opus_int16));

    printf("popi16fmtBuffer = %p\n", popi16fmtBuffer);

    ui32SizeOfRdBuf = (sWaveHeader.ui32SampleRate*
            sWaveHeader.ui16NumChannels*
            OPUS_FRAME_SIZE_IN_MS*
            ui8ScaleFactor)/1000;
    ui32Sizeofpopi16fmtBuffer = (sWaveHeader.ui32SampleRate*
            OPUS_FRAME_SIZE_IN_MS*
            sWaveHeader.ui16NumChannels*
            sizeof(opus_int16))/1000;

    printf("ui32SizeOfRdBuf %d ui32Sizeofpopi16fmtBuffer %d\n", ui32SizeOfRdBuf, ui32Sizeofpopi16fmtBuffer);
    //
    // Create the opx file header
    //
    strcpy(cOpxDelimiter, "HDR\0");
    iFWrResult = write(fout, (void*)&cOpxDelimiter[0], 4);
    iFWrResult = write(fout, (void*)&sWaveHeader.ui16NumChannels, 2);
    iFWrResult = write(fout, (void*)&sWaveHeader.ui16BitsPerSample,2);
    iFWrResult = write(fout, (void*)&sWaveHeader.ui32SampleRate,4);
    iFWrResult = write(fout, (void*)&sWaveHeader.ui32SubChunk2Size,4);

    //
    // Enter a loop to repeatedly read the wav data from the file, encode it
    // and then store it in the sd card.
    //
    //
    int pos = lseek(fin, 0, SEEK_CUR);
    int filesize = lseek(fin, 0, SEEK_END);
    printf("filesize: %d\n", filesize);
    pos = lseek(fin, 0, SEEK_SET);
    
    do
    {
        //
        // Read a block of data from the file as specified by the selected
        // frame size in the header file
        //
        pos = lseek(fin, 0, SEEK_CUR);
        if(pos + ui32SizeOfRdBuf < filesize)
            ui32BytesRead = read(fin, (void*)pcRdBuf, ui32SizeOfRdBuf);
        else
            ui32BytesRead = read(fin, (void*)pcRdBuf, filesize - pos);
        printf("ui32BytesRead = %d\n", ui32BytesRead);
        //
        // Process the data as per the scale factor. A scale factor of 1 is
        // applied when the data is 8 bit and a scale factor of 2 is applied
        // when the data is 16 bit.
        //

#if 1
        for(ui32Loop = 0 ; ui32Loop < ui32BytesRead ; ui32Loop++)
        {
            if(ui8ScaleFactor == 1)
            {
                   popi16fmtBuffer[ui32Loop] = (opus_int16)pcRdBuf[ui32Loop];
            }
            else if(ui8ScaleFactor == 2)
            {
                if(ui32Loop%2 == 0)
                    popi16fmtBuffer[ui32Loop/2] = pcRdBuf[ui32Loop];
                else
                    popi16fmtBuffer[ui32Loop/2] |= (pcRdBuf[ui32Loop] << 8);
            }

        }

        //
        // If there was an error reading the data then free the buffers, close
        // the file handlers, destory the encoded structure.
        //
        if(ui32BytesRead < 0)
        {
            printf("read failed\n");
            free(pui8data);
            free(pcRdBuf);
            free(popi16fmtBuffer);
            close(fin);
            close(fout);
            opus_encoder_destroy(sOpusEnc);
            return(-1);
        }
        
        //
        // If no error with file handling then start the compression.
        //
        i32len = opus_encode(sOpusEnc,
                popi16fmtBuffer,
                (ui32Sizeofpopi16fmtBuffer/2),
                pui8data,
                OPUS_MAX_PACKET);

        //
        // If this is not the last packet then add the 'Mid ' as delimiter
        // else add 'End ' as the delimiter which will be used during the
        // decompression process.
        //
        if(ui32BytesRead == ui32SizeOfRdBuf)
        {
            strcpy(cOpxDelimiter, "Mid\0");
        }
        else
        {
            strcpy(cOpxDelimiter, "End\0");
        }

        //
        // Store the compressed data with length of data, followed by the
        // delimiter and then the actual compressed data
        //
        iFWrResult = write(fout, (void*) &cOpxDelimiter[0], 4);
        iFWrResult = write(fout, (void*)&i32len, 4);
        iFWrResult = write(fout, (void*)pui8data, i32len);

        //
        // Add the length of the wav file and the compressed data for printing
        // the statistics
        //

#endif
 //       printf("encoded len = %d\n", i32len);
        ui32EncodedLen += i32len;
        ui32RawLen     += ui32BytesRead;

        printf("OPUS Encoder Completion: %03d\r",
                ((ui32RawLen*100)/sWaveHeader.ui32SubChunk2Size));
    }
    while(ui32BytesRead == ui32SizeOfRdBuf);

    //
    // Print the compression statistics
    //
    printf("\n***********STATISTICS*************\n");
    printf("Raw WAV Bytes      = %d\n", ui32RawLen);
    printf("Encoded OPX Bytes  = %d\n", ui32EncodedLen);
    printf("Compression Factor = %02d.%02d\n", (ui32RawLen/ui32EncodedLen),
            (((ui32RawLen*100)/ui32EncodedLen)-((ui32RawLen/ui32EncodedLen)*100)));

    //
    // Free the buffers that have been dynamically allocated
    //
    free(pui8data);
    free(pcRdBuf);
    free(popi16fmtBuffer);

    //
    // Close Read and Write file
    //
    close(fin);
    close(fout);

    //
    // free the memory assigned to the encode
    //
    opus_encoder_destroy(sOpusEnc);

    //
    // Return success.
    //
    return(0);
}

//*****************************************************************************
//
// This function implements the "testenc" command. It tests the encoder by
// reading an input wav file and runs ths encoder with different complexity
// setting. There is no output file generated but only statistics.
//
//*****************************************************************************
int
Cmd_encode_performance(int argc, char *argv[])
{
    //FRESULT  iFRdResult;
    uint8_t  *pui8data;
    uint8_t  ui8ScaleFactor;
    char     *pcRdBuf;
    uint32_t ui32BytesRead;
    uint32_t ui32Sizeofpopi16fmtBuffer;
    uint32_t ui32Loop;
    uint32_t ui32EncodedLen;
    uint32_t ui32RawLen;
    //uint32_t ui32TimeStart;
    uint32_t ui32TimeElapsed;
    uint32_t ui32FrameCount;
    uint32_t ui32SizeOfRdBuf;
    int32_t  i32error;
    int32_t  i32len;
    uint8_t  ui8ComplexityIndex = 7;

#ifdef ENCODE_SCENE
    mqd_t mq; 
    struct mq_attr attr;
    struct opus_msg msg;
    int seqnum=0;
    int ret;
#endif

    //FILE *fin;
    //FILE *fout;
    tWaveHeader sWaveHeader;
    opus_int16 *popi16fmtBuffer;
    OpusEncoder *sOpusEnc;
    int pos = 0; 
    struct timeval tv1; 
    struct timeval tv2; 
    //unsigned long mtime1 = 0;
    unsigned long mtime2 = 0;
    
    argc++;
    if(strcmp(argv[argc], "-complexity" ) == 0 ) 
        ui8ComplexityIndex = atoi(argv[argc+1]);
 
#ifdef ENCODE_SCENE

    attr.mq_maxmsg = 20; 
    attr.mq_msgsize = sizeof(msg);
    attr.mq_flags = 0;
    mq = mq_open("mq1", O_RDWR | O_CREAT, 0644, &attr);
    if (mq == NULL)
    {
      /* Error creating message queue! */
      printf("ERROR: Couldn't allocate message queue\n");
      return -1;
    }

#endif
    //
    // Print the header for the encoder performance
    //
    printf("\n\n");
    printf("INPUT STREAM SIZE %d ms\n\n",OPUS_FRAME_SIZE_IN_MS);
    printf("***************************************ENCODER STATISTICS*****************************************\n\n");
    printf("COMPLEXITY | RAW DATA | OUTPUT DATA | FRAMES | TOTAL TIME (s) | COMPRESSION | PER FRAME TIME (ms)\n\n");

    //
    // Loop through the different complexity for the encoder
    //
//       for(ui8ComplexityIndex = 0; ui8ComplexityIndex < 11 ;ui8ComplexityIndex++)
      //{
        //
        // Reset the statistic counters to 0
        //
        ui32EncodedLen  = 0;
        ui32RawLen      = 0;
        ui32TimeElapsed = 0;
        ui32FrameCount  = 0;
        pos = 0;
        //
        // Open the file for reading.
        //
#if 0
        fin = fopen(argv[argc], "rb");

        //
        // If there was some problem opening the file, then return an error.
        //
        if(fin == NULL)
        {
            printf("open file %s fail\n", argv[argc]);
            return -1;
        }
#endif
        //
        // Get the Header Information
        //
       // iFRdResult = fread((void*)&sWaveHeader, sizeof(sWaveHeader),1, fin);
         ui32BytesRead = read_from_flash((char*)&sWaveHeader,sizeof(sWaveHeader), pos);
    
         pos+=ui32BytesRead; 

#if 0
        //
        // Check if the WAV file header is correctly formatted
        //
        if((strncmp((char *)&sWaveHeader.ui8ChunkID[0],"RIFF",4) != 0) ||
           (strncmp((char *)&sWaveHeader.ui8Format[0],"WAVE",4) != 0) ||
           (strncmp((char *)&sWaveHeader.ui8SubChunk1ID[0],"fmt ",4) != 0) ||
           (strncmp((char *)&sWaveHeader.ui8SubChunk2ID[0],"data",4) != 0))
        {
            printf("ENC_ERR: WAV file is invalid\n");
            fclose(fin);
            return(0);
        }

        //
        // Since OPUS supports 8k, 16k, 24k, 32k or 48k sampling rates a check is
        // is made to see if the incoming audio stream also has the same property
        //
        if((sWaveHeader.ui32SampleRate != 8000)  &&
           (sWaveHeader.ui32SampleRate != 16000) &&
           (sWaveHeader.ui32SampleRate != 24000) &&
           (sWaveHeader.ui32SampleRate != 32000) &&
           (sWaveHeader.ui32SampleRate != 48000) )
        {
            printf("ENC_ERR: Sample Rate Must be either 8k, 16k, 24k, 32k or ");
            printf("48k\n");
            //fclose(fin);
            return(0);
        }

        //
        // Only Linear PCM format is supported
        //
        if(sWaveHeader.ui16AudioFormat != 0x1)
        {
            printf("ENC_ERR: Only Linear PCM Format Supported\n");
            //fclose(&fin);
            return(0);
        }

#endif
        //
        // Create the encoder
        //
        sOpusEnc = opus_encoder_create(sWaveHeader.ui32SampleRate,
                sWaveHeader.ui16NumChannels, OPUS_APPLICATION_AUDIO, &i32error);

        //
        // If there is an error creating the encoder then close the input and
        // output file handle. Else print the information on the file parameters
        //
        if (i32error != OPUS_OK)
        {
           printf("ENC_ERR: Cannot create encoder: %s\n",
                   opus_strerror(i32error));
           //fclose(fin);
           //fclose(fout);
           return(0);
        }

        //
        // Set the OPUS encoder parameters
        //
 opus_encoder_ctl(sOpusEnc, OPUS_SET_BITRATE((sWaveHeader.ui32SampleRate*OPUS_BITRATE_SCALER)));
    opus_encoder_ctl(sOpusEnc, OPUS_SET_BANDWIDTH(OPUS_AUTO));
//    opus_encoder_ctl(sOpusEnc, OPUS_SET_VBR(1));
 //   opus_encoder_ctl(sOpusEnc, OPUS_SET_VBR_CONSTRAINT(0));
    opus_encoder_ctl(sOpusEnc, OPUS_SET_COMPLEXITY(ui8ComplexityIndex));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_INBAND_FEC(0));
    //opus_encoder_ctl(sOpusEnc, OPUS_SET_FORCE_CHANNELS(OPUS_AUTO));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_DTX(0));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_PACKET_LOSS_PERC(0));
    //opus_encoder_ctl(sOpusEnc, OPUS_SET_LSB_DEPTH(16));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_EXPERT_FRAME_DURATION(
     //       OPUS_FRAMESIZE_ARG));
   // opus_encoder_ctl(sOpusEnc, OPUS_SET_FORCE_MODE(MODE_CELT_ONLY));

     opus_encoder_ctl(sOpusEnc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));



        //
        // Dynamic allocation of memory for the sd card read buffer, output from
        // the codec and formatted input buffer for the codec
        //
    
        
        ui8ScaleFactor = (sWaveHeader.ui16BitsPerSample) >> 3;

        pui8data = (uint8_t *)calloc(OPUS_MAX_PACKET,sizeof(uint8_t));
        pcRdBuf = (char *)calloc((((sWaveHeader.ui32SampleRate*
                OPUS_FRAME_SIZE_IN_MS*
                sWaveHeader.ui16NumChannels*
                ui8ScaleFactor)/1000)+1),
                sizeof(uint8_t));
        popi16fmtBuffer = (opus_int16 *)calloc((((sWaveHeader.ui32SampleRate*
                OPUS_FRAME_SIZE_IN_MS*
                sWaveHeader.ui16NumChannels)/1000)+1),
                sizeof(opus_int16));

        ui32SizeOfRdBuf = (sWaveHeader.ui32SampleRate*
                sWaveHeader.ui16NumChannels*
                OPUS_FRAME_SIZE_IN_MS*
                ui8ScaleFactor)/1000;
        ui32Sizeofpopi16fmtBuffer = (sWaveHeader.ui32SampleRate*
                OPUS_FRAME_SIZE_IN_MS*
                sWaveHeader.ui16NumChannels*
                sizeof(opus_int16))/1000;

        //
        // Enter a loop to repeatedly read the wav data from the file, encode it
        // and then store it in the sd card.
        //
        
        #ifdef ENCODE_SCENE
            //msg.seqnum = seqnum++;
            msg.code = OPUS_ENCODE_START;
            //gettimeofday(&tv1, NULL); 
            //msg.ts.tv_sec = tv1.tv_sec;
            //msg.ts.tv_usec = tv1.tv_usec;
            
            ret = mq_send(mq, (char*)&msg, sizeof(msg),OPUS_MSG_PRIO);
            if(ret < 0)
            {
                printf("send fail\n");
                return;

            } 
        #endif
            //sleep(2);

        do
        {
            //
            // Read a block of data from the file as specified by the selected
            // frame size in the header file
            //
        //    ui32BytesRead = fread(pcRdBuf, 1, ui32SizeOfRdBuf, fin);
#if 0
              if(pos + ui32SizeOfRdBuf < FILESIZE)
              {
                    ui32BytesRead = read_from_flash((char*)pcRdBuf, ui32SizeOfRdBuf, pos);
              }
              else
              {
                  ui32BytesRead = read_from_flash((char*)pcRdBuf, (FILESIZE-pos), pos);
              }
              pos+=ui32BytesRead;
#endif              
       
            //
            // Process the data as per the scale factor. A scale factor of 1 is
            // applied when the data is 8 bit and a scale factor of 2 is applied
            // when the data is 16 bit.
            //
              for(ui32Loop = 0 ; ui32Loop < ui32BytesRead ; ui32Loop++)
             {
                if(ui8ScaleFactor == 1)
                {
                       popi16fmtBuffer[ui32Loop] = (opus_int16)pcRdBuf[ui32Loop];
                }
                else if(ui8ScaleFactor == 2)
                {
                    if(ui32Loop%2 == 0)
                        popi16fmtBuffer[ui32Loop/2] = pcRdBuf[ui32Loop];
                    else
                        popi16fmtBuffer[ui32Loop/2] |= (pcRdBuf[ui32Loop] << 8);
                }

            }

            //
            // If there was an error reading the data then free the buffers, close
            // the file handlers, destory the encoded structure.
            //
            if( ui32BytesRead < 0)
            {
                free(pui8data);
                free(pcRdBuf);
                free(popi16fmtBuffer);
            //    fclose(fin);
                opus_encoder_destroy(sOpusEnc);
                return -1;
            }

            // Enable the timer
            
            
            // If no error with file handling then start the compression.
#ifndef CPU_LOAD_MONITOR 
            gettimeofday(&tv1, NULL); 
#endif

            i32len = opus_encode(sOpusEnc,
                    popi16fmtBuffer,
                    (ui32Sizeofpopi16fmtBuffer/2),
                    pui8data,
                    OPUS_MAX_PACKET);

             //get_cpu_load();

#ifndef CPU_LOAD_MONITOR
            gettimeofday(&tv2, NULL);
            mtime2 +=(tv2.tv_sec*1000+tv2.tv_usec/1000) - (tv1.tv_sec*1000 + tv1.tv_usec/1000);
#endif
            //
            // Disable the timer and get the value
            //
 
           // Add the length of the wav file and the compressed data for printing
            // the statistics
            //
            ui32EncodedLen += i32len;
            ui32RawLen     += ui32BytesRead;
            ui32FrameCount++;

        }
        while(ui32BytesRead == ui32SizeOfRdBuf);

        get_cpu_load();
#ifdef CPU_LOAD_MONITOR
       // get_task_load();
#endif


#ifdef ENCODE_SCENE
            printf("%05d\n          ", mtime2);
            msg.seqnum = seqnum++;
            msg.code = OPUS_ENCODE_EXIT;
            gettimeofday(&tv1, NULL); 
            msg.ts.tv_sec = tv1.tv_sec;
            msg.ts.tv_usec = tv1.tv_usec;
            ret = mq_send(mq, (char*)&msg, sizeof(msg),OPUS_MSG_PRIO);
#endif


                // Print the compression statistics
        //
#ifndef CPU_LOAD_MONITOR
        printf("%01d           ", ui8ComplexityIndex);
        printf("%05d     ", ui32RawLen);
        printf("%07d      ", ui32EncodedLen);
        printf("%05d     ", ui32FrameCount);
        printf("%05d          ", mtime2);
        printf("%02d.%02d          ", (ui32RawLen/ui32EncodedLen),
                (((ui32RawLen*100)/ui32EncodedLen)-
                        ((ui32RawLen/ui32EncodedLen)*100)));
        printf("%02d.%02d\n",mtime2/ui32FrameCount, ((mtime2*100)/ui32FrameCount)-((mtime2/ui32FrameCount)*100));
        
#endif    
        // Free the buffers that have been dynamically allocated
        //
        free(pui8data);
        free(pcRdBuf);
        free(popi16fmtBuffer);

        //
        // Close Read and Write file
        //
      //  fclose(fin);

        //
        // free the memory assigned to the encode
        //
        opus_encoder_destroy(sOpusEnc);
        //}

#ifdef ENCODER_SCENE
        mq_close(mq);
#endif
    //
    // Return success.
    //
    return(0);
}

//*****************************************************************************
//
// This function implements the "dec" command. It is provided with 2 parameters
// The first parameter is an input opx file and the second parameter is the
// output wav file decompressed with OPUS.
//
//*****************************************************************************
int
Cmd_decode(int argc, char *argv[])
{
    FRESULT  iFRdResult;
    FRESULT  iFWrResult;
    uint8_t  *pcRdBuf;
    uint8_t  ui8ProgressDisplay=0;
    char     cOpxDelimiter[4];
    uint16_t ui32BitsPerSample;
    uint16_t ui32Channel;
    //uint32_t ui32BytesRead;
    //uint32_t ui32BytesWrite;
    uint32_t ui32SizeOfOutBuf;
    uint32_t ui32Loop;
    uint32_t ui32SamplingRate;
    uint32_t ui32WavFileSize;
    uint32_t ui32EncodedLen=0;
    uint32_t ui32RawLen=0;
    int32_t  i32error;
    int32_t  i32len;
    int32_t  i32OutSamples;
    tWaveHeader sWaveHeader;
    opus_int16 *pcop16OutBuf;
    OpusDecoder *sOpusDec;
    int fin;
    int fout;
    struct timeval tv1; 
    struct timeval tv2; 
    unsigned long mtime = 0;
    //unsigned long mtime1 = 0;


    argc++; 
    fin = open(argv[argc], O_RDWR);
    if(fin < 0)
    {
        fprintf(stderr, "fin open %s fail\n", argv[2]);
        return -1;
    }

    argc++;
    fout = open(argv[argc], O_RDWR | O_CREAT | O_TRUNC);

    if(fout < 0)
    {
        close(fin);
        fprintf(stderr, "fout open %s fail\n", argv[3]);
        return -1;
    }

    //
    // Read the opx file for header information
    //
    iFRdResult = read(fin, (void*)&cOpxDelimiter, 4);
    iFRdResult = read(fin, (void*)&ui32Channel, 2);
    iFRdResult = read(fin, (void*)&ui32BitsPerSample, 2);
    iFRdResult = read(fin, (void*)&ui32SamplingRate, 4);
    iFRdResult = read(fin, (void*)&ui32WavFileSize, 4);

    //
    // If the ASCII string 'HDR' is not found then this may not b a valid opx
    // file and close the file handlers
    //
    if(strncmp(&cOpxDelimiter[0],"HDR",3) != 0)
    {
        fprintf(stderr, "DEC_ERR: OPX file is invalid\n");
        close(fin);
        close(fout);
        return(0);
    }
    else
    {
        fprintf(stderr, "DEC: Original File is %u bytes\n",ui32WavFileSize);
    }

    //
    // Create the WAV file header using information collected during the
    // decompression process.
    //
    strcpy((char *)sWaveHeader.ui8ChunkID,"RIFF");
    strcpy((char *)sWaveHeader.ui8Format,"WAVE");
    strcpy((char *)sWaveHeader.ui8SubChunk1ID,"fmt ");
    strcpy((char *)sWaveHeader.ui8SubChunk2ID,"data");
    sWaveHeader.ui32ChunkSize = 36;
    sWaveHeader.ui32SubChunk1Size = 16;
    sWaveHeader.ui16AudioFormat = 1;
    sWaveHeader.ui16NumChannels = ui32Channel;
    sWaveHeader.ui32SampleRate = ui32SamplingRate;
    sWaveHeader.ui32ByteRate = ui32SamplingRate*ui32Channel*(ui32BitsPerSample/8);
    sWaveHeader.ui16BlockAlign = ui32Channel*(ui32BitsPerSample/8);
    sWaveHeader.ui16BitsPerSample = ui32BitsPerSample;
    sWaveHeader.ui32SubChunk2Size = 0;

    //
    // Write the wav file header to the final file
    //
    iFWrResult = write(fout, (void*)&sWaveHeader,
            sizeof(sWaveHeader));

    //
    // If the opx file format is correct then create the decoder
    //
    sOpusDec = opus_decoder_create(ui32SamplingRate, ui32Channel, &i32error);

    //
    // If there was some problem creating the OPUS decoder, then close the write
    // and read file handle and return an error
    //
    if (i32error != OPUS_OK)
    {
       fprintf(stderr, "DEC_ERR: Cannot create decoder: %s\n", opus_strerror(i32error));
       close(fin);
       close(fout);
       return((int)i32error);
    }
    else
    {
        fprintf(stderr, "Decoding %d Channels at %d Sampling Rate\n",ui32Channel,ui32SamplingRate);
    }

    //
    // Set the parameters for the OPUS decoder
    //
    opus_decoder_ctl(sOpusDec, OPUS_SET_LSB_DEPTH(ui32BitsPerSample));

    //
    // Dynamic allocation of memory for the sd card read buffer and the output
    // from the codec.
    //
    ui32SizeOfOutBuf = (ui32SamplingRate*
            ui32Channel*
            OPUS_FRAME_SIZE_IN_MS*
            OPUS_DATA_SCALER)/1000;
    pcop16OutBuf     = (int16_t*)calloc((
            (ui32SizeOfOutBuf/OPUS_DATA_SCALER)+1),
            sizeof(int16_t));
    pcRdBuf          = (uint8_t *)calloc(OPUS_MAX_PACKET,sizeof(uint8_t));


    printf("======pcop16OutBuf pcRdBuf %p %p\n", pcop16OutBuf, pcRdBuf);
    //
    // Enter a loop to repeatedly read data from the file and display it, until
    // the end of the file is reached.
    //
    do
    {
        //
        // Read the delimiter and length first
        //
        //

        iFRdResult = read(fin, &cOpxDelimiter, 4);
        iFRdResult = read(fin, &i32len, 4);

        //
        // Read a block of data from the file as specified by the length.
        //
        iFRdResult = read(fin, pcRdBuf, i32len);

        //
        // If there was an error reading, then print a newline and return the
        // error to the user.
        //
        if(iFRdResult < 0)
        {
            fprintf(stderr, "DEC_ERR: File Processing Error\n");

            //
            // Free the buffers
            //
            free(pcop16OutBuf);
            free(pcRdBuf);

            //
            // Close Write file
            //
            close(fin);
            close(fout);

            //
            // destroy the decoder to free up the memory
            //
            opus_decoder_destroy(sOpusDec);

            return((int)iFRdResult);

        }

        //
        // Now start the decompression process
        //
#if 1
        gettimeofday(&tv1, NULL); 
        i32OutSamples = opus_decode(sOpusDec,
                (const unsigned char *)&pcRdBuf[0],
                i32len,
                pcop16OutBuf,
                (ui32SizeOfOutBuf/OPUS_DATA_SCALER), 0);

        gettimeofday(&tv2, NULL); 
        mtime +=(tv2.tv_sec*1000+tv2.tv_usec/1000) - (tv1.tv_sec*1000 + tv1.tv_usec/1000);
#endif    
        //
        // If there is an error in the decoder then free the buffer and
        // destroy the decoder to free the memory
        //
        //
       // i32OutSamples = OPUS_OK;

        if (i32OutSamples < OPUS_OK)
        {
           fprintf(stderr, "DEC_ERR: Decode Failed %s\n", opus_strerror(i32error));

           free(pcop16OutBuf);
           free(pcRdBuf);

           close(fin);
           close(fout);

           opus_decoder_destroy(sOpusDec);

           return(1);
        }

        //
        // Based on the original bits per sample, perform bit operation for
        // final wav file
        //
        printf ("ui32BitsPerSample: %d\n", ui32BitsPerSample);
        if(ui32BitsPerSample == 8)
        {
            //
            // If the data is 8 bit then convert from signed to unsigned format
            //
            for(ui32Loop = 0 ; ui32Loop < (i32OutSamples) ; ui32Loop++)
            {
                pcRdBuf[ui32Loop] = (uint8_t)(pcop16OutBuf[ui32Loop] ^ 0x80);
            }

            //
            // Write the data to the temporary file
            //
            iFWrResult = write(fout, (void*)pcRdBuf, i32OutSamples);

            //
            // Add the number of bytes from the decoder output for statistics
            //
            ui32RawLen     += i32OutSamples;
        }
        else
        {
            //
            // If the data is 16 bit then write the data as is to the output
            // temporary file
            //

              iFWrResult = write(fout, (void*)pcop16OutBuf, (i32OutSamples*OPUS_DATA_SCALER));

            //
            // Add the number of bytes from the decoder output for statistics
            //
                 ui32RawLen     += i32OutSamples*OPUS_DATA_SCALER;
        }

        //
        // Add the opx file byte length for each of the segments for statistic.
        //

        ui32EncodedLen += i32len;

        //
        // Increment the progress display on the command line

    
        ui8ProgressDisplay++;

        if((ui8ProgressDisplay/100) == 1)
        {
            ui8ProgressDisplay = 0;
            fprintf(stderr, "\r.");
        }
        else if((ui8ProgressDisplay%20) == 0)
        {
            fprintf(stderr, ".");
        }
    }
    while(strncmp(&cOpxDelimiter[0],"End",3) != 0);
    //get_cpu_load();
   

    fprintf(stderr, "\r");

    //
    // Go back to top of the file to update the Size of the file
    //
    lseek(fout, 0, SEEK_SET);

    //
    // Create the WAV file header using information collected during the
    // decompression process.
    //
    sWaveHeader.ui32ChunkSize = ui32RawLen + 36;
    sWaveHeader.ui32SubChunk2Size = ui32RawLen;

    //
    // Write the wav file header to the final file
    //
    iFWrResult = write(fout, (void*)&sWaveHeader, sizeof(sWaveHeader));

    //
    // Free the buffers
    //

    free(pcop16OutBuf);
    free(pcRdBuf);

    //
    // Close Write file
    //
    close(fin);
    close(fout);

    //
    // destriy the decoder to free the memory allocated to it.
    //
    opus_decoder_destroy(sOpusDec);

    //
    // Print the statistics
    //
    printf("\n***********STATISTICS*************\n");
    printf("Input OPX Bytes   = %d\n", ui32EncodedLen);
    printf("Decoded WAV Bytes = %d\n", ui32RawLen);
    printf("Decoded time = %lu\n", mtime);

    //
    // Return success.
    //
    return(0);
}

int opus_demo_entry(void *arg)
{
    int argc;
    char **argv;
    struct myparm *param = (struct myparm *)arg;
    argc = param->argc;
    argv = param->argv;
    
    argc = 1;

    if (strcmp(argv[argc], "-e")==0)
    {
        Cmd_encode(argc, argv);

    }
    else if (strcmp(argv[argc], "-d")==0)
    {
        Cmd_decode(argc, argv);
    }
    
    else if (strcmp(argv[argc], "-encperf")==0)
    {
        Cmd_encode_performance(argc, argv);
    }

    return 0;
}
