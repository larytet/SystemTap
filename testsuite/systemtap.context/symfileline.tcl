set correct_fileline 0
set correct_file 0
set correct_line 0
set pp_matched 0
set testname symfileline

# tests functions symfileline, symfile and symline

set cmd "stap -d kernel -d systemtap_test_module2 $srcdir/$subdir/$testname.stp"
send_log "executing: $cmd\n"
eval spawn $cmd
expect {
  -timeout 120
  "READY" { 
    # KLUDGE WARNING: Why is this sleep needed? Good question. This
    # test kept failing, but worked fine when run by hand. The only
    # real difference when run by the testsuite vs. running it by hand
    # was the timing. When this sleep was added, the test started
    # passing again.
    catch { exec sleep 1 }
    send_log "executing: echo 0 > /proc/stap_test_cmd\n"
    exec echo 0 > /proc/stap_test_cmd
    expect {
      -re {fileline .*systemtap_test_module2.c:[1-9][0-9]*} {
	incr correct_fileline; exp_continue }
      -re {file .*systemtap_test_module2.c} { incr correct_file; exp_continue }
      -re {line [1-9][0-9]*} { incr correct_line; exp_continue }
      -re {matched} { incr pp_matched; exp_continue }
    }
  }
  timeout {fail "$testname (timeout)"}
}

if { $correct_fileline } { pass "symfileline ()" } { fail "symfileline ()" }
if { $correct_file } { pass "symfile ()" } { fail "symfile ()" }
if { $correct_line } { pass "symline ()" } { fail "symline ()" }
if { $pp_matched } { pass "symfileline in pp ()" } { fail "symfileline in pp ()" }

catch { close }
catch { wait }
