local person = {"name": "Ash", "version": 2};
say person;
say person["name"];
person["version"] = 3;
say person;
say keys(person);
say values(person);
say has(person, "name");
delete(person, "version");
say person;

forge double(x) { yield x * 2; }
local f = double;
say f(21);

local nums = [1, 2, 3, 4, 5];
local doubled = map(nums, double);
say doubled;

forge isEven(x) { yield x % 2 == 0; }
local evens = filter(nums, isEven);
say evens;

forge add(a, b) { yield a + b; }
local total = reduce(nums, add, 0);
say total;

say upper("hello");
say lower("WORLD");
say trim("  spaced  ");
say split("a,b,c", ",");
say join(["x", "y", "z"], "-");
say substring("hello world", 0, 5);
say indexOf("hello world", "world");
say replace("foo bar foo", "foo", "baz");

write_file("/tmp/ashvm_test.txt", "vm file test\n");
say file_exists("/tmp/ashvm_test.txt");
say read_file("/tmp/ashvm_test.txt");
