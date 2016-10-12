int
__stp_next_python2_probe_info(char *str)
{
    static int idx = 0;
    if (idx < (sizeof(python2_probe_info) - 1))
    {
	size_t length = clamp_t(size_t,
				(sizeof(python2_probe_info) - idx),
				0, MAXSTRINGLEN);
	strlcpy(str, python2_probe_info + idx, length);
	idx += (length - 1);
	return 0;
    }
    else
    {
	idx = 0;
	return 1;
    }   
}

int
__stp_next_python3_probe_info(char *str)
{
    static int idx = 0;
    if (idx < (sizeof(python3_probe_info) - 1))
    {
	size_t length = clamp_t(size_t,
				(sizeof(python3_probe_info) - idx),
				0, MAXSTRINGLEN);
	strlcpy(str, python3_probe_info + idx, length);
	idx += (length - 1);
	return 0;
    }
    else
    {
	idx = 0;
	return 1;
    }   
}
