package ash.io;

import ash.util.List;

/// Whole-file operations. Failures are reported as IOException.
public final class Files {
    @Lower("file_exists($1)") private static native bool existsRaw(String path);
    @Lower("read_file($1)") private static native String readRaw(String path);
    @Lower("write_file($1, $2)") private static native void writeRaw(String path, String text);
    @Lower("append_file($1, $2)") private static native void appendRaw(String path, String text);
    @Lower("delete_file($1)") private static native void deleteRaw(String path);
    @Lower("rename_file($1, $2)") private static native void moveRaw(String from, String to);
    @Lower("make_dir($1)") private static native void mkdirRaw(String path);
    @Lower("list_dir($1)") private static native List<String> listRaw(String path);

    /// True when the file or directory exists.
    public static bool exists(Path path) {
        return existsRaw(path.value());
    }

    /// Whole file contents as text; throws IOException if missing.
    public static String readString(Path path) throws IOException {
        require(path);
        return readRaw(path.value());
    }

    /// Replaces the file contents; throws IOException on failure.
    public static void writeString(Path path, String text) throws IOException {
        writeRaw(path.value(), text);
    }

    /// Appends text to the file.
    public static void appendString(Path path, String text) throws IOException {
        appendRaw(path.value(), text);
    }

    /// Deletes the file; throws IOException if missing.
    public static void delete(Path path) throws IOException {
        require(path);
        deleteRaw(path.value());
    }

    /// Renames or moves a file.
    public static void move(Path from, Path to) throws IOException {
        require(from);
        moveRaw(from.value(), to.value());
    }

    /// Creates a directory.
    public static void createDirectory(Path path) throws IOException {
        mkdirRaw(path.value());
    }

    /// Names of the directory entries.
    public static List<String> list(Path dir) throws IOException {
        require(dir);
        return listRaw(dir.value());
    }

    private static void require(Path path) throws IOException {
        if (!existsRaw(path.value())) {
            throw new IOException("no such file or directory: " + path.value());
        }
    }
}
