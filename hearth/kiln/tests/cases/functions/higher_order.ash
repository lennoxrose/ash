local nums = [1, 2, 3, 4, 5];

forge double(x) { yield x * 2; }
local doubled = map(nums, double);
say doubled[0];
say doubled[1];
say doubled[4];
say len(doubled);

forge is_even(x) { yield x - floor(x / 2) * 2 == 0; }
local evens = filter(nums, is_even);
say len(evens);
say evens[0];
say evens[1];

forge add(a, b) { yield a + b; }
say reduce(nums, add, 0);
say reduce(nums, add, 100);

local squares = map(nums, forge(x) { yield x * x; });
say squares[2];
say squares[4];

local empty = [];
local mapped_empty = map(empty, double);
say len(mapped_empty);
