let person = {"name": "Lennox", "age": 20};
print person;
print person["name"];
print person["age"];

person["age"] = 21;
print person["age"];

person["city"] = "Luebeck";
print person;

print keys(person);
print values(person);
print has(person, "name");
print has(person, "zip");

delete(person, "city");
print person;

let empty = {};
print empty;
print len(person);
