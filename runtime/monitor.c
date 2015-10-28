#ifndef _MONITOR_C_
#define _MONITOR_C_
// Temporary solution to redirect output...
static char _monitor_stp_printf_buf[8192];

static void _monitor_fill_printf_buf(char *data, size_t len)
{
  strcat(_monitor_stp_printf_buf, data);
}

static ssize_t
_stp_monitor_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
  ssize_t res = simple_read_from_buffer(buf, count, ppos, _monitor_stp_printf_buf, strlen(_monitor_stp_printf_buf));
  _monitor_stp_printf_buf[0] = '\0';
  return res;
}

static struct file_operations _stp_monitor_fops = {
  .owner = THIS_MODULE,
  .read = _stp_monitor_read
};
#endif
