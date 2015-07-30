static int
foo (int v)
{
	return v + 1;
}

int
bar (int i)
{
	return foo (i - 1);
}
