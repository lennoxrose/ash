package examples;

import ash.cli.Args;

public class CliTool {
    public static void main(String[] args) {
        Args parsed = new Args(new String[]{"--name", "world", "--shout", "extra"});
        String name = parsed.get("name", "nobody");
        String message = "hello " + name;
        if (parsed.has("shout")) {
            message = message.toUpperCase();
        }
        System.out.println(message + " " + parsed.positional().size());
    }
}
