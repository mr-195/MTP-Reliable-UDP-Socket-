// Record the start time
let startTime = performance.now();

// Your code here
let arrayOfObjects = [
    { name: 'John', age: 30 },
    { name: 'Alice', age: 25 },
    { name: 'Bob', age: 35 },
    { name: 'John', age: 40 }, // Duplicate entry
    { name: 'Alice', age: 22 }, // Duplicate entry
    { name: 'John', age: 30 },
    { name: 'Alice', age: 25 },
    { name: 'Bob', age: 35 },
    { name: 'John', age: 40 }, // Duplicate entry
    { name: 'Alice', age: 22 }, // Duplicate entry
    { name: 'John', age: 30 },
    { name: 'Alice', age: 25 },
    { name: 'Bob', age: 35 },
    { name: 'John', age: 40 }, // Duplicate entry
    { name: 'Alice', age: 22 }, // Duplicate entry
    { name: 'John', age: 30 },
    { name: 'Alice', age: 25 },
    { name: 'Bob', age: 35 },
    { name: 'John', age: 40 }, // Duplicate entry
    { name: 'Alice', age: 22 }, // Duplicate entry
];

arrayOfObjects.sort((a, b) => a.age - b.age);

function removeDuplicatesByName(array) {
    let uniqueArray = [];
    let seenNames = {};
    for (let obj of array) {
        if (!seenNames[obj.name]) {
            uniqueArray.push(obj);
            seenNames[obj.name] = true;
        }
    }
    return uniqueArray;
}

let uniqueArrayOfObjects = removeDuplicatesByName(arrayOfObjects);
console.log(uniqueArrayOfObjects);
// Record the end time
let endTime = performance.now();

// Calculate and output the runtime
console.log("Runtime: " + (endTime - startTime) + " milliseconds");
