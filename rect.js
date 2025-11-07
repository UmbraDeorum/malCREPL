// node rect.js | ./crepl libraylib.so
console.log("InitWindow 800 600 \"FFI\"");
console.log("SetTargetFPS 60");
let x = 0
let y = 0
while (true) {
    x += 1
    y += 1
    console.log("BeginDrawing");
    console.log("ClearBackground 0xFF000000");
    console.log(`DrawRectangle ${x} ${y} 100 100 0xFF0000FF`);
    console.log("EndDrawing");
}
