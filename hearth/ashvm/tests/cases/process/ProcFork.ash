package process;

/// sys_fork/sys_wait/sys_exec: the child execs /bin/echo, the parent waits for it.
class ProcFork {
    @Lower("sys_fork()") private static native int fork();
    @Lower("sys_wait($1)") private static native int await(int pid);
    @Lower("sys_exec($1, $2)") private static native int exec(String path, String arg);

    public static void main(String[] args) {
        int pid = fork();
        if (pid == 0) {
            exec("/bin/echo", "child ran");
            System.out.println("exec failed");
            return;
        }
        int done = await(pid);
        System.out.println(done == pid);
    }
}
