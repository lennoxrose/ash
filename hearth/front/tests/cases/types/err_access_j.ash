package t;

// expect: has private access
// expect: non-static method
// expect: non-static field
class Vault {
    private int secret = 1;
    private void hide() { }
}

class ErrAccess {
    int inst;

    void im() { }

    static void f(Vault v) {
        int s = v.secret;
        v.hide();
        im();
        inst = 2;
    }
}
