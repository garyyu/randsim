

#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>

#include <unistd.h>

#include <stdio.h>
#include <sys/select.h>
#include <poll.h>
#include <string.h>
#include <inttypes.h>

#include <set>
#include <map>
#include <random>                       //  50M/s, Too Slow
#include "SFMT.h"                       // 756M/s, Super Fast
#include "os_wrapper.h"

typedef enum
{
    INS_rand_wait           = 0,
    INS_rand_start             ,
    INS_rand_stop              ,
    INS_rand_pause             ,

    INS_RAND_MAX
} instruction_opcode_t;

typedef enum
{
    ALGO_SFMT_SSE2_SEQUE           = 0,     // SFMT Sequence Algorithm by SSE2 Implementation
    ALGO_SFMT_SSE2_BLOCK              ,     // SFMT Block Algorithm by SSE2 Implementation
    ALGO_SYSTEM_RANDOM                ,     // System Random Algorithm std::mt19937

    ALGO_MAX
} rand_algo_type;
const char *rand_algo_str[ALGO_MAX] = {
        "SFMT Sequence Algorithm by SSE2"           ,
        "SFMT Block Algorithm by SSE2"              ,
        "System Random Algorithm by std::mt19937"   ,
};

static instruction_opcode_t instructionShared = INS_rand_wait;
static bool          bRandGenerating = false;
static std::mutex    minerMutex;

static uint64_t randomGenerated = 0;

#define          ALLNODES       8388608        // simulate whole network miner nodes number
static const int networknodes = ALLNODES;
static int   activethreads = 3;

typedef enum{
    zero32bit_heading,
    one32bit_heading,
} heading_type;

static inline void report_news(uint64_t nTime, uint64_t magicNumber, heading_type headingtype)
{
    msg_t goodnews;
    if (headingtype==zero32bit_heading){
        msgS_allocate(goodnews, MSG_found_odd0, nTime, magicNumber);
    }
    else{           //one32bit_heading
        msgS_allocate(goodnews, MSG_found_odd1, nTime, magicNumber);
    }
    minerMutex.lock();
    msgQ_send(QUEUE_ID_manager, goodnews);
    minerMutex.unlock();
}


static void rand_thread_entry(void)
{
    msg_t       msg;
    uint64_t    magicNumber;
    int         i;
    int loop2 = (networknodes>>5);            // must be 4*x

    w128_t     *array1 = new w128_t[(ALLNODES>>5) / 2];
    uint64_t   *array64 = (uint64_t *)array1;
    uint32_t   *array64h= (uint32_t *)array1;

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<uint64_t> uint64_dist; // by default range [0, MAX]

    uint32_t rand_seed;
    sfmt_t sfmt;

    // random seed
    rand_seed = (uint32_t)uint64_dist(rng);
    sfmt_init_gen_rand(&sfmt, rand_seed);

    if (sfmt_get_min_array_size64(&sfmt) > loop2) {
        syslog(LM_RAND, LOG_ERROR, "array size too small!\n");
        if (array1) free(array1);
        return;
    }

    while ( 0 == msgQ_recv(QUEUE_ID_worker, msg) )
    {
        if (msg_opcode(msg) == MSG_worker_quit){
                break;  // rand manager command to quit
        }

        else if (msg_opcode(msg) == MSG_worker_start)
        {
            rand_algo_type rand_algo = (rand_algo_type)msgS_data(msg);
            if (rand_algo < ALGO_MAX)
                syslog(LM_RAND, LOG_VERBOSE, "This thread's using algo: [%s]\n", rand_algo_str[rand_algo]);
            uint32_t *pMagicNumberH = (uint32_t *)&magicNumber;
            pMagicNumberH++;

            uint64_t nTime0=0, nTime1=0;
            bool     found0=false, found1=false;
            while (instructionShared==INS_rand_start){

                if (rand_algo == ALGO_SFMT_SSE2_SEQUE){
                    for (i=0; i<loop2; i++){
                        magicNumber = sfmt_genrand_uint64(&sfmt);
                        if (*pMagicNumberH == 0){
                            report_news( nTime0, magicNumber, zero32bit_heading);
                            found0 = true;
                        }
                        else if (*pMagicNumberH == -1){
                            report_news( nTime1, magicNumber, one32bit_heading);
                            found1 = true;
                        }
                    }
                }
                else if (rand_algo == ALGO_SFMT_SSE2_BLOCK){
                    sfmt_fill_array64(&sfmt, array64, loop2);

                    array64h= (uint32_t *)array1;
                    for (i=0,array64h++; i<loop2; i++, array64h+=2){
                        if (*array64h == 0){
                            report_news( nTime0, array64[i], zero32bit_heading);
                            found0 = true;
                        }
                        else if (*array64h == -1){
                            report_news( nTime1, array64[i], one32bit_heading);
                            found1 = true;
                        }
                    }
                }
                else if (rand_algo == ALGO_SYSTEM_RANDOM){
                    for (i=0; i<loop2; i++){
                        magicNumber = uint64_dist(rng);
                        if (*pMagicNumberH == 0){
                            report_news( nTime0, magicNumber, zero32bit_heading);
                            found0 = true;
                        }
                        else if (*pMagicNumberH == -1){
                            report_news( nTime1, magicNumber, one32bit_heading);
                            found1 = true;
                        }
                    }
                }

                minerMutex.lock();
                randomGenerated += loop2;
                minerMutex.unlock();
                nTime0 ++;
                nTime1 ++;
                if (found0){
                    found0 = false;
                    nTime0 = 0;
                }
                if (found1){
                    found1 = false;
                    nTime1 = 0;
                }

            }   // end of while loop of checking instructionShared
        }
    }

    if (array1) free(array1);
    return;
}

#define TOTAL_WORK_THREAD  8

static int randworker_init(void)
{
    oswrapper_init();
    msgQ_create(QUEUE_ID_minermgr);
    msgQ_create(QUEUE_ID_miner);
    if ( 0 != thread_create( THREAD_ID_worker1, rand_thread_entry ) ){
            printf("\nimpossible error! thread creation failure.");
        return -1;
    }
    if (activethreads>=2) thread_create( THREAD_ID_worker2, rand_thread_entry );
    if (activethreads>=3) thread_create( THREAD_ID_worker3, rand_thread_entry );
    if (activethreads>=4) thread_create( THREAD_ID_worker4, rand_thread_entry );
    if (activethreads>=5) thread_create( THREAD_ID_worker5, rand_thread_entry );
    if (activethreads>=6) thread_create( THREAD_ID_worker6, rand_thread_entry );
    if (activethreads>=7) thread_create( THREAD_ID_worker7, rand_thread_entry );
    if (activethreads>=8) thread_create( THREAD_ID_worker8, rand_thread_entry );

    printf("\n --- random number generation worker threads start. active thread numbers: %d---\n", activethreads);

    return 0;
}

static int randworker_term(void)
{
    oswrapper_term();
    bRandGenerating = false;
    printf("\n --- random number generation worker threads stop. ---\n");
    return 0;
}

int main(int argc, char* argv[])
{
    int totalfound0= 0;
    int totalfound1= 0;
    int loopcount = 10;       // On my mac, 1k need about 53 minutes, 15k need 11 hours
    rand_algo_type rand_algo = ALGO_SFMT_SSE2_BLOCK;

    if (argc > 4){
        syslog(LM_RAND, LOG_WARNING, "usage: randsim numbers-of-precious-32bits-leading0 threads algorithm\n\
                threads number: [1..8]\n\
                algorithm: [0: SFMT-SEQUENCE; 1: SFMT-BLOCK; 2: SYSTEM RANDOM]\n\
                Tips: if need quit during the generation, press 'q' and 'Enter'\n");

        return 0;
    }

    if (argc>=2){
        loopcount = atoi(argv[1]);
        if ((loopcount <= 0) || (loopcount > 500000)){
            syslog(LM_RAND, LOG_WARNING, "warning: random precious too big, already draw back to 10 as default.\n");
            loopcount = 10;
        }
    }

    if (argc>=3){
        activethreads = atoi(argv[2]);
        if ((activethreads <= 0) || (activethreads > 8)){
            syslog(LM_RAND, LOG_WARNING, "warning: active threads must be [1..8], already draw back to 3 as default.\n");
            activethreads = 3;
        }
    }

    if (argc>=4){
        rand_algo = (rand_algo_type)atoi(argv[3]);
        if ((rand_algo < 0) || (rand_algo > ALGO_MAX)){
            syslog(LM_RAND, LOG_WARNING, "warning: random generation algorithm parameter must be [0..2], already draw back to SFMT-BLOCK as default.\n");
            rand_algo = ALGO_SFMT_SSE2_BLOCK;
        }
    }

    syslog(LM_RAND, LOG_VERBOSE, "\nrandom simulation settings summary: precious-rand-numbers=%d, threads=%d, algorithm=[%s]\n", loopcount, activethreads, rand_algo_str[rand_algo]);

    bRandGenerating = true;
    randworker_init();

    std::set<uint32_t> intervalsets0;
    std::map<uint32_t, uint32_t /*occurrence numbers*/> intervaloccurence0;
    std::set<uint32_t> intervalsets1;
    std::map<uint32_t, uint32_t /*occurrence numbers*/> intervaloccurence1;
    uint32_t interval;

    struct timeval tp;
    long int beginMs,currMs;
    gettimeofday(&tp, NULL);
    beginMs = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    //--- Command the Miners to Start the Random Number Generation Tasks
    int i;
    minerMutex.lock();
    if (instructionShared != INS_rand_start)
    {
        instructionShared = INS_rand_start;

        msg_t taskmsg;
        msgS_allocate(taskmsg, MSG_worker_start, (uint64_t)rand_algo, (uint64_t)0);
        for (i=0; i<activethreads; i++){
            if ( msgQ_send(QUEUE_ID_worker, taskmsg) !=0 ){
                syslog(LM_RAND, LOG_ERROR, "\nFatal exception! msgQ_send failed.\n");
                loopcount = 0;  // force to skip the following while loop
                break;
            }
        }

    }
    minerMutex.unlock();
    //--- Tips for Above Code: if Miners already in START state, just update nTime, which can trigger miner to update to new block header also.

    uint64_t nTime0, nTime1;
    bool     found0=false, found1=false;
    uint64_t magicNumber;

    while (loopcount > 0){

        // use poll to get quit command from console
        {
            struct pollfd attention = { 0, POLLIN } ;
            int   x,y = 0;
            x = poll(&attention, 1, 1);             // 1ms
            if (x){
                y = getc(stdin);
            }
            if (x && ((y=='q') || (y=='Q'))){
                syslog(LM_RAND, LOG_VERBOSE, "Quit by Request.\n");
                break;
            }
        }

        // Check if there's any good news
        msg_t       msg;
        int status = msgQ_recv_nowait(QUEUE_ID_manager, msg);
        if (status == 0){
            if (msg_opcode(msg) == MSG_found_odd0)
            {
                nTime0 = msgS_data(msg);
                magicNumber = msgS_payload(msg);
                found0 = true;
            }
            else if (msg_opcode(msg) == MSG_found_odd1)
            {
                nTime1 = msgS_data(msg);
                magicNumber = msgS_payload(msg);
                found1 = true;
            }

        }
        else if ( status < 0 ){
            // for thread exit on msgQ exception
            syslog(LM_RAND, LOG_ERROR, "\nFatal exception! minermgr msgQ fail to read.\n");
            loopcount = 0;  // force to quit while loop for safety
            break;
        }

        // Statistics
        if (found0){
            found0 = false;
            totalfound0++;

            if ((nTime0 >> 32) > 0)
                interval = (uint32_t)0xffffffff;
            else
                interval = (uint32_t)nTime0;

            // Convert time unit. Split into 256 groups
            interval >>= (24-16);                   // 16 is the experimental value corresponding to ALLNODES. if change ALLNODES, remember to change this also !
            if (interval >= 255)
                interval = 255;

            if (intervalsets0.find (interval) == intervalsets0.end()){
                intervalsets0.insert(interval);
                intervaloccurence0[interval] = 1;    // 1st occurrence
            }
            else{
                intervaloccurence0[interval]++;      // occurrence accumulation
            }
            syslog(LM_RAND, LOG_VERBOSE, "magicNumber=%016x loopleft=%-6d nTime=0x%08x, randomGenerated=0x%016lx\n", (magicNumber), loopcount, nTime0, randomGenerated);
            loopcount--;
        }

        if (found1){
            found1 = false;
            totalfound1++;

            if ((nTime1 >> 32) > 0)
                interval = (uint32_t)0xffffffff;
            else
                interval = (uint32_t)nTime1;

            // Convert time unit. Split into 256 groups
            interval >>= (24-16);               // 16 is experimental value, same as above comments
            if (interval >= 255)
                interval = 255;

            if (intervalsets1.find (interval) == intervalsets1.end()){
                intervalsets1.insert(interval);
                intervaloccurence1[interval] = 1;    // 1st occurrence
            }
            else{
                intervaloccurence1[interval]++;      // occurrence accumulation
            }
            syslog(LM_RAND, LOG_VERBOSE, "magicNumber=%016lx loopleft=%-6d nTime=0x%08x, randomGenerated=0x%016lx\n", (magicNumber), loopcount, nTime1, (randomGenerated));
        }

    }
    gettimeofday(&tp, NULL);
    currMs = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    if (currMs == beginMs){
        currMs = beginMs + 1;   // to avoid dividing by zero
    }
    syslog(LM_RAND, LOG_VERBOSE, "\nsimulation: random generated speed = %d (M/s), total used time = %d(s)\n", (randomGenerated/(currMs-beginMs))>>10, (currMs-beginMs)/1000);
    syslog(LM_RAND, LOG_VERBOSE, "\nsimulation: found total 32bit0 leading: %d, total 32bit1 leading: %d\n", totalfound0, totalfound1);

    // miner threads safety close
    {
        // Quit all those miner threads to release the CPU power
        instructionShared = INS_rand_stop;
        msg_t quitcommand;
        msgS_allocate(quitcommand, MSG_worker_quit, 0, 0);
        for (int i=0; i<activethreads; i++)
        {
            if ( msgQ_send(QUEUE_ID_worker, quitcommand) !=0 )
                break;
        }
        usleep(1000 * 1000);    // wait for threads quit safely

        randworker_term();
    }

    std::ostringstream strResult;
    syslog(LM_RAND, LOG_VERBOSE, "\nInterval  0-Occur\n");
    for (const uint32_t& s : intervalsets0){
        syslog(LM_RAND, LOG_VERBOSE, "%4d   %4d\n", s, intervaloccurence0[s]);
    }
    syslog(LM_RAND, LOG_VERBOSE, "\nInterval  1-Occur\n");
    for (const uint32_t& s : intervalsets1){
        syslog(LM_RAND, LOG_VERBOSE, "%4d   %4d\n", s, intervaloccurence1[s]);
    }
    intervalsets0.clear();
    intervaloccurence0.clear();
    intervalsets1.clear();
    intervaloccurence1.clear();

    return 0;
}
