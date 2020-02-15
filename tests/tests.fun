
fun is_equal(a, b) { return a==b; }

class ClassA {}
class ClassB {}
class ClassC {}
class ClassD {}
class ClassE {}


var instA1 = ClassA();
var instA2 = ClassA();
var instB1 = ClassB();
var instB2 = ClassB();

var i;
var flag;

var tests = [];


tests += [
  "Types",
  [null.type,     "null",       is_equal,   true],
  [true.type,     "boolean",    is_equal,   true],
  [false.type,    "boolean",    is_equal,   true],
  [0.type,        "number",     is_equal,   true],
  ["".type,       "string",     is_equal,   true],
  [is_equal.type, "function",   is_equal,   true],
  [ClassA.type,   "class",      is_equal,   true],
  [instA1.type,   "instance",   is_equal,   true]
];

tests += [
  "Booleans and Null",
  [true,		true,			is_equal,		true],
  [false,		false,		is_equal,		true],
  [true,		false,		is_equal,		false],
  [false,		true,		  is_equal,		false],
  [true,		null,			is_equal,		false],
  [false,		null,		  is_equal,		false],
  [null,		true,			is_equal,		false],
  [null,		false,	  is_equal,		false],
  [null,		null,	    is_equal,		true]
];

tests += [
  "Numbers",
  [0,       0,        is_equal,   true],
  [0.1,     0.1,      is_equal,   true],
  [1,       1,        is_equal,   true],
  [-1,      -1,       is_equal,   true],
  [-0.1,    -0.1,     is_equal,   true],
  [0,       0.1,      is_equal,   false],
  [0.1,     1,        is_equal,   false],
  [1,       -1,       is_equal,   false],
  [-1,      -0.1,     is_equal,   false],
  [-0.1,    0,        is_equal,   false],
  [0>1,     true,     is_equal,   false],
  [0>=1,    true,     is_equal,   false],
  [0<1,     true,     is_equal,   true],
  [0<=1,    true,     is_equal,   true],
  [1>0,     true,     is_equal,   true],
  [1>=0,    true,     is_equal,   true],
  [1<0,     true,     is_equal,   false],
  [1<=0,    true,     is_equal,   false]
];

tests += [
  "Binary",
  [0b0000,  0,        is_equal,   true],
  [0b0001,  1,        is_equal,   true],
  [0b0010,  2,        is_equal,   true],
  [0b0100,  4,        is_equal,   true],
  [0b1000,  8,        is_equal,   true],
  [0b1111,  15,       is_equal,   true]
];

tests += [
  "Octal",
  [00000,   0,        is_equal,   true],
  [00001,   1,        is_equal,   true],
  [00100,   64,       is_equal,   true],
  [00644,   420,      is_equal,   true],
  [00755,   493,      is_equal,   true],
  [01000,   512,      is_equal,   true]
];

tests += [
  "Hexadecimal",
  [0x0000,  0,        is_equal,   true],
  [0x00ff,  255,      is_equal,   true],
  [0x1000,  4096,     is_equal,   true],
  [0x2000,  8192,     is_equal,   true],
  [0x4000,  16384,    is_equal,   true],
  [0x8000,  32768,    is_equal,   true],
  [0xffff,  65535,    is_equal,   true]
  // Note: compiler.c uses strtol() which uses "long" -> max 31 bits
  // Internally, FunC uses "double" -> max 56 bits without loss of precision
];

tests += [
  "Strings",
  ["",      "",       is_equal,   true],
  ["foo",   "foo",    is_equal,   true],
  ["bar",   "bar",    is_equal,   true],
  ["baz",   "baz",    is_equal,   true],
  ["",      "foo",    is_equal,   false],
  ["foo",   "bar",    is_equal,   false],
  ["bar",   "baz",    is_equal,   false],
  ["baz",   "",       is_equal,   false]
];

tests += [
  "Objects",
  [ClassA,  ClassA,   is_equal,   true],
  [ClassB,  ClassB,   is_equal,   true],
  [ClassA,  ClassB,   is_equal,   false],
  [ClassB,  ClassA,   is_equal,   false]
];

tests += [
  "Instances",
  [instA1,  instA1,   is_equal,   true],
  [instA2,  instA2,   is_equal,   true],
  [instB1,  instB1,   is_equal,   true],
  [instB2,  instB2,   is_equal,   true],
  [instA1,  instB2,   is_equal,   false],
  [instA2,  instA1,   is_equal,   false],
  [instB1,  instA2,   is_equal,   false],
  [instB2,  instB1,   is_equal,   false]
];

tests += [
  "Instance properties",
  [instA1.a1=null,    null,   is_equal,   true],
  [instA1.a2=true,    true,   is_equal,   true],
  [instA1.a3=false,   false,  is_equal,   true],
  [instA1.a4=0,       0,      is_equal,   true],
  [instA1.a5="foo",   "foo",  is_equal,   true],
  [instA1.a1,         null,   is_equal,   true],
  [instA1.a2,         true,   is_equal,   true],
  [instA1.a3,         false,  is_equal,   true],
  [instA1.a4,         0,      is_equal,   true],
  [instA1.a5,         "foo",  is_equal,   true]
];

tests += [
  "Unary operators",
  [!false,            true,   is_equal,   true],
  [!true,             false,  is_equal,   true],
  [-1,                1-2,    is_equal,   true],
  [-0,                0,      is_equal,   true],
  [~0,                4294967295, is_equal, true],
  [i=1,               1,      is_equal,   true],
  [i++,               1,      is_equal,   true],
  [i++,               2,      is_equal,   true],
  [i,                 3,      is_equal,   true],
  [i--,               3,      is_equal,   true],
  [i--,               2,      is_equal,   true],
  [i,                 1,      is_equal,   true]
];

tests += [
  "Binary operators",
  [0x1002|0x2001,     0x3003, is_equal,   true],
  [0x1002&0x3003,     0x1002, is_equal,   true],
  [0x2001|0x1002,     0x3003, is_equal,   true],
  [0x3003&0x1002,     0x1002, is_equal,   true],
  [0x1002^0x3003,     0x2001, is_equal,   true],
  [0x3003^0x1002,     0x2001, is_equal,   true],
  [0x0102<<0,         0x0102, is_equal,   true],
  [0x0102<<4,         0x1020, is_equal,   true],
  [0x0102>>0,         0x0102, is_equal,   true],
  [0x0102>>4,         0x0010, is_equal,   true]
];

tests += [
  "Arithmetic operators",
  [0+0,               0,      is_equal,   true],
  [0+1,               1,      is_equal,   true],
  [1+0,               1,      is_equal,   true],
  [1+1,               2,      is_equal,   true],
  [2+2,               4,      is_equal,   true],
  [4+4,               8,      is_equal,   true],
  [65536+65536,       131072, is_equal,   true],
  [0-0,               0,      is_equal,   true],
  [0-1,               -1,     is_equal,   true],
  [1-0,               1,      is_equal,   true],
  [1-1,               0,      is_equal,   true],
  [2-2,               0,      is_equal,   true],
  [4-4,               0,      is_equal,   true],
  [65536-65536,       0,      is_equal,   true],
  [0*0,               0,      is_equal,   true],
  [0*1,               0,      is_equal,   true],
  [1*0,               0,      is_equal,   true],
  [1*1,               1,      is_equal,   true],
  [2*2,               4,      is_equal,   true],
  [4*4,               16,     is_equal,   true],
  [0/2,               0,      is_equal,   true],
  [10/4,              2.5,    is_equal,   true],
  [100/10,            10,     is_equal,   true],
  [10/100,            0.1,    is_equal,   true],
  [4/16,              0.25,   is_equal,   true],
  [10/5,              2,      is_equal,   true],
  [10/20,             0.5,    is_equal,   true],
  [1000/200,          5,      is_equal,   true]
];


tests += [
  "String methods",
  ["".bytes,          0,      is_equal,   true],
  ["".chars,          0,      is_equal,   true],
  ["foobar".bytes,    6,      is_equal,   true],
  ["foobar".chars,    6,      is_equal,   true],
  ["foobar".byte_at(0), "f",  is_equal,   true],
  ["foobar".byte_at(1), "o",  is_equal,   true],
  ["foobar".byte_at(2), "o",  is_equal,   true],
  ["foobar".byte_at(3), "b",  is_equal,   true],
  ["foobar".byte_at(4), "a",  is_equal,   true],
  ["foobar".byte_at(5), "r",  is_equal,   true],
  ["foobar".char_at(0), "f",  is_equal,   true],
  ["foobar".char_at(1), "o",  is_equal,   true],
  ["foobar".char_at(2), "o",  is_equal,   true],
  ["foobar".char_at(3), "b",  is_equal,   true],
  ["foobar".char_at(4), "a",  is_equal,   true],
  ["foobar".char_at(5), "r",  is_equal,   true],
  ["foobar".bytes_at(0,3),  "foo",  is_equal,   true],
  ["foobar".bytes_at(3,3),  "bar",  is_equal,   true],
  ["foobar".bytes_at(-3,3), "bar",  is_equal,   true],
  ["foobar".bytes_at(-6,3), "foo",  is_equal,   true],
  ["foobar".substr(0,3),  "foo",  is_equal,   true],
  ["foobar".substr(3,3),  "bar",  is_equal,   true],
  ["foobar".substr(-3,3), "bar",  is_equal,   true],
  ["foobar".substr(-6,3), "foo",  is_equal,   true]
  // TODO: add utf8 testing here to show the purpose of .char vs .byte methods
];

tests += [
  "Number methods",
  [0.base(2),         "0",    is_equal,   true],
  [0.base(8),         "0",    is_equal,   true],
  [0.base(10),        "0",    is_equal,   true],
  [0.base(16),        "0",    is_equal,   true],
  [10.base(2),        "1010", is_equal,   true],
  [10.base(8),        "12",   is_equal,   true],
  [10.base(10),       "10",   is_equal,   true],
  [10.base(16),       "a",    is_equal,   true]
];

var log = "";

while(true) {

  var test_no = 0;
  var failed = 0;
  var ok = 0;
  var total = 0;


  for (var i=0; i < tests.length; i++) {
    var test = tests[i];
    test_no++;

    if (test.type == "string") {
      if (failed) { break; }
      debug("==== " + test + " ====");
      test_no = 0;
    } else {
      total++;
      if (test[2](test[0], test[1]) == test[3]) {
        debug("test " + test_no.base(10) + "...ok");
        ok++;
      } else {
        debug("test " + test_no.base(10) + "...FAILED");
        failed++;
      }
    }
  }
  debug("");
  if (failed) { debug("*** " + failed.base(10) + " TESTS FAILED ***"); }
  debug("Total:" + total.base(10) + " ok:" + ok.base(10));
  log += "Run completed.\n";
}

