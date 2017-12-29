// Copyright (c) 2017 Gary Yu
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef _OS_WRAPPER_H_
#define _OS_WRAPPER_H_

#ifndef FALSE
#define FALSE   false
#endif

#ifndef TRUE
#define TRUE    true
#endif

/*!
 * \def UNDEF
 *    Define an unvalid value for thread and message queue.
 */
#define UNDEF        0

/*------------------------------------------------------------------
 * Module Macro and Type definitions
 * Module Variables Definitions
 *------------------------------------------------------------------*/

typedef enum
{
    MSG_BASE               	      = 0x4000,
       MSG_worker_start			  ,
       MSG_found_odd0			  ,
       MSG_found_odd1             ,
       MSG_worker_quit			  ,

    MSG_BASE_MAX                  = 0xffff

} msg_opcode_t;


/*!
 *  \brief This is a primitive structure.
 *
 * primitive structure
 *   1) primitive code
 *   2) short message without payload
 *   3) short messsage with payload
 *
 */
typedef struct
{
    msg_opcode_t primitiveCode;     //!< Message Opcode
    uint64_t     shortmsg;  	 	    //!< Short message
    uint64_t     payload;           //!< Short message payload
    
} S_PRIMITIVE;

/*-------------------------------------------------------------------
  Threads
  -------------------------------------------------------------------*/

/*!
 * \enum thread_id_t
 *      The Thread IDs
 */
typedef enum {
    THREAD_ID_worker1,    /*!< Worker Thread           */      \
    THREAD_ID_worker2,    /*!< Worker Thread           */      \
    THREAD_ID_worker3,    /*!< Worker Thread           */      \
    THREAD_ID_worker4,    /*!< Worker Thread           */      \
    THREAD_ID_worker5,    /*!< Worker Thread           */      \
    THREAD_ID_worker6,    /*!< Worker Thread           */      \
    THREAD_ID_worker7,    /*!< Worker Thread           */      \
    THREAD_ID_worker8,    /*!< Worker Thread           */      \
    THREAD_ID_MAX		 //!< Thread Number
} thread_id_t;

/*!
 * Creates a type name for thread entry point function type
 */
typedef void thread_entry_t(void);

/*------------------------------------------------------------------
  Queues
  ------------------------------------------------------------------*/

/*!
 * \enum msgQ_id_t
 * The User Message Queue IDs
 */
typedef enum {
    QUEUE_ID_dummy,     /*!< dummy id, don't remove it  */
    QUEUE_ID_worker,    /*!< worker message queue */
    QUEUE_ID_manager,   /*!< manager message queue */
    QUEUE_ID_MAX
} msgQ_id_t;

/*! Creates a type msg_t name for S_PRIMITIVE */ 
typedef S_PRIMITIVE msg_t;

/*------------------------------------------------------------------
 * Module External functions Declaration
 *------------------------------------------------------------------*/

int     oswrapper_init  (void);
void    oswrapper_term  (void);

/*------------------------------------------------------------------
 * Thread
 *------------------------------------------------------------------*/

/*!
 * \def thread_create(id,entry)
 *  Create thread with \a id and function \a entry.
 */
#define thread_create(id,entry) _thread_create(__FILE__, __LINE__, id, entry)

/*!
 * \def thread_delete(id)
 *  Delete thread which has \a id.
 */
#define thread_delete(id)       _thread_delete(__FILE__, __LINE__, id)

 int _thread_create(const char *filename, int linenum,
         thread_id_t id, thread_entry_t entry);

 int _thread_delete(const char *filename, int linenum,
         thread_id_t id);

 const char* get_thread_name(thread_id_t id);

/*------------------------------------------------------------------
 * Message Queue
 *------------------------------------------------------------------*/

/*!
 * \def msgQ_send(id, msg)
 *  Send a message into a message queue.
 */
#define msgQ_send(id, msg)    _msgQ_send(__FILE__, __LINE__, id, msg)

/*!
 * \def msgQ_recv(id, msg)
 *  Receive a message from a message queue.
 */
#define msgQ_recv(id, msg)    	     _msgQ_recv(__FILE__, __LINE__, id, &msg, FALSE)
#define msgQ_recv_nowait(id, msg)    _msgQ_recv(__FILE__, __LINE__, id, &msg, TRUE )

 int _msgQ_send(const char *filename, int linenum,
   msgQ_id_t            id,
   msg_t                msg
);

 int _msgQ_recv(const char *filename, int linenum,
   msgQ_id_t            id,
   msg_t               *ptr_msg,
   bool		            nowait
);


/*------------------------------------------------------------------
 * Syslog 
 *------------------------------------------------------------------*/

/*!
  \enum log_module_t
  The Log Source Module IDs
 */
typedef enum {
    LM_OSWRAP = 0,   //!< OS Wrapper Log
    LM_RAND   = 1,   //!< Random Generation Log
    LM_DEBUG  = 2,   //!< debug() Log
    LM_SIZE
} logmodule_id_t;

/*!
  \enum log_grav_t
  The Log Severity Levels
 */
typedef enum {
    LOG_NEVER		=99,	//!< no any log output
    LOG_FATAL		=70,	//!< fatal exception
    LOG_ERROR		=50,	//!< errors and critical errors only
    LOG_WARNING		=40,	//!< abnormal situation no service
    LOG_NOTICE		=30,	//!< level for release version
    LOG_VERBOSE		=10,	//!< level used in development version
    LOG_DEBUG		=0,	//!< level for debug
} log_level_id_t;

#define log_grav_t      log_level_id_t


/*! 
 * \brief syslog wrapper function.
 *       outputs the specified string to the specified destination flow.
 *       - if enabled, a timestamp is added
 *       - the output buffer length is limited to SYSLOG_OUTPUT_BUFFER_LEN
 * 
 * \param full_name : source file name in which this function is called
 * \param line_num  : source file line number in which this function is called
 * \param flux      : syslog source module
 * \param grav      : severity level of log
 * \param format    : output format
 */

 void __syslog(const char* full_name, int line_num,
             unsigned int flux, unsigned int grav, const char *format, ...);

#define SRCFILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define syslog(...)  	__syslog(SRCFILE, __LINE__, ##__VA_ARGS__)



/*------------------------------------------------------------------
 * Macro for Misc 
 *
 *------------------------------------------------------------------*/

#define msg_opcode(msg)		(msg.primitiveCode)
#define msgS_data(m)    		((m).shortmsg)
#define msgS_payload(m)      ((m).payload)

#define msgS_allocate(msg, opcode, short_msg, short_payload)	\
{										\
	msg.primitiveCode = opcode;			\
	msg.shortmsg = short_msg;           \
    msg.payload = short_payload;        \
}

#define msgQ_create(queue_id)

#endif//_OS_WRAPPER_H_


