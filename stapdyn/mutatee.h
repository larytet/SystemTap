// stapdyn mutatee declarations
// Copyright (C) 2012-2013 Red Hat Inc.
// Copyright (C) 2013 Serhei Makarov
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef MUTATEE_H
#define MUTATEE_H

#include <string>
#include <vector>

#include <BPatch_object.h>
#include <BPatch_process.h>
#include <BPatch_snippet.h>

#include "dynprobe.h"
#include "dynutil.h"


class script_target;

// A mutatee is created for each attached process
class mutatee {
  private:
    pid_t pid; // The system's process ID.
    BPatch_process* process; // Dyninst's handle for this process

    std::vector<BPatch_snippet*> registers; // PC + DWARF registers

    std::vector<boost::shared_ptr<script_target> > targets; // all sets of snippets to be inserted

    int c_main_target; // Counts number of scripts that use this as main target

    // disable implicit constructors by not implementing these
    mutatee (const mutatee& other);
    mutatee& operator= (const mutatee& other);

  public:
    mutatee(BPatch_process* process);
    ~mutatee();

    // Create a new set of snippets to instrument this process with.
    boost::shared_ptr<script_target> create_target (bool main_target_p=false);

    void add_main_target() { c_main_target++; }
    void remove_main_target() { c_main_target--; }
    bool is_main_target() { return c_main_target > 0; }

    std::vector<BPatch_snippet*> *get_registers() { return &registers; }

    pid_t process_id() { return pid; }

    bool operator==(BPatch_process* other) { return process == other; }

    // Send a signal to the process.
    int kill(int signal);

    bool stop_execution();
    void continue_execution();
    void terminate_execution() { process->terminateExecution(); }

    bool is_stopped() { return process->isStopped(); }
    bool is_terminated() { return process->isTerminated(); }

    bool check_exit() { return check_dyninst_exit(process); }

    void thread_callback(BPatch_thread *thread, bool create_p);
};

// A script_target manages the snippets a stap module inserts into a mutatee
class script_target {
  private:
    mutatee *owner;
    BPatch_process* process; // Dyninst's handle for the process

    BPatch_object* stap_dso; // the injected stap module

    std::vector<BPatchSnippetHandle*> snippets; // handles from insertSnippet

    std::vector<BPatch_variableExpr*> semaphores; // SDT semaphore variables

    std::vector<dynprobe_location> attached_probes;
    BPatch_function* utrace_enter_function;

    bool p_main_target; // is this the main_target of the script?

    // disable implicit constructors by not implementing these
    script_target (const script_target& other);
    script_target& operator= (const script_target& other);

    void update_semaphores(unsigned short delta, size_t start=0);

    void call_utrace_dynprobe(const dynprobe_location& probe,
                              BPatch_thread* thread=NULL);
    void instrument_utrace_dynprobe(const dynprobe_location& probe);
    void instrument_global_dynprobe_target(const dynprobe_target& target);
    void instrument_global_dynprobes(const std::vector<dynprobe_target>& targets);

 public:
    script_target (mutatee *owner, BPatch_process* process, bool main_target_p);
    ~script_target ();

    // Inject the stap module into the target process
    bool load_stap_dso(const std::string& filename);

    // Unload the stap module from the target process
    void unload_stap_dso();

    bool is_terminated() { return owner->is_terminated(); }
    void continue_execution() { owner->continue_execution(); }
    bool operator==(BPatch_process* other) { return *owner == other; }
    bool check_exit() { return owner->check_exit(); }
    pid_t process_id() { return owner->process_id(); }

    // Given a target and the matching object, instrument all of the probes
    // with calls to the stap_dso's entry function.
    void instrument_dynprobe_target(BPatch_object* object,
                                    const dynprobe_target& target);

    // Look for all matches between this object and the targets
    // we want to probe, then do the instrumentation.
    void instrument_object_dynprobes(BPatch_object* object,
                                     const std::vector<dynprobe_target>& targets);

    // Look for probe matches in all objects.
    void instrument_dynprobes(const std::vector<dynprobe_target>& targets,
                              bool after_exec_p=false);

    // Copy data for forked instrumentation
    void copy_forked_instrumentation(script_target& other);

    // Reset instrumentation after an exec
    void exec_reset_instrumentation();

    // Remove all BPatch snippets we've instrumented in the target
    void remove_instrumentation();

    // Look up a stap function by name and invoke it, optionally with parameters.
    void call_function(const std::string& name);
    void call_function(const std::string& name,
                       const std::vector<BPatch_snippet *>& args);

    void find_attached_probes(uint64_t flag,
			      std::vector<const dynprobe_location *>&probes);

    void begin_callback();
    void exit_callback(BPatch_thread *thread);
    void thread_callback(BPatch_thread *thread, bool create_p);
};


#endif // MUTATEE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
