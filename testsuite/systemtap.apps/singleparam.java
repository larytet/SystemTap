class singleparam
{
    public static void printMessage(int message)
    {
	//		System.out.println("int: " + message);
	final long j = 254775806;
	printMessage(j);
    }
    public static void printMessage(long message)
    {
	final double k = 3.14;
	printMessage(k);
	//	System.out.println("long: " + message);
    }
    public static void printMessage(double message)
    {
	final float l = 2345987;
	printMessage(l);
	//	System.out.println("double: " + message);
    }
    public static void printMessage(float message)
    {
	final byte n = 10;
	printMessage(n);
	//	System.out.println("float: " + message);
    }
    public static void printMessage(byte message)
    {
	final boolean o = true;
	printMessage(o);
	//	System.out.println("byte: " + message);
    }
    public static void printMessage(boolean message)
    {
	final char p = 'a';
	printMessage(p);
	//	System.out.println("boolean: " + message);
    }
    public static void printMessage(char message)
    {
	final short q = 14;
	printMessage(q);
    }
    public static void printMessage(short message)
    {
	//	System.out.println("short: " + message);
    }


    public static void main(String[] args) throws InterruptedException
    {

	Thread.sleep(30000);
	final int i = 42;
	printMessage(i);

    }
}
