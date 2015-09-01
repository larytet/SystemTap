#include <linux/module.h>
#include <trace/events/module.h>

// NB: in kernels which do have the requisite pieces, just unconfigured, then
// everything below will compile just fine, only returning ENOSYS at runtime.
// To get the compile-time error that autoconf needs, check it directly:
#ifndef CONFIG_TRACEPOINTS
#error "CONFIG_TRACEPOINTS is not enabled"
#endif

// presuming void *data-parametrized tracepoint callback api

void __module_load(void *cb_data, struct module* mod)
{
        (void) cb_data;
        (void) mod;
	return;
}

void __module_free(void *cb_data, struct module* mod)
{
        (void) cb_data;
        (void) mod;
	return;
}

void __autoconf_func(void)
{
	(void) register_trace_module_load(__module_load, NULL);
        (void) register_trace_module_free(__module_free, NULL);
}
