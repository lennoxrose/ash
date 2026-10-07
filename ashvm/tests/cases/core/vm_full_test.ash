let person = {"name": "Ash", "version": 2};
print person;
print person["name"];
person["version"] = 3;
print person;
print keys(person);
print values(person);
print has(person, "name");
delete(person, "version");
print person;

fn double(x) { return x * 2; }
let f = double;
print f(21);

let nums = [1, 2, 3, 4, 5];
let doubled = map(nums, double);
print doubled;

fn isEven(x) { return x % 2 == 0; }
let evens = filter(nums, isEven);
print evens;

fn add(a, b) { return a + b; }
let total = reduce(nums, add, 0);
print total;

print upper("hello");
print lower("WORLD");
print trim("  spaced  ");
print split("a,b,c", ",");
print join(["x", "y", "z"], "-");
print substring("hello world", 0, 5);
print indexOf("hello world", "world");
print replace("foo bar foo", "foo", "baz");

write_file("/tmp/ashvm_test.txt", "vm file test\n");
print file_exists("/tmp/ashvm_test.txt");
print read_file("/tmp/ashvm_test.txt");
