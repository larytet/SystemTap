#!/usr/bin/env tclsh

# List of systemcalls that the systemtap.syscall tests will not cover.
#
# - bdflush: obsolete since 2.6
# - dmi_field_show: Non-syscall picked up by the "sys_*" wildcard below.
# - dmi_modalias_show: Ditto.
# - copyarea: Ditto.
# - fillrect: Ditto.
# - imageblit: Ditto.
# - ni_syscall: non-implemented syscall
# - rt_sigreturn: rt_sigreturn is a syscall that you don't call
#		  directly, so we can't really test it. glibc calls it
#		  in signal handlers. 
# - sigreturn: Ditto.
# - socketcall: common entry point for other socket syscalls
# - tux: obsolete
set badlist {
    bdflush dmi_field_show dmi_modalias_show copyarea fillrect
    imageblit ni_syscall rt_sigreturn sigreturn socketcall tux
}

foreach f $badlist {
    set funcname($f) -1
}

# Get list of syscalls.
set cmd {stap -l "kernel.function(\"sys_*\").call"}
if {[catch {eval exec $cmd} output]} {
    puts "ERROR running stap: $output"
    exit
}
foreach line [split $output "\n"] {
    if {[regexp {^kernel.function\(\"[Ss]y[Ss]_([^@\"]+)} $line match fn]} {
	# arm (and perhaps other platforms) has assembly language
	# syscall wrappers like "sigreturn_wrapper". Notice we're
	# ignoring these.
	if {![info exists funcname($fn)] && ![regexp {_wrapper$} $fn]} {
	    set funcname($fn) 0
	}
    }
}

# Get list of syscall probes.
set cmd {stap -l "syscall.*"}
if {[catch {eval exec $cmd} output]} {
    puts "ERROR running '${cmd}': $output"
    exit
}
foreach line [split $output "\n"] {
    if {[regexp {^syscall\.(.+)$} $line match pn]} {
	set probename($pn) 1
    }
}

# Get list of covered functions.
foreach filename [glob *.c] {
    if {[catch {open $filename r} fd]} {
	puts "ERROR opening $filename: $fd"
	exit
    }
    while {[gets $fd line] >= 0} {
	if {[regexp {/* COVERAGE: ([^\*]*)\*/} $line match res]} {
	    foreach f [split $res] {
		if {[info exists funcname($f)]} {
		    incr funcname($f)
		}
	    }
	}
    }
    close $fd
}

set uncovlist {}
set covered 0
set uncovered 0
set handled 0
set unhandled 0
set unhandled_list {}
foreach {func val} [array get funcname] {
    if {$val < 0} {
	continue
    }

    if {$val > 0} {
	incr covered
    } elseif {$val == 0} {
	incr uncovered
	lappend uncovlist $func
    }

    # If we've got a test program for it, by definition it is
    # handled. It is also handled if we've got a probe for it.
    if {$val > 0 || [info exists probename($func)]} {
	incr handled
    } elseif {$val == 0} {
	incr unhandled
	lappend unhandled_list $func
    }
}

# Display list of covered/uncovered syscalls. A covered syscall has a
# syscall test for it.
set total [expr $covered + $uncovered]
puts "Covered $covered out of $total. [format "%2.1f" [expr ($covered * 100.0)/$total]]%"

puts "\nUNCOVERED FUNCTIONS"
set i 0
foreach f [lsort $uncovlist] {
    puts -nonewline [format "%-24s" $f]
    incr i
    if {$i >= 3} {
	puts ""
	set i 0
    }
}
if {$i != 0} { puts "\n" }

# Display list of handled/unhandled syscalls. A syscall is "handled"
# if syscall/nd_syscall probes exists for the syscall.
set total [expr $handled + $unhandled]
puts "Handled $handled out of $total. [format "%2.1f" [expr ($handled * 100.0)/$total]]%"
if {$unhandled > 0} {
    puts "\nUNHANDLED FUNCTIONS"
    
    set i 0
    foreach f [lsort $unhandled_list] {
	puts -nonewline [format "%-24s" $f]
	incr i
	if {$i >= 3} {
	    puts ""
	    set i 0
	}
    }
    if {$i != 0} { puts "\n" }
}

