local person = {"name": "Lennox", "age": 20};
say person;
say person["name"];
say person["age"];

person["age"] = 21;
say person["age"];

person["city"] = "Luebeck";
say person;

say keys(person);
say values(person);
say has(person, "name");
say has(person, "zip");

delete(person, "city");
say person;

local empty = {};
say empty;
say len(person);
