forge double(x) { yield x * 2; }
forge is_even(x) { yield x % 2 == 0; }

local nums = [1, 2, 3, 4, 5, 6];

local doubled = map(nums, double);
say doubled;

local evens = filter(nums, is_even);
say evens;

local total = reduce(nums, forge(acc, x) { yield acc + x; }, 0);
say total;

local squareLambda = forge(x) { yield x * x; };
local squares = map(nums, squareLambda);
say squares;

local f = double;
say f(21);
