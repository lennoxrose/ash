let nums = [1, 2, 3, 4, 5];
print nums;
print nums[0];
print nums[4];

nums[0] = 99;
print nums;

let sum = 0;
let i = 0;
while (i < 5) {
    let sum = sum + nums[i];
    let i = i + 1;
}
print sum;

fn first(arr) {
    return arr[0];
}
print first(nums);

let names = ["Ash", "Sonnet"];
print names[1];
