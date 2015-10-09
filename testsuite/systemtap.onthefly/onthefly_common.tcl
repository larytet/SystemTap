# Contains common procs used by onthefly testcases.

# Returns true if this subtest targets dyninst
proc is_dyninst_subtest {subtest} {
   return [string match dyninst_* $subtest]
}

# Checks if the output matches the expected pattern. The 'patterns' arg is a
# list of lines which are matched against the output using [string match], so
# globby chars are allowed.
proc is_valid_output {output patterns} {

   # i represents the index of the last pattern matched in 'patterns' list
   set i -1
   foreach line [split $output "\n"] {

      # Passed all patterns
      if {$i >= [llength $patterns]} {
         verbose -log "no more patterns to match against"
         return 0
      }

      # Matches the next pattern?
      set next_pattern [lindex $patterns [expr $i + 1]]
      if {[string match $next_pattern $line]} {
         incr i
      } else {
          verbose -log "expected: $next_pattern"
          verbose -log "received: $line"
          # continue; $i will be too small, but we can see more errors
      }
   }

   # Check that we went through all the patterns
   if {$i >= [expr [llength $patterns] - 1]} {
      return 1
   } else {
      return 0
   }
}

# Runs the subtest with the given parameters. See make_script for SUBTEST,
# ENABLED, MAXTOGGLES, and TIMER. The 'args' parameter contains any extra
# arguments to pass to stap.
proc run_subtest {SUBTEST ENABLED MAXTOGGLES TIMER args} {
   global test

   # Prepare the script
   set script [make_script $SUBTEST $ENABLED $MAXTOGGLES $TIMER]

   # Run stap (and throw on error)
   if {[catch {run_stap $SUBTEST $args $script} stapout]} {
      verbose -log "stap error: $stapout"
      error "stap"
      return
   }

   return $stapout
}

proc cannot_test {subtest timer} {
   global test

   if {[is_dyninst_subtest $subtest] && ![dyninst_p]} {
      untested "$test - $subtest (no dyninst)"
      return 1
   }

   if {$timer < 1000 && ![hrtimer_p]} {
      untested "$test - $subtest (no hrtimer)"
      return 1
   }

   return 0
}

# Same as run_subtest, except that the output is then checked for validity
proc run_subtest_valid {subtest start_enabled max_toggles timer args} {
   global test
   if {[cannot_test $subtest $timer]} { return }

   if {[catch {eval run_subtest $subtest $start_enabled \
                                $max_toggles $timer $args} output]} {
      fail "$test - $subtest ($output)"
      return
   }

   # Prepare the pattern
   set pattern [make_pattern $subtest $start_enabled $max_toggles]

   # Check that the output is valid
   if {![is_valid_output $output $pattern]} {
      fail "$test - $subtest (invalid output)"
      return
   }

   pass "$test - $subtest (valid output)"
}

# Same as run_subtest, except that errors are caught and cause FAILs. If
# run_subtest returns without errors, we PASS.
proc run_subtest_stress {subtest start_enabled max_toggles timer args} {
   global test
   if {[cannot_test $subtest $timer]} { return }

   # We don't care about warnings/handler errors
   set args "$args --suppress-handler-errors -w"

   if {[catch {eval run_subtest $subtest $start_enabled \
                                $max_toggles $timer $args} output]} {
      fail "$test - $subtest ($output)"
      return
   }

   pass "$test - $subtest (survived)"
}

