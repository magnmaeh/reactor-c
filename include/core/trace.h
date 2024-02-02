/**
 * @file
 * @author Edward A. Lee
 * @author Peter Donovan
 * @copyright (c) 2020-2024, The University of California at Berkeley.
 * License: <a href="https://github.com/lf-lang/reactor-c/blob/main/LICENSE.md">BSD 2-clause</a>
 * @brief Definitions of tracepoint functions for use with the C code generator and any other
 * code generator that uses the C infrastructure (such as the Python code generator).
 *
 * See: https://www.lf-lang.org/docs/handbook/tracing?target=c
 *
* The trace file is named trace.lft and is a binary file with the following format:
 *
 * Header:
 * * instant_t: The start time. This is both the starting physical time and the starting logical time.
 * * int: Size N of the table mapping pointers to descriptions.
 * This is followed by N records each of which has:
 * * A pointer value (the key).
 * * A null-terminated string (the description).
 *
 * Traces:
 * A sequence of traces, each of which begins with an int giving the length of the trace
 * followed by binary representations of the trace_record struct written using fwrite().
 */

#ifdef RTI_TRACE
#define LF_TRACE
#endif

#ifndef TRACE_H
#define TRACE_H

#include "lf_types.h"
#include <stdio.h>

#ifdef FEDERATED
#include "net_common.h"
#endif // FEDERATED

/**
 * Trace event types. If you update this, be sure to update the
 * string representation below. Also, create a tracepoint function
 * for each event type.
 */
typedef enum {
    reaction_starts,
    reaction_ends,
    reaction_deadline_missed,
    schedule_called,
    user_event,
    user_value,
    worker_wait_starts,
    worker_wait_ends,
    scheduler_advancing_time_starts,
    scheduler_advancing_time_ends,
    federated, // Everything below this is for tracing federated interactions.
    // Sending messages
    send_ACK,
    send_FAILED,
    send_TIMESTAMP,
    send_NET,
    send_LTC,
    send_STOP_REQ,
    send_STOP_REQ_REP,
    send_STOP_GRN,
    send_FED_ID,
    send_PTAG,
    send_TAG,
    send_REJECT,
    send_RESIGN,
    send_PORT_ABS,
    send_CLOSE_RQ,
    send_TAGGED_MSG,
    send_P2P_TAGGED_MSG,
    send_MSG,
    send_P2P_MSG,
    send_ADR_AD,
    send_ADR_QR,
    // Receiving messages
    receive_ACK,
    receive_FAILED,
    receive_TIMESTAMP,
    receive_NET,
    receive_LTC,
    receive_STOP_REQ,
    receive_STOP_REQ_REP,
    receive_STOP_GRN,
    receive_FED_ID,
    receive_PTAG,
    receive_TAG,
    receive_REJECT,
    receive_RESIGN,
    receive_PORT_ABS,
    receive_CLOSE_RQ,
    receive_TAGGED_MSG,
    receive_P2P_TAGGED_MSG,
    receive_MSG,
    receive_P2P_MSG,
    receive_ADR_AD,
    receive_ADR_QR,
    receive_UNIDENTIFIED,
    NUM_EVENT_TYPES
} trace_event_t;

#ifdef LF_TRACE

#include "plugin-apis/trace-abi.h"

/**
 * String description of event types.
 */
static const char *trace_event_names[] = {
    "Reaction starts",
    "Reaction ends",
    "Reaction deadline missed",
    "Schedule called",
    "User-defined event",
    "User-defined valued event",
    "Worker wait starts",
    "Worker wait ends",
    "Scheduler advancing time starts",
    "Scheduler advancing time ends",
    "Federated marker",
    // Sending messages
    "Sending ACK",
    "Sending FAILED",
    "Sending TIMESTAMP",
    "Sending NET",
    "Sending LTC",
    "Sending STOP_REQ",
    "Sending STOP_REQ_REP",
    "Sending STOP_GRN",
    "Sending FED_ID",
    "Sending PTAG",
    "Sending TAG",
    "Sending REJECT",
    "Sending RESIGN",
    "Sending PORT_ABS",
    "Sending CLOSE_RQ",
    "Sending TAGGED_MSG",
    "Sending P2P_TAGGED_MSG",
    "Sending MSG",
    "Sending P2P_MSG",
    "Sending ADR_AD",
    "Sending ADR_QR",
    // Receiving messages
    "Receiving ACK",
    "Receiving FAILED",
    "Receiving TIMESTAMP",
    "Receiving NET",
    "Receiving LTC",
    "Receiving STOP_REQ",
    "Receiving STOP_REQ_REP",
    "Receiving STOP_GRN",
    "Receiving FED_ID",
    "Receiving PTAG",
    "Receiving TAG",
    "Receiving REJECT",
    "Receiving RESIGN",
    "Receiving PORT_ABS",
    "Receiving CLOSE_RQ",
    "Receiving TAGGED_MSG",
    "Receiving P2P_TAGGED_MSG",
    "Receiving MSG",
    "Receiving P2P_MSG",
    "Receiving ADR_AD",
    "Receiving ADR_QR",
    "Receiving UNIDENTIFIED",
};

/**
 * @brief A trace record that gets written in binary to the trace file in the default implementation.
 */
typedef struct trace_record_t {
    trace_event_t event_type;
    void* pointer;  // pointer identifying the record, e.g. to self struct for a reactor.
    int src_id;     // The ID number of the source (e.g. worker or federate) or -1 for no ID number.
    int dst_id;     // The ID number of the destination (e.g. reaction or federate) or -1 for no ID number.
    instant_t logical_time;
    microstep_t microstep;
    instant_t physical_time;
    trigger_t* trigger;
    interval_t extra_delay;
} trace_record_t;

// void lf_tracing_init(int process_id, int max_num_local_threads);

// void lf_tracing_global_shutdown();

void call_tracepoint(
        // trace_t* trace,
        int event_type,
        void* reactor,
        tag_t tag,
        int worker,
        int src_id,
        int dst_id,
        instant_t* physical_time,
        trigger_t* trigger,
        interval_t extra_delay,
        bool is_interval_start
);

// /**
//  * @brief Dynamically allocate a new tracing object.
//  *
//  * @param env The environment in which we are tracing. If passed NULL we use the GLOBAL_ENVIRONMENT
//  * @param filename Name of the file in which to store the trace
//  * @return trace_t* A newly allocated trace object with environment pointer and filename initialized
//  */
// trace_t* trace_new(environment_t *env, const char *filename);

// /**
//  * @brief Free the memory allocated for the trace object
//  *
//  * @param trace
//  */
// void trace_free(trace_t *trace);

/**
 * Register a trace object.
 * @param env Pointer to the environment in which the event is traced
 * @param pointer1 Pointer that identifies the object, typically to a reactor self struct.
 * @param pointer2 Further identifying pointer, typically to a trigger (action or timer) or NULL if irrelevant.
 * @param type The type of trace object.
 * @param description The human-readable description of the object.
 * @return 1 if successful, 0 if the trace object table is full.
 */
int _lf_register_trace_event(void* pointer1, void* pointer2, _lf_trace_object_t type, char* description);

/**
 * Register a user trace event. This should be called once, providing a pointer to a string
 * that describes a phenomenon being traced. Use the same pointer as the first argument to
 * tracepoint_user_event() and tracepoint_user_value().
 * @param description Pointer to a human-readable description of the event.
 * @return 1 if successful, 0 if the trace object table is full.
 */
int register_user_trace_event(void* self, char* description);

// /**
//  * Open a trace file and start tracing.
//  * @param filename The filename for the trace file.
//  */
// void start_trace(trace_t* trace);

/**
 * Trace the start of a reaction execution.
 * @param env The environment in which we are executing
 * @param reaction Pointer to the reaction_t struct for the reaction.
 * @param worker The thread number of the worker thread or 0 for single-threaded execution.
 */
#define tracepoint_reaction_starts(env, reaction, worker) \
    call_tracepoint(reaction_starts, reaction->self, env->current_tag, worker, worker, reaction->number, NULL, NULL, 0, true)

/**
 * Trace the end of a reaction execution.
 * @param env The environment in which we are executing
 * @param reaction Pointer to the reaction_t struct for the reaction.
 * @param worker The thread number of the worker thread or 0 for single-threaded execution.
 */
#define tracepoint_reaction_ends(env, reaction, worker) \
    call_tracepoint(reaction_ends, reaction->self, env->current_tag, worker, worker, reaction->number, NULL, NULL, 0, false)

/**
 * Trace a call to schedule.
 * @param env The environment in which we are executing
 * @param trigger Pointer to the trigger_t struct for the trigger.
 * @param extra_delay The extra delay passed to schedule().
 */
void tracepoint_schedule(environment_t* env, trigger_t* trigger, interval_t extra_delay);

/**
 * Trace a user-defined event. Before calling this, you must call
 * register_user_trace_event() with a pointer to the same string
 * or else the event will not be recognized.
 * @param self Pointer to the self struct of the reactor from which we want
 * to trace this event. This pointer is used to get the correct environment and 
 * thus the correct logical tag of the event.
 * @param description Pointer to the description string.
 */
void tracepoint_user_event(void* self, char* description);

/**
 * Trace a user-defined event with a value.
 * Before calling this, you must call
 * register_user_trace_event() with a pointer to the same string
 * or else the event will not be recognized.
 * @param self Pointer to the self struct of the reactor from which we want
 * to trace this event. This pointer is used to get the correct environment and 
 * thus the correct logical tag of the event.
 * @param description Pointer to the description string.
 * @param value The value of the event. This is a long long for
 *  convenience so that time values can be passed unchanged.
 *  But int values work as well.
 */
void tracepoint_user_value(void* self, char* description, long long value);

/**
 * Trace the start of a worker waiting for something to change on the reaction queue.
 * @param trace The trace object.
 * @param worker The thread number of the worker thread or 0 for single-threaded execution.
 */
#define tracepoint_worker_wait_starts(env, worker) \
    call_tracepoint(worker_wait_starts, NULL, env->current_tag, worker, worker, -1, NULL, NULL, 0, true)

/**
 * Trace the end of a worker waiting for something to change on the event or reaction queue.
 * @param trace The trace object.
 * @param worker The thread number of the worker thread or 0 for single-threaded execution.
 */
#define tracepoint_worker_wait_ends(env, worker) \
    call_tracepoint(worker_wait_ends, NULL, env->current_tag, worker, worker, -1, NULL, NULL, 0, false)

/**
 * Trace the start of the scheduler waiting for logical time to advance or an event to
 * appear on the event queue.
 * @param trace The trace object.
 */
#define tracepoint_scheduler_advancing_time_starts(env) \
    call_tracepoint(scheduler_advancing_time_starts, NULL, env->current_tag, -1, -1, -1, NULL, NULL, 0, true);

/**
 * Trace the end of the scheduler waiting for logical time to advance or an event to
 * appear on the event queue.
 * @param trace The trace object.
 */
#define tracepoint_scheduler_advancing_time_ends(env) \
    call_tracepoint(scheduler_advancing_time_ends, NULL, env->current_tag, -1, -1, -1, NULL, NULL, 0, false)

/**
 * Trace the occurrence of a deadline miss.
 * @param trace The trace object.
 * @param reaction Pointer to the reaction_t struct for the reaction.
 * @param worker The thread number of the worker thread or 0 for single-threaded execution.
 */
#define tracepoint_reaction_deadline_missed(env, reaction, worker) \
    call_tracepoint(reaction_deadline_missed, reaction->self, env->current_tag, worker, worker, reaction->number, NULL, NULL, 0, false)

// /**
//  * Flush any buffered trace records to the trace file and close the files.
//  * @param trace The trace object.
//  */
// void stop_trace(trace_t* trace);

// /**
//  * Version of stop_trace() that does not lock the trace mutex.
//  */
// void stop_trace_locked(trace_t* trace);

////////////////////////////////////////////////////////////
//// For federated execution

#if defined(FEDERATED) || defined(LF_ENCLAVES)

/**
 * Trace federate sending a message to the RTI.
 * @param event_type The type of event. Possible values are:
 * 
 * @param fed_id The federate identifier.
 * @param tag Pointer to the tag that has been sent, or NULL.
 */
void tracepoint_federate_to_rti(trace_event_t event_type, int fed_id, tag_t* tag);

/**
 * Trace federate receiving a message from the RTI.
 * @param event_type The type of event. Possible values are:
 * 
 * @param fed_id The federate identifier.
 * @param tag Pointer to the tag that has been received, or NULL.
 */
void tracepoint_federate_from_rti(trace_event_t event_type, int fed_id, tag_t* tag);

/**
 * Trace federate sending a message to another federate.
 * @param event_type The type of event. Possible values are:
 *
 * @param fed_id The federate identifier.
 * @param partner_id The partner federate identifier.
 * @param tag Pointer to the tag that has been sent, or NULL.
 */
void tracepoint_federate_to_federate(trace_event_t event_type, int fed_id, int partner_id, tag_t *tag);

/**
 * Trace federate receiving a message from another federate.
 * @param event_type The type of event. Possible values are:
 *
 * @param fed_id The federate identifier.
 * @param partner_id The partner federate identifier.
 * @param tag Pointer to the tag that has been received, or NULL.
 */
void tracepoint_federate_from_federate(trace_event_t event_type, int fed_id, int partner_id, tag_t *tag);

#else
#define tracepoint_federate_to_rti(...);
#define tracepoint_federate_from_rti(...);
#define tracepoint_federate_to_federate(...);
#define tracepoint_federate_from_federate(...);
#endif // FEDERATED

////////////////////////////////////////////////////////////
//// For RTI execution

#ifdef RTI_TRACE

/**
 * Trace RTI sending a message to a federate.
 * @param event_type The type of event. Possible values are:
 *
 * @param fed_id The fedaerate ID.
 * @param tag Pointer to the tag that has been sent, or NULL.
 */
void tracepoint_rti_to_federate(trace_event_t event_type, int fed_id, tag_t* tag);

/**
 * Trace RTI receiving a message from a federate.
 * @param event_type The type of event. Possible values are:
 * 
 * @param fed_id The fedaerate ID.
 * @param tag Pointer to the tag that has been sent, or NULL.
 */
void tracepoint_rti_from_federate(trace_event_t event_type, int fed_id, tag_t* tag);

#else
#define tracepoint_rti_to_federate(...);
#define tracepoint_rti_from_federate(...) ;
#endif // RTI_TRACE

#else
typedef struct trace_t trace_t;

// empty definition in case we compile without tracing
#define _lf_register_trace_event(...) 1
#define register_user_trace_event(...) 1
#define tracepoint_reaction_starts(...)
#define tracepoint_reaction_ends(...)
#define tracepoint_schedule(...)
#define tracepoint_user_event(...)
#define tracepoint_user_value(...)
#define tracepoint_worker_wait_starts(...)
#define tracepoint_worker_wait_ends(...)
#define tracepoint_scheduler_advancing_time_starts(...);
#define tracepoint_scheduler_advancing_time_ends(...);
#define tracepoint_reaction_deadline_missed(...);
#define tracepoint_federate_to_rti(...);
#define tracepoint_federate_from_rti(...);
#define tracepoint_federate_to_federate(...) ;
#define tracepoint_federate_from_federate(...) ;
#define tracepoint_rti_to_federate(...);
#define tracepoint_rti_from_federate(...) ;

// #define start_trace(...)
// #define stop_trace(...)
// #define stop_trace_locked(...)
// #define trace_new(...) NULL
// #define trace_free(...)
#define lf_tracing_register_trace_event(...);
#define lf_tracing_set_start_time(...);
#define tracepoint(...);
#define lf_tracing_global_init(...);
#define lf_tracing_global_shutdown(...);


#endif // LF_TRACE
#endif // TRACE_H
