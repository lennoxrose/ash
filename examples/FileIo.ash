package examples;

import ash.io.Files;
import ash.io.Path;
import ash.util.List;

public class FileIo {
    public static void main(String[] args) {
        Path dir = Path.of("/tmp/ash_example_io");
        Path file = dir.resolve("notes.txt");
        try {
            if (!Files.exists(dir)) {
                Files.createDirectory(dir);
            }
            Files.writeString(file, "alpha\n");
            Files.appendString(file, "beta\n");
            String text = Files.readString(file);
            System.out.println(text.split("\n").length + " lines, " + text.length() + " chars");
            List<String> entries = Files.list(dir);
            System.out.println(entries.size() + " entry: " + entries.get(0));
            Files.delete(file);
            Files.readString(file);
        } catch (IOException e) {
            System.out.println("failed: " + e.getMessage());
        }
    }
}
