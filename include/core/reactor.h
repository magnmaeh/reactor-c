/**
 * @file
 * @author Edward A. Lee (eal@berkeley.edu)
 * @author Marten Lohstroh (marten@berkeley.edu)
 * @author Chris Gill (cdgill@wustl.edu)
 * @author Mehrdad Niknami (mniknami@berkeley.edu)
 *
 * @section LICENSE
 * Copyright (c) 2019, The University of California at Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 *
 * Header file for the infrastructure for the C target of Lingua Franca.
 * This file contains header information used by both the threaded and
 * non-threaded versions of the C runtime.
 *
 * This header file defines the functions and macros that programmers use
 * in the body of reactions for reading and writing inputs and outputs and
 * scheduling future events. The LF compiler does not parse that C code.
 * This fact strongly affects the design.
 *
 * The intent of the C target for Lingua Franca not to provide a safe
 * programming environment (The C++ and TypeScript targets are better
 * choices for that), but rather to find the lowest possible overhead
 * implementation of Lingua Franca. The API herein can easily be misused,
 * leading to memory leaks, nondeterminism, or program crashes.
 */

#ifndef REACTOR_H
#define REACTOR_H

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lf_types.h"
#include "modes.h" // Modal model support
#include "platform.h"  // Platform-specific times and APIs
#include "port.h"
#include "pqueue.h"
#include "tag.h"       // Time-related functions.
#include "trace.h"
#include "util.h"

//  ======== Macros ========  //
#define CONSTRUCTOR(classname) (new_ ## classname)
#define SELF_STRUCT_T(classname) (classname ## _self_t)

/**
 * Prototype for the internal API. @see reactor_common.c
 **/
lf_token_t* _lf_initialize_token_with_value(lf_token_t* token, void* value, size_t length);

////////////////////////////////////////////////////////////
//// Macros for producing outputs.

// NOTE: According to the "Swallowing the Semicolon" section on this page:
//    https://gcc.gnu.org/onlinedocs/gcc-3.0.1/cpp_3.html
// the following macros should use an odd do-while construct to avoid
// problems with if ... else statements that do not use braces around the
// two branches.

void _lf_set_present(lf_port_base_t* port);

/**
 * Set the specified output (or input of a contained reactor)
 * to the specified value.
 *
 * This version is used for primitive types such as int,
 * double, etc. as well as the built-in types bool and string.
 * The value is copied and therefore the variable carrying the
 * value can be subsequently modified without changing the output.
 * This can also be used for structs with a type defined by a typedef
 * so that the type designating string does not end in '*'.
 * @param out The output port (by name) or input of a contained
 *  reactor in form input_name.port_name.
 * @param value The value to insert into the self struct.
 */
#define _LF_SET(out, val) \
do { \
    /* We need to assign "val" to "out->value" since we need to give "val" an address */ \
    /* even if it is a literal */ \
    out->value = val; \
    _lf_set_present((lf_port_base_t*)out); \
    if (out->token != NULL) { \
        /* The cast "*((void**) &out->value)" is a hack to make the code */ \
        /* compile with non-token types where val is not a pointer. */ \
        lf_token_t* token = _lf_initialize_token_with_value(out->token, *((void**) &out->value), 1); \
        token->ref_count = out->num_destinations; \
        out->token = token; \
        out->token->ok_to_free = token_and_value; \
        if (out->destructor != NULL) { \
            out->token->destructor = out->destructor; \
        } \
        if (out->copy_constructor != NULL) { \
            out->token->copy_constructor = out->copy_constructor; \
        } \
    } \
} while(0)

/**
 * Version of set for output types given as 'type[]' where you
 * want to send a previously dynamically allocated array.
 *
 * The deallocation is delegated to downstream reactors, which
 * automatically deallocate when the reference count drops to zero.
 * It also sets the corresponding _is_present variable in the self
 * struct to true (which causes the object message to be sent).
 * @param out The output port (by name).
 * @param val The array to send (a pointer to the first element).
 * @param length The length of the array to send.
 * @see lf_token_t
 */
#ifndef __cplusplus
#define _LF_SET_ARRAY(out, val, length) \
do { \
    _lf_set_present((lf_port_base_t*)out); \
    lf_token_t* token = _lf_initialize_token_with_value(out->token, val, length); \
    token->ref_count = out->num_destinations; \
    out->token = token; \
    out->value = token->value; \
} while(0)
#else
#define _LF_SET_ARRAY(out, val, length) \
do { \
    _lf_set_present((lf_port_base_t*)out); \
    lf_token_t* token = _lf_initialize_token_with_value(out->token, val, length); \
    token->ref_count = out->num_destinations; \
    out->token = token; \
    out->value = static_cast<decltype(out->value)>(token->value); \
} while(0)
#endif

/**
 * Version of set() for output types given as 'type*' that
 * allocates a new object of the type of the specified output port.
 *
 * This macro dynamically allocates enough memory to contain one
 * instance of the output datatype and sets the variable named
 * by the argument to point to the newly allocated memory.
 * The user code can then populate it with whatever value it
 * wishes to send.
 *
 * This macro also sets the corresponding _is_present variable in the self
 * struct to true (which causes the object message to be sent),
 * @param out The output port (by name).
 */
#ifndef __cplusplus
#define _LF_SET_NEW(out) \
do { \
    _lf_set_present((lf_port_base_t*)out); \
    lf_token_t* token = _lf_set_new_array_impl(out->token, 1, out->num_destinations); \
    out->value = token->value; \
    out->token = token; \
} while(0)
#else
#define _LF_SET_NEW(out) \
do { \
    _lf_set_present((lf_port_base_t*)out); \
    lf_token_t* token = _lf_set_new_array_impl(out->token, 1, out->num_destinations); \
    out->value = static_cast<decltype(out->value)>(token->value); \
    out->token = token; \
} while(0)
#endif // __cplusplus

/**
 * Version of set() for output types given as 'type[]'.
 *
 * This allocates a new array of the specified length,
 * sets the corresponding _is_present variable in the self struct to true
 * (which causes the array message to be sent), and sets the variable
 * given by the first argument to point to the new array so that the
 * user code can populate the array. The freeing of the dynamically
 * allocated array will be handled automatically
 * when the last downstream reader of the message has finished.
 * @param out The output port (by name).
 * @param length The length of the array to be sent.
 */
#ifndef __cplusplus
#define _LF_SET_NEW_ARRAY(out, len) \
do { \
    _lf_set_present((lf_port_base_t*)out); \
    lf_token_t* token = _lf_set_new_array_impl(out->token, len, out->num_destinations); \
    out->value = token->value; \
    out->token = token; \
    out->length = len; \
} while(0)
#else
#define _LF_SET_NEW_ARRAY(out, len) \
do { \
    _lf_set_present((lf_port_base_t*)out); \
    lf_token_t* token = _lf_set_new_array_impl(out->token, len, out->num_destinations); \
    out->value = static_cast<decltype(out->value)>(token->value); \
    out->token = token; \
    out->length = len; \
} while(0)
#endif
/**
 * Version of set() for output types given as 'type[number]'.
 *
 * This sets the _is_present variable corresponding to the specified output
 * to true (which causes the array message to be sent). The values in the
 * output are normally written directly to the array or struct before or
 * after this is called.
 * @param out The output port (by name).
 */
#define lf_set_present(out) \
do { \
    _lf_set_present((lf_port_base_t*)out); \
} while(0)

/**
 * Version of set() for output types given as 'type*' or 'type[]' where you want
 * to forward an input or action without copying it.
 *
 * The deallocation of memory is delegated to downstream reactors, which
 * automatically deallocate when the reference count drops to zero.
 * @param out The output port (by name).
 * @param token A pointer to token obtained from an input or action.
 */
#ifndef __cplusplus
#define _LF_SET_TOKEN(out, newtoken) \
do { \
    _lf_set_present((lf_port_base_t*)out); \
    out->value = newtoken->value; \
    out->token = newtoken; \
    newtoken->ref_count += out->num_destinations; \
    out->length = newtoken->length; \
} while(0)
#else
#define _LF_SET_TOKEN(out, newtoken) \
do { \
    _lf_set_present((lf_port_base_t*)out); \
    out->value = static_cast<decltype(out->value)>(newtoken->value); \
    out->token = newtoken; \
    newtoken->ref_count += out->num_destinations; \
    out->length = newtoken->length; \
} while(0)
#endif

/**
 * Set the destructor used to free "token->value" set on "out".
 * That memory will be automatically freed once all downstream
 * reactions no longer need the value.
 *
 * @param out The output port (by name) or input of a contained
 *            reactor in form input_name.port_name.
 * @param dtor A pointer to a void function that takes a pointer argument
 *             or NULL to use the default void free(void*) function.
 */
#define _LF_SET_DESTRUCTOR(out, dtor) \
do { \
    out->destructor = dtor; \
} while(0)

/**
 * Set the destructor used to copy construct "token->value" received
 * by "in" if "in" is mutable.
 *
 * @param out The output port (by name) or input of a contained
 *            reactor in form input_name.port_name.
 * @param cpy_ctor A pointer to a void* function that takes a pointer argument
 *                 or NULL to use the memcpy operator.
 */
#define _LF_SET_COPY_CONSTRUCTOR(out, cpy_ctor) \
do { \
    out->copy_constructor = cpy_ctor; \
} while(0)

/**
 * Macro for extracting the deadline from the index of a reaction.
 * The reaction queue is sorted according to this index, and the
 * use of the deadline here results in an earliest deadline first
 * (EDF) scheduling poicy.
 */
#define DEADLINE(index) (index & 0x7FFFFFFFFFFF0000)

/**
 * Macro for determining whether two reactions are in the
 * same chain (one depends on the other). This is conservative.
 * If it returns false, then they are surely not in the same chain,
 * but if it returns true, they may be in the same chain.
 * This is in reactor_threaded.c to execute reactions in parallel
 * on multiple cores even if their levels are different.
 */
#define OVERLAPPING(chain1, chain2) ((chain1 & chain2) != 0)

//  ======== Function Declarations ========  //

/**
 * Return the global STP offset on advancement of logical
 * time for federated execution.
 */
interval_t lf_get_stp_offset(void);

/**
 * Set the global STP offset on advancement of logical
 * time for federated execution.
 *
 * @param offset A positive time value to be applied
 *  as the STP offset.
 */
void lf_set_stp_offset(interval_t offset);

/**
 * Print a snapshot of the priority queues used during execution
 * (for debugging).
 */
void lf_print_snapshot(void);

/**
 * Request a stop to execution as soon as possible.
 * In a non-federated execution, this will occur
 * at the conclusion of the current logical time.
 * In a federated execution, it will likely occur at
 * a later logical time determined by the RTI so that
 * all federates stop at the same logical time.
 */
void lf_request_stop(void);

/**
 * Allocate zeroed-out memory and record the allocated memory on
 * the specified list so that it will be freed when calling
 * {@link _lf_free(allocation_record_t**)}.
 * @param count The number of items of size 'size' to accomodate.
 * @param size The size of each item.
 * @param head Pointer to the head of a list on which to record
 *  the allocation, or NULL to not record it.
 */
void* _lf_allocate(
		size_t count, size_t size, struct allocation_record_t** head);

/**
 * Free memory allocated using
 * {@link _lf_allocate(size_t, size_t, allocation_record_t**)}
 * and mark the list empty by setting `*head` to NULL.
 * @param head Pointer to the head of a list on which to record
 *  the allocation, or NULL to not record it.
 */
void _lf_free(struct allocation_record_t** head);

/**
 * Allocate memory for a new runtime instance of a reactor.
 * This records the reactor on the list of reactors to be freed at
 * termination of the program. If you plan to free the reactor before
 * termination of the program, use
 * {@link _lf_allocate(size_t, size_t, allocation_record_t**)}
 * with a null last argument instead.
 * @param size The size of the self struct, obtained with sizeof().
 */
void* _lf_new_reactor(size_t size);

/**
 * Free all the reactors that are allocated with
 * {@link #_lf_new_reactor(size_t)}.
 */
void _lf_free_all_reactors(void);

/**
 * Free memory recorded on the allocations list of the specified reactor.
 * @param self The self struct of the reactor.
 */
void _lf_free_reactor(struct self_base_t *self);

/**
 * Generated function that optionally sets default command-line options.
 */
void _lf_set_default_command_line_options(void);

/**
 * Generated function that resets outputs to be absent at the
 * start of a new time step.
 */
void _lf_start_time_step(void);

/**
 * Generated function that produces a table containing all triggers
 * (i.e., inputs, timers, and actions).
 */
void _lf_initialize_trigger_objects(void);

/**
 * Pop all events from event_q with timestamp equal to current_time, extract all
 * the reactions triggered by these events, and stick them into the reaction
 * queue.
 */
void _lf_pop_events(void);

/**
 * Internal version of the lf_schedule() function, used by generated
 * _lf_start_timers() function.
 * @param trigger The action or timer to be triggered.
 * @param delay Offset of the event release.
 * @param token The token payload.
 * @return A handle to the event, or 0 if no event was scheduled, or -1 for error.
 */
trigger_handle_t _lf_schedule(trigger_t* trigger, interval_t delay, lf_token_t* token);

/**
 * Function (to be code generated) to schedule timers.
 */
void _lf_initialize_timers(void);

/**
 * Function (to be code generated) to trigger startup reactions.
 */
void _lf_trigger_startup_reactions(void);


/**
 * Function (to be code generated) to terminate execution.
 * This will be invoked after all shutdown actions have completed.
 */
void terminate_execution(void);

void termination(void);

/**
 * Function (to be code generated) to trigger shutdown reactions.
 */
bool _lf_trigger_shutdown_reactions(void);

/**
 * Create a new token and initialize it.
 * The value pointer will be NULL and the length will be 0.
 * @param element_size The size of an element carried in the payload or
 *  0 if there is no payload.
 * @return A pointer to a new or recycled lf_token_t struct.
 */
lf_token_t* create_token(size_t element_size);

/**
 * Schedule the specified action with an integer value at a later logical
 * time that depends on whether the action is logical or physical and
 * what its parameter values are. This wraps a copy of the integer value
 * in a token. See schedule_token() for more details.
 * @param action The action to be triggered.
 * @param extra_delay Extra offset of the event release above that in the action.
 * @param value The value to send.
 * @return A handle to the event, or 0 if no event was scheduled, or -1 for error.
 */
trigger_handle_t _lf_schedule_int(void* action, interval_t extra_delay, int value);

/**
 * Create a dummy event to be used as a spacer in the event queue.
 */
event_t* _lf_create_dummy_event(trigger_t* trigger, instant_t time, event_t* next, unsigned int offset);

/**
 * Schedule the specified action with the specified token as a payload.
 * This will trigger an event at a later logical time that depends
 * on whether the action is logical or physical and what its parameter
 * values are.
 *
 * logical action: A logical action has an offset (default is zero)
 * and a minimum interarrival time (MIT), which also defaults to zero.
 * The logical time at which this scheduled event will trigger is
 * the current time plus the offset plus the delay argument given to
 * this function. If, however, that time is not greater than a prior
 * triggering of this logical action by at least the MIT, then the
 * one of two things can happen depending on the policy specified
 * for the action. If the action's policy is DROP (default), then the
 * action is simply dropped and the memory pointed to by value argument
 * is freed. If the policy is DEFER, then the time will be increased
 * to equal the time of the most recent triggering plus the MIT.
 *
 * For the above, "current time" means the logical time of the
 * reaction that is calling this function. Logical actions should
 * always be scheduled within a reaction invocation, never asynchronously
 * from the outside. FIXME: This needs to be checked.
 *
 * physical action: A physical action has all the same parameters
 * as a logical action, but its timestamp will be the larger of the
 * current physical time and the time it would be assigned if it
 * were a logical action.
 *
 * The token is required to be either NULL or a pointer to
 * a token created using create_token().
 *
 * There are three conditions under which this function will not
 * actually put an event on the event queue and decrement the reference count
 * of the token (if there is one), which could result in the payload being
 * freed. In all three cases, this function returns 0. Otherwise,
 * it returns a handle to the scheduled trigger, which is an integer
 * greater than 0.
 *
 * The first condition is that stop() has been called and the time offset
 * of this event is greater than zero.
 * The second condition is that the logical time of the event
 * is greater that the stop time (timeout) that is specified in the target
 * properties or on the command line.
 * The third condition is that the trigger argument is null.
 *
 * @param action The action to be triggered.
 * @param extra_delay Extra offset of the event release above that in the action.
 * @param token The token to carry the payload or null for no payload.
 * @return A handle to the event, or 0 if no event was scheduled, or -1 for error.
 */
trigger_handle_t _lf_schedule_token(void* action, interval_t extra_delay, lf_token_t* token);

/**
 * Variant of schedule_token that creates a token to carry the specified value.
 * The value is required to be malloc'd memory with a size equal to the
 * element_size of the specifies action times the length parameter.
 * See _lf_schedule_token() for details.
 * @param action The action to be triggered.
 * @param extra_delay Extra offset of the event release above that in the action.
 * @param value Dynamically allocated memory containing the value to send.
 * @param length The length of the array, if it is an array, or 1 for a
 *  scalar and 0 for no payload.
 * @return A handle to the event, or 0 if no event was scheduled, or -1 for error.
 */
trigger_handle_t _lf_schedule_value(void* action, interval_t extra_delay, void* value, size_t length);

/**
 * Schedule an action to occur with the specified value and time offset
 * with a copy of the specified value. If the value is non-null,
 * then it will be copied into newly allocated memory under the assumption
 * that its size is given in the trigger's token object's element_size field
 * multiplied by the specified length.
 * See _lf_schedule_token(), which this uses, for details.
 * @param action Pointer to an action on a self struct.
 * @param offset The time offset over and above that in the action.
 * @param value A pointer to the value to copy.
 * @param length The length, if an array, 1 if a scalar, and 0 if value is NULL.
 * @return A handle to the event, or 0 if no event was scheduled, or -1 for error.
 */
trigger_handle_t _lf_schedule_copy(void* action, interval_t offset, void* value, size_t length);

/**
 * For a federated execution, send a STOP_REQUEST message
 * to the RTI.
 */
void _lf_fd_send_stop_request_to_rti(void);

/**
 * Check the deadline of the currently executing reaction against the
 * current physical time. If the deadline has passed, invoke the deadline
 * handler (if invoke_deadline_handler parameter is set true) and return true.
 * Otherwise, return false.
 *
 * @param self The self struct of the reactor.
 * @param invoke_deadline_handler When this is set true, also invoke deadline
 *  handler if the deadline has passed.
 * @return True if the specified deadline has passed and false otherwise.
 */
bool _lf_check_deadline(self_base_t* self, bool invoke_deadline_handler);

/**
 * These functions must be implemented by both threaded and unthreaded
 * runtime. Should be routed to appropriate API calls in platform.h
*/

/**
 * @brief Notify other threads of new events on the event queue.
 * 
 */
void _lf_notify_of_event();

/**
 * @brief Enter critical section. Must be paired with a
 * `_lf_critical_section_exit()`
 * 
 */
int _lf_critical_section_enter();

/**
 * @brief Leave critical section
 */
int _lf_critical_section_exit();

#endif /* REACTOR_H */
/** @} */