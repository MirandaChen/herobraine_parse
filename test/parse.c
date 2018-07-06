/* Yinglan Chen, July 2018 */

/* a parse function that takes in a player_stream file and parse it to 
 * FILE_NUM output files.
 * player_stream has the following format:
    [entry][sequence_number][time_stamp][len][json data]
 * implementation:
 *  keep append files open, open and close write files
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

const int OVERWRITE = 0;
const int APPEND = 1;
const int FILE_NUM = 14;
const int BUF_LEN = 500000;
const char* default_src_stream = "new.bin";
const char* FILES[] =  {
    "null",                     /* 0 */
    "metaData.json", 
    "recording.tmcpr",         
    "resource_pack.zip",         
    "resource_pack_index.json" ,  
    "thumb.json",                 
    "visibility",        
    "visibility.json" ,           
    "markers.json",               
    "asset.zip",  
    "pattern_assets.zip",               
    "mods.json",      
    "end_of_stream.txt",              
    "stream_meta_data.json" 
};
const int WRITE_METHODS[] =  {
    APPEND,                     /* 0 */
    OVERWRITE, 
    APPEND,                     /* 2 */
    OVERWRITE,         
    OVERWRITE ,                 /* 4 */ 
    OVERWRITE,                 
    OVERWRITE,                  /* 6 */ 
    OVERWRITE ,           
    OVERWRITE,                  /* 8 */           
    APPEND,   
    APPEND,                     /* 10 */ 
    OVERWRITE,      
    APPEND,                     /* 12 */ 
    OVERWRITE
};

/* use this part for easier testing
const char* Files[] =  {
    "output0",                       
    "output1.json",
    "output2.tmcpr",          
    "output3.zip",         
    "output4.json" ,  
    "output5.json",                 
    "output6",        
    "output7.json" ,           
    "output8.json",               
    "output9.zip",                 
    "output10.json",      
    "output11.json",              
    "output12.json" 
        // 4 on brandon's side and 5 here   
};
*/ 

/*
 * If DEBUG is defined, enable printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#endif

/* function prototypes */
unsigned int string_to_unsigned_int(char* buf);
unsigned int get_entry(char* buf, FILE* stream);
unsigned int get_sequence_number(char* buf, FILE* stream);
unsigned int get_time_stamp(char* buf, FILE* stream);
unsigned int get_len(char* buf, FILE* stream);


void parse(FILE* input)
{ 
    // be careful: error handling, check return of fread value later 
    FILE* output;
    FILE* outputs[FILE_NUM];
    int counter = 0, err_check;
    char buf[BUF_LEN];
    char* backup_buf;
    unsigned int entry, time_stamp, len, sequence_number;
    
    // open all "a" output
    for (int i = 0; i < FILE_NUM; i++)
    {
        if (WRITE_METHODS[i] == APPEND)
        {
            outputs[i] = fopen(FILES[i],"r+");
            if (outputs[i] == NULL)
            {
                printf("error opening file %s. Abort\n", FILES[i]);
                exit(-1);
            }
            else
            {
                dbg_printf("successfully open file [%d] %s\n", i, FILES[i]);
            }
        }
    }


    // to-do: distinguish EOF and error
    while ((err_check= fread(buf, 4, 1, input)) == 1)
    {
        dbg_printf("[%d]\n",counter);
        /* entry: first fread in while loop condition */
        entry = get_entry(buf, input);
        
        /* sequence_number */
        fread(buf, 4, 1, input);
        sequence_number = get_sequence_number(buf, input);

        /* time_stamp */
        fread(buf, 4, 1, input);
        time_stamp = get_time_stamp(buf, input);

        /* len */
        fread(buf, 4, 1, input);
        len = get_len(buf, input);
        
        /* data */
        if (len > BUF_LEN) // handle exception first
        {
            printf("detect data with %u bytes\n", len );
            backup_buf = malloc(sizeof(len)); // and continue to use this buf
            if (backup_buf == NULL)
            {
                printf("failed to allocate new buffer, abort\n");
                // no file leaks since close rightaway
                exit(-1);
            }
            fread(backup_buf , len, 1, input);
            dbg_printf("data too long, omit printing\n");
        }
        else
        {
            fread(buf, len, 1, input);
            dbg_printf("data = %s\n", buf);
        }
        
        
        // open and write output
        if (entry < FILE_NUM){ 
            // case 1: append, directly write
            if (WRITE_METHODS[entry] == APPEND)
            {
                if ((err_check = fwrite(buf, len, 1, outputs[entry])) != 1)
                {

                    printf("err_check = %d,trouble writing to output file[%d] %s\n",err_check,entry,FILES[entry] );
                }
            }
            // case 2: overwrite
            else
            {
                // change to dbg_assert to improve performance
                assert(WRITE_METHODS[entry] == OVERWRITE );
                output = fopen(FILES[entry], "w+");
                // error checking
                if (output == NULL) 
                {
                    printf("failed to open the output file %d\n", entry);
                }
                // write and close
                if (fwrite(buf, len, 1, output) != 1)
                {
                    printf("trouble writing to output\n" );
                }
                fclose(output);
            }
            
        }

        else // invalid entry
        {
            printf("corrupted data? with entry = %u\n", entry);
        }


        dbg_printf("file position: %ld\n\n", ftell(input));
        counter++;
    }

    // close all "a" output
    for (int i = 0; i < FILE_NUM; i++)
    {
        if (WRITE_METHODS[i]== APPEND)
        {
            err_check = fclose(outputs[i]);
            if (err_check != 0)
            {
                printf("error closing file %s. Abort\n", FILES[i]);
            }
            else
            {
                dbg_printf("successfully close file [%d] %s\n", i, FILES[i]);
            }
        }
    }
}


/* MAIN */
// Exception CorruptedStream?
// next: overwrite or append? 
// future: output a meta data json for my streaming. 
//      {error: false, eof: true, esg:...}
int main(int argc, char **argv)
{   
    int opt;
    size_t err_check;
    FILE* input;
    const char* src_stream = default_src_stream;

    // get 
    while ((opt = getopt(argc, argv, "f:")) != -1){
        switch (opt) {
            case 'f':
                src_stream = optarg;
                break;
        }
    }
    printf("running %s...\n", src_stream);

    // open input file
    input = fopen(src_stream,"r");
    if (input == NULL)
    {
        printf("Error opening the source stream. Abort.\n");
    }

    // the main parse function
    parse(input);
    
    // close input file
    err_check = fclose(input);
    if (err_check != 0)
    {
        printf("error closing the source stream. \n");
    }
    // finish and return
    printf("finish parsing the source stream.\n");
    return 0;
}

/* HELPER FUNCTIONS */

/* convert string to unsigned int*/
unsigned int string_to_unsigned_int(char* buf)
{
    unsigned int result = 0;
    unsigned int curr;
    for (int i = 0; i < 4; i++)
    {
        curr = (unsigned int)(unsigned char) buf[i];
        // printf("curr: %u\n", curr );
        result = (result << 8) + curr;
    }
    return result;
}

// return entry, assume already copied to buf
unsigned int get_entry(char* buf, FILE* stream)
{
    unsigned int entry = string_to_unsigned_int(buf);
    dbg_printf("entry = %u\n", entry);
    return entry;
}

// return time_stamp, assume already copied to buf
unsigned int get_time_stamp(char* buf, FILE* stream)
{
    unsigned int time_stamp = string_to_unsigned_int(buf);
    dbg_printf("time_stamp = %u\n", time_stamp);
    return time_stamp;
}

// return time_stamp, assume already copied to buf
unsigned int get_len(char* buf, FILE* stream)
{
    unsigned int len = string_to_unsigned_int(buf);
    dbg_printf("len = %u\n", len);
    return len;
}

// return sequence_number, assume already copied to buf
unsigned int get_sequence_number(char* buf, FILE* stream)
{
    unsigned int sequence_number = string_to_unsigned_int(buf);
    dbg_printf("sequence_number = %u\n", sequence_number);
    return sequence_number;
}