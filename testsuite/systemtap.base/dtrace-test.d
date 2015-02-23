#define INT16 short
#define INT32 int

provider tstsyscall
{
 probe test0();
 probe test1(INT16 arg1, INT32 arg2, INT32 arg3, INT32 arg4, struct astruct arg5);
 probe test2(INT16 arg1, INT32 arg2, INT32 arg3, INT32 arg4, struct astruct arg5);
}
