int bar (int i);

int foo (int v)
{
	return bar (v - 1);
}

int
main (int argc, char **argv)
{
	return foo (argc);
}
