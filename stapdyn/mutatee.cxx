// stapdyn mutatee functions
// Copyright (C) 2012-2014 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "mutatee.h"

extern "C" {
#include <signal.h>
#include <sys/types.h>
}

#include <BPatch_function.h>
#include <BPatch_image.h>
#include <BPatch_module.h>
#include <BPatch_point.h>
#include <BPatch_thread.h>

#include "dynutil.h"
#include "../util.h"

extern "C" {
#include "../runtime/dyninst/stapdyn.h"
}

using namespace std;


// Create snippets for all the DWARF registers,
// in their architecture-specific order.
static void
get_dwarf_registers(BPatch_process *app,
                    vector<BPatch_snippet*>& registers)
{
  static const char* const names[] = {
#if defined(__i386__)
      "eax", "ecx", "edx", "ebx",
      "esp", "ebp", "esi", "edi",
#elif defined(__x86_64__)
      "rax", "rdx", "rcx", "rbx",
      "rsi", "rdi", "rbp", "rsp",
      "r8",  "r9",  "r10", "r11",
      "r12", "r13", "r14", "r15",
#elif defined(__powerpc__) || defined(__powerpc64__)
      "r0",  "r1",  "r2",  "r3",
      "r4",  "r5",  "r6",  "r7",
      "r8",  "r9",  "r10", "r11",
      "r12", "r13", "r14", "r15",
      "r16", "r17", "r18", "r19",
      "r20", "r21", "r22", "r23",
      "r24", "r25", "r26", "r27",
      "r28", "r29", "r30", "r31",
#endif
      NULL };

  // First push the original PC, before instrumentation mucked anything up.
  // (There's also BPatch_actualAddressExpr for the instrumented result...)
  registers.push_back(new BPatch_originalAddressExpr());

  vector<BPatch_register> bpregs;
  app->getRegisters(bpregs);

  ostream& debug_reg = staplog(4);
  debug_reg << "pid " << app->getPid() << " has "
             << bpregs.size() << " registers available:";
  for (size_t i = 0; i < bpregs.size(); ++i)
    debug_reg << " " << bpregs[i].name();
  debug_reg << endl;

  // Look for each DWARF register in BPatch's register set.
  // O(m*n) loop, but neither array is very large
  for (const char* const* name = names; *name; ++name)
    {
      size_t i;
      for (i = 0; i < bpregs.size(); ++i)
        if (bpregs[i].name() == *name)
          {
            // Found it, add a snippet.
            registers.push_back(new BPatch_registerExpr(bpregs[i]));
            break;
          }

      // If we didn't find it, put a zero in its place.
      if (i >= bpregs.size())
        registers.push_back(new BPatch_constExpr((unsigned long)0));
    }

#if defined(__powerpc__) || defined(__powerpc64__)
  // In EmitterPOWER::emitCall(), Dyninst enforces a limit that "only 8
  // arguments can (currently) be passed on the POWER architecture."
  // We start with the probe index and nregs, leaving just 6 more...
  while (registers.size() > 6)
    {
      delete registers.back();
      registers.pop_back();
    }
#endif
}


// Simple object to temporarily make sure a process is stopped
class mutatee_freezer {
    mutatee& m;
    bool already_stopped;

  public:
    mutatee_freezer(mutatee& m):
      m(m), already_stopped(m.is_stopped())
    {
      // If process is currently running, stop it.
      if (!already_stopped && !m.stop_execution())
        {
          staplog(3) << "stopping process failed, stopped="
                     << m.is_stopped() << ", terminated="
                     << m.is_terminated() << endl;
        }
    }

    ~mutatee_freezer()
    {
      // Let the process continue (if it wasn't stopped when we started).
      if (!already_stopped)
        m.continue_execution();
    }
};


mutatee::mutatee(BPatch_process* process):
  pid(process? process->getPid() : 0),
  process(process), stap_dso(NULL),
  utrace_enter_function(NULL)
{
  get_dwarf_registers(process, registers);
}

mutatee::~mutatee()
{
  remove_instrumentation();
  unload_stap_dso();
  if (process)
    process->detach(true);
}


// Inject the stap module into the target process
bool
mutatee::load_stap_dso(const string& filename)
{
  stap_dso = process->loadLibrary(filename.c_str());
  if (!stap_dso)
    {
      staperror() << "Couldn't load " << filename
                  << " into the target process" << endl;
      return false;
    }
  return true;
}

// Unload the stap module from the target process
void
mutatee::unload_stap_dso()
{
  if (!process || !stap_dso)
    return;

  // XXX Dyninst has no unloadLibrary() yet, as of 8.0
  stap_dso = NULL;
}



void
mutatee::update_semaphores(unsigned short delta, size_t start)
{
  if (!process || process->isTerminated())
    return;

  for (size_t i=start; i < semaphores.size(); ++i)
    {
      unsigned short value = 0;
      BPatch_variableExpr* semaphore = semaphores[i];

      // Read-modify-write the semaphore remotely.
      semaphore->readValue(&value, (int)sizeof(value));
      value += delta;
      semaphore->writeValue(&value, (int)sizeof(value));
    }
  // NB: Alternatively, we could build up a oneTimeCode snippet to do them all
  // at once within the target process.  For now, remote seems good enough.
}


void
mutatee::call_utrace_dynprobes(const vector<dynprobe_location>& probes,
			       BPatch_thread* thread)
{
  if (!stap_dso || probes.empty())
    return;

  if (utrace_enter_function == NULL)
    {
      vector<BPatch_function *> functions;
      stap_dso->findFunction("enter_dyninst_utrace_probe",
			     functions);
      if (!functions.empty())
	utrace_enter_function = functions[0];
      else
	{
	  staplog(1) << "no utrace enter function in pid " << pid << "!" << endl;
	  return;
	}
    }

  for (size_t i = 0; i < probes.size(); ++i)
    {
      const dynprobe_location& probe = probes[i];
      vector<BPatch_snippet *> args;
      args.push_back(new BPatch_constExpr((int64_t)probe.index));
      args.push_back(new BPatch_constExpr((void*)NULL)); // pt_regs
      BPatch_funcCallExpr call(*utrace_enter_function, args);
      staplog(3) << "calling utrace function in pid " << pid
		 << " for probe index " << probe.index << endl;
      if (thread)
	thread->oneTimeCode(call);
      else
	process->oneTimeCode(call);
    }
}


// Remember utrace probes. They get handled when the associated
// callback hits.
void
mutatee::instrument_utrace_dynprobe(const dynprobe_location& probe)
{
  // Just remember this probe. It will get called from a callback function.
  attached_probes.push_back(probe);
}


// Handle "global" targets. They aren't really global, but non-path
// based probes, like:
//	probe process.begin { ... }
//	probe process(PID).begin { ... }
void
mutatee::instrument_global_dynprobe_target(const dynprobe_target& target)
{
  staplog(1) << "found global target in pid " << pid << ", inserting "
             << target.probes.size() << " probes" << endl;

  for (size_t j = 0; j < target.probes.size(); ++j)
    {
      const dynprobe_location& probe = target.probes[j];

      // We already know this isn't a path-based probe. We've got 2
      // other qualifications here:
      // (1) Make sure this is a utrace probe by checking the flags.
      // (2) If PID was specified, does the pid match?
      if (((probe.flags & (STAPDYN_PROBE_FLAG_PROC_BEGIN
			   | STAPDYN_PROBE_FLAG_PROC_END
			   | STAPDYN_PROBE_FLAG_THREAD_BEGIN
			   | STAPDYN_PROBE_FLAG_THREAD_END)) != 0)
	  && (probe.offset == 0 || (int)probe.offset == process->getPid()))
	instrument_utrace_dynprobe(probe);
    }
}


// Given a target and the matching object, instrument all of the probes
// with calls to the stap_dso's entry function.
void
mutatee::instrument_dynprobe_target(BPatch_object* object,
                                    const dynprobe_target& target)
{
  if (!process || !stap_dso || !object)
    return;

  vector<BPatch_function *> functions;
  BPatch_function* enter_function = NULL;
  bool use_pt_regs = false;

  staplog(1) << "found target \"" << target.path << "\" in pid " << pid
	     << ", inserting " << target.probes.size() << " probes" << endl;

  process->beginInsertionSet();
  for (size_t j = 0; j < target.probes.size(); ++j)
    {
      const dynprobe_location& probe = target.probes[j];

      if ((probe.flags & (STAPDYN_PROBE_FLAG_PROC_BEGIN
			  | STAPDYN_PROBE_FLAG_PROC_END
			  | STAPDYN_PROBE_FLAG_THREAD_BEGIN
			  | STAPDYN_PROBE_FLAG_THREAD_END)) != 0)
        {
	  instrument_utrace_dynprobe(probe);
	  continue;
	}

      if (! enter_function)
        {
	  // XXX Until we know how to build pt_regs from here, we'll
	  // try the entry function for individual registers first.
	  if (!registers.empty())
	    stap_dso->findFunction("enter_dyninst_uprobe_regs", functions,
				   false);
	  if (!functions.empty())
	    enter_function = functions[0];

	  // If the other entry wasn't found, or we don't have
	  // registers for it anyway, try the form that takes pt_regs*
	  // and we'll just pass NULL.
	  if (!enter_function)
	    {
	      stap_dso->findFunction("enter_dyninst_uprobe", functions,
				     false);
	      if (functions.empty())
	        {
		  stapwarn() << "Couldn't find the uprobe entry function (either " << endl
			     << "\"enter_dyninst_uprobe_regs\" or \"enter_dyninst_uprobe\"). Uprobe probes"
			     << endl << "disabled." << endl;
		  return;
		}
	      use_pt_regs = true;
	      enter_function = functions[0];
	    }
	}

      // Convert the file offset to a memory address.
      Dyninst::Address address = object->fileOffsetToAddr(probe.offset);
      if (address == BPatch_object::E_OUT_OF_BOUNDS)
        {
          stapwarn() << "Couldn't convert " << target.path << "+"
                     << lex_cast_hex(probe.offset) << " to an address" << endl;
          continue;
        }

      // Turn the address into instrumentation points.
      // NB: There may be multiple results if Dyninst determined that multiple
      // concrete functions have overlapping ranges.  Rare, but possible.
      vector<BPatch_point*> points;
      object->findPoints(address, points);
      if (points.empty())
        {
          stapwarn() << "Couldn't find an instrumentation point at "
                     << lex_cast_hex(address) << ", " << target.path
                     << "+" << lex_cast_hex(probe.offset) << endl;
          continue;
        }

      // Check that the functions containing each point are actually
      // instrumentable.  Unfortunately, findPoints doesn't make any
      // distinction, and insertSnippet doesn't tell us either, so this
      // is the best way we have to let the user know.
      vector<BPatch_point*> instrumentable_points;
      for (size_t i = 0; i < points.size(); ++i)
        if (points[i]->getFunction()->isInstrumentable())
          instrumentable_points.push_back(points[i]);
        else
          stapwarn() << "Couldn't instrument the function containing "
                     << lex_cast_hex(address) << ", " << target.path
                     << "+" << lex_cast_hex(probe.offset) << endl;
      points.swap(instrumentable_points);

      if (probe.return_p)
        {
          // Transform the address points into function exits
          vector<BPatch_point*> return_points;
          for (size_t i = 0; i < points.size(); ++i)
            {
              vector<BPatch_point*>* exits =
                points[i]->getFunction()->findPoint(BPatch_locExit);
              if (!exits || exits->empty())
                {
                  stapwarn() << "Couldn't find a return point from "
                             << lex_cast_hex(address) << ", " << target.path
                             << "+" << lex_cast_hex(probe.offset) << endl;
                  continue;
                }
              return_points.insert(return_points.end(),
                                   exits->begin(), exits->end());
            }
          points.swap(return_points);
        }

      if (points.empty())
        continue;

      // The entry function needs the index of this particular probe, then
      // the registers in whatever form we chose above.
      vector<BPatch_snippet *> args;
      args.push_back(new BPatch_constExpr((int64_t)probe.index));
      if (use_pt_regs)
        args.push_back(new BPatch_constExpr((void*)NULL)); // pt_regs
      else
        {
          args.push_back(new BPatch_constExpr((unsigned long)registers.size()));
          args.insert(args.end(), registers.begin(), registers.end());
        }
      BPatch_funcCallExpr call(*enter_function, args);

      // Finally write the instrumentation for the probe!
      BPatchSnippetHandle* handle = process->insertSnippet(call, points);
      if (handle)
        snippets.push_back(handle);

      // Update SDT semaphores as needed.
      if (probe.semaphore)
        {
          Dyninst::Address sem_address = object->fileOffsetToAddr(probe.semaphore);
          if (sem_address == BPatch_object::E_OUT_OF_BOUNDS)
            stapwarn() << "Couldn't convert semaphore " << target.path << "+"
                       << lex_cast_hex(probe.offset) << " to an address" << endl;
          else
            {
              // Create a variable to represent this semaphore
              BPatch_type *sem_type = process->getImage()->findType("unsigned short");
              BPatch_variableExpr *semaphore = process->createVariable(sem_address, sem_type);
              if (semaphore)
                semaphores.push_back(semaphore);
            }
        }
    }
  process->finalizeInsertionSet(false);
}


// Look for "global" (non-path based) probes and handle them.
void
mutatee::instrument_global_dynprobes(const vector<dynprobe_target>& targets)
{
  if (!process || !stap_dso || targets.empty())
    return;

  // Look for global (non path-based probes), and remember them.
  for (size_t i = 0; i < targets.size(); ++i)
    {
      const dynprobe_target& target = targets[i];

      // Do the real work...
      if (target.path.empty())
	instrument_global_dynprobe_target(target);
    }
}


// Look for all matches between this object and the targets
// we want to probe, then do the instrumentation.
void
mutatee::instrument_object_dynprobes(BPatch_object* object,
                                     const vector<dynprobe_target>& targets)
{
  if (!process || !stap_dso || !object || targets.empty())
    return;

  // We want to map objects by their full path, but the pathName from
  // Dyninst might be relative, so fill it out.
  string path = resolve_path(object->pathName());
  staplog(2) << "found object \"" << path << "\" in pid " << pid << endl;

  size_t semaphore_start = semaphores.size();

  // Match the object to our targets, and instrument matches.
  for (size_t i = 0; i < targets.size(); ++i)
    {
      const dynprobe_target& target = targets[i];

      // Do the real work...
      if (path == target.path)
        instrument_dynprobe_target(object, target);
    }

  // Increment new semaphores
  update_semaphores(1, semaphore_start);
}


void
mutatee::begin_callback(BPatch_thread *thread)
{
  const vector<dynprobe_location>& proc_begin_probes =
    find_attached_probes(STAPDYN_PROBE_FLAG_PROC_BEGIN);

  // Shortcut out if there aren't any relevant probes
  if (proc_begin_probes.empty())
    return;

  // process->oneTimeCode() requires that the process be stopped
  mutatee_freezer mf(*this);
  if (!is_stopped())
    return;

  staplog(2) << "firing " << proc_begin_probes.size()
	     << " process.begin probes in pid " << pid << endl;
  call_utrace_dynprobes(proc_begin_probes, thread);
}


// FIXME: When dyninst's registerExitCallback() hits, it's too late
// to stop the process and inject oneTimeCode() for process.end.
// So while the code here works for post-exec "end", for now the
// mutator::exit_callback() will run its probes locally in stapdyn.
void
mutatee::exit_callback(BPatch_thread *thread, bool exec_p)
{
  const vector<dynprobe_location>& proc_end_probes =
    exec_p ? exec_proc_end_probes
    : find_attached_probes(STAPDYN_PROBE_FLAG_PROC_END);

  // Shortcut out if there aren't any relevant probes
  if (proc_end_probes.empty())
    return;

  // thread->oneTimeCode() requires that the process (not just the
  // thread) be stopped. So, stop the process if needed.
  mutatee_freezer mf(*this);
  if (!is_stopped())
    return;

  staplog(2) << "firing " << proc_end_probes.size()
	     << " process.end probes in pid " << pid << endl;
  call_utrace_dynprobes(proc_end_probes, thread);
}


void
mutatee::thread_callback(BPatch_thread *thread, bool create_p)
{
  const vector<dynprobe_location>& probes =
    find_attached_probes(create_p
			 ? STAPDYN_PROBE_FLAG_THREAD_BEGIN
			 : STAPDYN_PROBE_FLAG_THREAD_END);

  // Shortcut out if there aren't any relevant probes
  if (probes.empty())
    return;

  // If 'thread' is the main process, just return. We can't stop the
  // process before it terminates.
  if (thread->getLWP() == process->getPid())
    return;

  // thread->oneTimeCode() requires that the process (not just the
  // thread) be stopped. So, stop the process if needed.
  mutatee_freezer mf(*this);
  if (!is_stopped())
    return;

  staplog(2) << "firing " << probes.size()
	     << " process.thread." << (create_p ? "begin" : "end")
	     << " probes in pid " << pid << endl;
  call_utrace_dynprobes(probes, thread);
}


vector<dynprobe_location>
mutatee::find_attached_probes(uint64_t flag)
{
  vector<dynprobe_location> probes;
  for (size_t i = 0; i < attached_probes.size(); ++i)
    {
      const dynprobe_location& probe = attached_probes[i];
      if (probe.flags & flag)
       probes.push_back(probe);
    }
  return probes;
}


// Look for probe matches in all objects.
void
mutatee::instrument_dynprobes(const vector<dynprobe_target>& targets)
{
  if (!process || !stap_dso || targets.empty())
    return;

  BPatch_image* image = process->getImage();
  if (!image)
    return;

  // Match non object/path specific probes.
  instrument_global_dynprobes(targets);

  // Read all of the objects in the process.
  vector<BPatch_object *> objects;
  image->getObjects(objects);
  for (size_t i = 0; i < objects.size(); ++i)
    instrument_object_dynprobes(objects[i], targets);
}


// Copy data for forked instrumentation
void
mutatee::copy_forked_instrumentation(mutatee& other)
{
  if (!process)
    return;

  // Freeze both processes, so we have a stable base.
  mutatee_freezer mf_parent(other), mf(*this);

  // Find the same stap module in the fork
  if (other.stap_dso)
    {
      BPatch_image* image = process->getImage();
      if (!image)
	return;

      string dso_path = other.stap_dso->pathName();

      vector<BPatch_object *> objects;
      image->getObjects(objects);
      for (size_t i = 0; i < objects.size(); ++i)
	if (objects[i]->pathName() == dso_path)
	  {
	    stap_dso = objects[i];
	    break;
	  }
    }

  // Get new handles for all inserted snippets
  for (size_t i = 0; i < other.snippets.size(); ++i)
    {
      BPatchSnippetHandle *handle =
	process->getInheritedSnippet(*other.snippets[i]);
      if (handle)
        snippets.push_back(handle);
    }

  // Get new variable representations of semaphores
  for (size_t i = 0; i < other.semaphores.size(); ++i)
    {
      BPatch_variableExpr *semaphore =
        process->getInheritedVariable(*other.semaphores[i]);
      if (semaphore)
        semaphores.push_back(semaphore);
    }

  // Update utrace probes to match, except PID-based probes.
  // (A forked PID will never be the same as the parent.)
  for (size_t i = 0; i < other.attached_probes.size(); ++i)
    {
      const dynprobe_location& probe = other.attached_probes[i];
      if (probe.offset == 0)
	instrument_utrace_dynprobe(probe);
    }
}


// Reset instrumentation after an exec
void
mutatee::exec_reset_instrumentation()
{
  // Reset members that are now out of date
  stap_dso = NULL;
  snippets.clear();
  semaphores.clear();

  // NB: the utrace process.end probes are saved, so they can run right
  // before the new process does its process.begin.
  exec_proc_end_probes = find_attached_probes(STAPDYN_PROBE_FLAG_PROC_END);

  attached_probes.clear();
  utrace_enter_function = NULL;
}


// Remove all BPatch snippets we've instrumented in the target
void
mutatee::remove_instrumentation()
{
  if (!process || snippets.empty())
    return;

  process->beginInsertionSet();
  for (size_t i = 0; i < snippets.size(); ++i)
    process->deleteSnippet(snippets[i]);
  process->finalizeInsertionSet(false);
  snippets.clear();

  // Decrement all semaphores
  update_semaphores(-1);
  semaphores.clear();

  unload_stap_dso();
}


// Look up a function by name in the target and invoke it without parameters.
void
mutatee::call_function(const string& name)
{
  vector<BPatch_snippet *> args;
  call_function(name, args);
}

// Look up a function by name in the target and invoke it with parameters.
void
mutatee::call_function(const string& name,
                       const vector<BPatch_snippet *>& args)
{
  if (!stap_dso)
    return;

  // process->oneTimeCode() requires that the process be stopped
  mutatee_freezer mf(*this);
  if (!is_stopped())
    return;

  vector<BPatch_function *> functions;
  stap_dso->findFunction(name.c_str(), functions);

  // XXX Dyninst can return multiple results, but we're not really
  // expecting that... should we really call them all anyway?
  for (size_t i = 0; i < functions.size(); ++i)
    {
      BPatch_funcCallExpr call(*functions[i], args);
      staplog(3) << "calling function '" << name << "' in pid " << pid << endl;
      process->oneTimeCode(call);
    }
}


// Send a signal to the process.
int
mutatee::kill(int signal)
{
  return pid ? ::kill(pid, signal) : -2;
}


void
mutatee::continue_execution()
{
  if (is_stopped())
    {
      staplog(2) << "continuing execution of pid " << pid << endl;
      process->continueExecution();
    }
}


bool
mutatee::stop_execution()
{
  if (process->isStopped())
    {
      // Process is already stopped, no need to do anything else.
      return true;
    }

  staplog(2) << "stopping execution of pid " << pid << endl;
  if (! process->stopExecution())
    {
      staplog(1) << "stopExecution on pid " << pid << " failed!" << endl;
      return false;
    }
  if (! process->isStopped() || process->isTerminated())
    {
      staplog(1) << "couldn't stop pid " << pid << "!" << endl;
      return false;
    }
  return true;
}
/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
