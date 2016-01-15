#!/bin/sh

# The declaration order should be irrelevant.  Run the script twice, each with
# a different counter declaration order, counting the insns and cycles.  For
# each run get the difference between cycles and insns and find the the largest
# term, e.g. 53874->50000.  Check that this term is equal for both runs.
# Caveat: possible false negative, if one run yields e.g. 59975->50000 and the
# other yields 600200>600000

STAP=$1

declare -A perfresult
for i in "first" "second"; do
perfresult[$i]=$($STAP -g -c "/usr/bin/cat $0 >/dev/null" -e '
global insn

%( @1 == "first" %?
probe perf.hw.instructions.process("/usr/bin/cat").counter("find_insns") {}
probe perf.hw.cpu_cycles.process("/usr/bin/cat").counter("find_cycles") {}
%:
probe perf.hw.cpu_cycles.process("/usr/bin/cat").counter("find_cycles") {}
probe perf.hw.instructions.process("/usr/bin/cat").counter("find_insns") {}
%)

function poly (val) %{ /* unprivileged */
  int i;
  int root = 1;
  int j = STAP_ARG_val;
  if (j < 0)
     j = -j;
  for (root = 1; root < __LONG_MAX__; root *= 10)
     if (root > j)
	{
	  STAP_RETURN ((root/10) * (j / (root/10)));
	}
  STAP_RETURN(0);
  %}

probe process("/usr/bin/cat").function("main")
{
  insn["find_insns"] = @perf("find_insns")
  insn["find_cycles"] = @perf("find_cycles")
}

# in lieu of .return
probe process("/usr/bin/cat").function("main").return
{
  insn["find_cycles"] = (@perf("find_cycles") - insn["find_cycles"])
  insn["find_insns"] =  (@perf("find_insns") - insn["find_insns"])
}

probe end
{
  printf ("%d\n", poly(insn["find_cycles"]-insn["find_insns"]))
}' $i )
done


if [ "${perfresult['first']}" == "${perfresult['second']}" ] ; then
    echo PASS: ${perfresult["first"]}
else
    echo UNRESOLVED: ${perfresult["first"]} ${perfresult["second"]}
fi
