// Copyright (c) 2017 Gary Yu
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*------------------------------------------------------------------
 * System includes
 *------------------------------------------------------------------*/
#   include <sys/types.h>
#   include <sys/ipc.h>
#   include <sys/msg.h>
#   include <sys/errno.h>
#   include <time.h>
#   include <stdlib.h>

#   include <unistd.h>
#   include <pthread.h>
#   include <semaphore.h>

#   include <stdio.h>
#   include <string.h>
#   include <stdarg.h>
#	include <errno.h>

/*------------------------------------------------------------------
 * Module includes
 *------------------------------------------------------------------*/

#include "os_wrapper.h"

/*-------------------------------------------------------------------
 Threads
 -------------------------------------------------------------------*/

/*!
 * \def OSTYPE_THREAD
 *    Define pthread_t as OSTYPE_THREAD.
 */
#define OSTYPE_THREAD  pthread_t

/*!
 *  \brief This is a thread structure.
 */
typedef struct {
	const char *name;   //!< thread name
	int priority;       //!< thread priority : 99 <-> highest, 1 <-> lowest in Linux

	OSTYPE_THREAD id;   //!< thread id
	thread_entry_t *entryPoint; //!< thread entry point function

} map_thread_t;

/*! 
 * \brief A global variable to save the list of all threads name and priority
 */
static map_thread_t thread_array[THREAD_ID_MAX] = {
        {"worker1",  0, UNDEF, NULL},
        {"worker2",  0, UNDEF, NULL},
        {"worker3",  0, UNDEF, NULL},
        {"worker4",  0, UNDEF, NULL},
        {"worker5",  0, UNDEF, NULL},
        {"worker6",  0, UNDEF, NULL},
        {"worker7",  0, UNDEF, NULL},
        {"worker8",  0, UNDEF, NULL}
};

/*------------------------------------------------------------------
 Queues
 ------------------------------------------------------------------*/

/*!
 * \def OSTYPE_MSGQ
 *  Define int as OSTYPE_MSGQ
 */
#define OSTYPE_MSGQ  int

/*!
 *  \brief This is a queue structure.
 */
typedef struct {
	const char *name;   //!< Name of Queue
	OSTYPE_MSGQ id;     //!< Queue id
} map_queue_t;

/*! 
 * \brief A global value to save system message Queue ID
 */
OSTYPE_MSGQ msgQ_id = UNDEF;

/*! 
 * \brief A global variable to save the list of message Queue name and ID
 */
static map_queue_t msgQ_array[QUEUE_ID_MAX] = {
        {"dummy",   QUEUE_ID_dummy}     ,
        {"worker",  QUEUE_ID_worker}    ,
        {"manager", QUEUE_ID_manager}   ,
};

/*------------------------------------------------------------------
 * Module Internal functions Definitions
 *------------------------------------------------------------------*/

/*! 
 * \brief Set Thread Priority
 * 
 * \param filename  : source file name in which this function is called
 * \param linenum   : source file line number in which this function is called
 * \param pattr     : thread attribute
 * \param priority  : thread priority : 99 <-> highest, 1 <-> lowest in Linux
 * 
 * \return int
 *          - positive  : successful
 *          - others    : failure
 */
static int set_thread_priority(const char *filename, int linenum,
		pthread_attr_t *pattr, int priority) {
	struct sched_param sched;
	int rs;
	int newpriority = 0;
	//int oldpriority;

	rs = pthread_attr_getschedparam(pattr, &sched);
	if (rs != 0) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"pthread_attr_getschedparam failed");
		return -1;
	}

	//oldpriority = sched.__sched_priority;
	sched.sched_priority = priority;

	rs = pthread_attr_setschedparam(pattr, &sched);
	if (rs != 0) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"pthread_attr_setschedparam failedv");
		return -2;
	}

	rs = pthread_attr_getschedparam(pattr, &sched);
	if (rs != 0) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"pthread_attr_getschedparam failed");
		return -3;
	}
	newpriority = sched.sched_priority;

	return newpriority;
}

/*------------------------------------------------------------------
 * Module External functions Definitions
 *------------------------------------------------------------------*/

/*! 
 * \brief OS Wrapper Layer Initialzation
 * Create one general system message queue and enable thread cancelable
 * 
 * \return int
 *          - 0     : successful
 *          - others: failure
 */
int oswrapper_init(void) {
	/* message queue creation */

	int msgQid;

	msgQid = msgget(IPC_PRIVATE, (IPC_CREAT | IPC_EXCL | 0666));
	if (msgQid < 0) {
		syslog(LM_OSWRAP, LOG_FATAL, "Message Queue creation failed\n");
		return -1;
	}

	/* Save it into global variable */
	msgQ_id = msgQid;

	/* thread attribute */

	int state, oldstate;

	state = PTHREAD_CANCEL_ENABLE;
	if (0 != pthread_setcancelstate(state, &oldstate)) {
		syslog(LM_OSWRAP, LOG_ERROR, "thread setcancelstate failed\n");
	}

	return 0;
}

/*! 
 * \brief OS Wrapper Layer Termination
 * The general system message queue will be deleted.
 */
void oswrapper_term(void) {
	/* message queue deletion */
	if (UNDEF != msgQ_id) {
		if (0 != msgctl(msgQ_id, IPC_RMID, NULL)) {
			syslog(LM_OSWRAP, LOG_ERROR, "message queue delete failed");
		}
		msgQ_id = UNDEF;
	}

	return;
}

/*! 
 * \brief initialize and activate a thread
 * 
 * \param filename  : source file name in which this function is called
 * \param linenum   : source file line number in which this function is called
 * \param id        : Thread ID
 * \param entry     : Thread Entry Point Function
 * 
 * \return int
 *          - 0     : successful
 *          - others: failure
 */
int _thread_create(const char *filename, int linenum, thread_id_t id,
		thread_entry_t entry) {
	int r;
	pthread_attr_t attr;
	int rs;

	rs = pthread_attr_init(&attr);
	if (rs != 0) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"pthread_attr_init failed");
		return -1;
	}

	if (thread_array[id].priority > 0) {
		set_thread_priority(filename, linenum, &attr,
				thread_array[id].priority);
	}

	r = pthread_create((pthread_t *) &thread_array[id].id, &attr,
			(void *(*)(void *))entry, NULL);
	if (r != 0) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"thread creation failed");
	} else {
		thread_array[id].entryPoint = entry;
	}

	rs = pthread_attr_destroy(&attr);
	if (rs != 0) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"pthread attribute destroy failed");
	}

	return 0;
}

/*! 
 * \brief delete a previously created thread
 * 
 * \param filename  : source file name in which this function is called
 * \param linenum   : source file line number in which this function is called
 * \param id        : Thread ID
 * 
 * \return int
 *          - 0 : successful
 *          - others : failure
 */
int _thread_delete(const char *filename, int linenum, thread_id_t id) {
	int ret = 0;
	pthread_t tid;

	if (id < THREAD_ID_MAX) {
		tid = thread_array[id].id;
		if (tid == UNDEF) {
			ret = -1;
		} else {
			if (0 != pthread_cancel(tid)) {
				__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
						"thread terminate failed\n");
				ret = -2;
			}
			thread_array[id].id = UNDEF;
		}
	}

	return ret;
}

const char* get_thread_name(thread_id_t id) {
    static const char unknown_thread[] = "unkown";
    if ((id >=0 ) && (id<THREAD_ID_MAX)){
        return thread_array[id].name;
    }
    else{
        return unknown_thread;
    }
}

/*! 
 * \brief send message to message queue
 * 
 * \param filename  : source file name in which this function is called
 * \param linenum   : source file line number in which this function is called
 * \param id        : User Message Queue ID
 * \param msg       : A primitive message
 * 
 * \return int
 *          - 0       : Success
 *          - others  : Failure
 */
int _msgQ_send(const char *filename, int linenum, msgQ_id_t id, msg_t msg) {
	struct {
		long mtype; /* message type */
		msg_t mtext; /* message data */
	} themsg;

	long msqid;

	if ((id < 0) || (id >= QUEUE_ID_MAX)) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"Queue ID out of range");
		return -1;
	}
	msqid = msgQ_array[id].id;

	themsg.mtype = msqid;
	themsg.mtext = msg;

	if (0 != msgsnd(msgQ_id, (struct msgbuf *) &themsg, sizeof(themsg)-sizeof(long),
					IPC_NOWAIT)) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"Message send failure. %s\n",strerror(errno));
		return -2;
	}

	return 0;
}

/*! 
 * \brief receive message from message queue
 * 
 * \param filename  : source file name in which this function is called
 * \param linenum   : source file line number in which this function is called
 * \param id        : User Message Queue ID
 * \param msg       : A primitive message
 * \param nowait    : true if don't want wait
 * 
 * \return int
 *          - 0       : Success
 *          - others  : Failure
 */
int _msgQ_recv(const char *filename, int linenum, msgQ_id_t id, msg_t *ptr_msg,
		bool nowait) {
	int msqid;
	int msgflg;
	int retVal;
	struct {
		long mtype; /* message type */
		msg_t mtext; /* message data */
	} themsg;

	if ((id < 0) || (id >= QUEUE_ID_MAX)) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"Queue ID out of range");
		return -1;
	}

	if (nowait) {
		msgflg = IPC_NOWAIT;
	} else {
		msgflg = 0;
	}
	msqid = msgQ_array[id].id;
	retVal = msgrcv(msgQ_id, (struct msgbuf *) &themsg, sizeof(themsg)-sizeof(long), msqid,
			msgflg);
	if ((-1 == retVal) && (errno != ENOMSG)) {
		__syslog(filename, linenum, LM_OSWRAP, LOG_ERROR,
				"Message receive failure\n");
		return -2;
	}

	if (retVal > 0) {
		*ptr_msg = themsg.mtext;
	}

	if (nowait) {
		if (retVal > 0)
			return 0;	// 0: success! got new message
		else
			return 1;	// 1: no new message
	} else {
		return 0; 	    // 0: success! got new message
	}
}

void __syslog(const char* full_name, int line_num, unsigned int flux,
		unsigned int grav, const char *format, ...) {
	va_list args;

	/*--- please use any available syslog module to replace this printf logs ---*/

	//--- the simple printf logs ---

//	if ((full_name != NULL) && (line_num != 0))
//		printf("%-12s:%4d ", full_name, line_num);
//	else if (full_name != NULL)
//		printf("%-12s ", full_name);

	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	return;
}

