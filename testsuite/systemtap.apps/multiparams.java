class multiparams
{
    public static void printMessage10(Object a, String b, short c, char d, boolean e, byte f, float g, double h, long i, int j) { }
    public String toString() { return "just a multiparams"; }
        
    public static void main(String[] args) throws InterruptedException
    {
        System.out.println("multiparams test started, waiting");
	Thread.sleep(20000);
        System.out.println("multiparams test function started");
	final int i = 42;
	final long j = 254775806;
	final double k = 3.14159265359;
	final float l = 2.71828f;
	final byte n = 10;
	final boolean o = true;
	final char p = 'a';
	final short q = 14;
        final String r = "hello";
        final Object s = new multiparams();
	printMessage10(s, r, q, p, o, n, l, k, j, i);
 	final Object nothing = null;
	printMessage10(nothing, r, q, p, o, n, l, k, j, i);
        System.out.println("multiparams test function completed");
    }
}
