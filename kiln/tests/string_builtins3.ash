let parts = split("a,b,c", ",");
print len(parts);
print parts[0];
print parts[1];
print parts[2];

let chars = split("abc", "");
print len(chars);
print chars[0];
print chars[2];

let empty1 = split("", ",");
print len(empty1);
print empty1[0];

let empty2 = split("", "");
print len(empty2);

print join(["a", "b", "c"], "-");
print join(["x"], ",");
print join([], ",");

print replace("hello world", "world", "there");
print replace("aaa", "a", "bb");
print replace("hello", "xyz", "abc");
print replace("hello", "", "X");
print replace("banana", "a", "o");
