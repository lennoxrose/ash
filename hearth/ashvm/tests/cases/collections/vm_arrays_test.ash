local nums = [1, 2, 3, 4, 5];
say nums;
say nums[0];
say nums[4];

nums[0] = 99;
say nums;

local sum = 0;
local i = 0;
during (i < 5) {
    local sum = sum + nums[i];
    local i = i + 1;
}
say sum;

forge first(arr) {
    yield arr[0];
}
say first(nums);

local names = ["Ash", "Sonnet"];
say names[1];
